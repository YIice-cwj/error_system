/**
 * @file demo06.cc
 * @brief 高级用法示例：模式匹配 / JSON 往返 / 二进制因果链 / 自定义格式化器 / 重复策略
 * @details 本示例演示 error_system 的高级特性，覆盖现有 demo01~demo05 未涉及的内容：
 *          1. result_t::match() 模式匹配
 *          2. error_context 的 JSON 序列化与反序列化往返
 *          3. 二进制序列化与因果链（cause chain）还原
 *          4. 自定义格式化器（custom formatter）
 *          5. 重复错误码注册策略（duplicate policy）
 *          6. 一个端到端的"订单服务"综合场景
 * @author yiice
 * @version 2.4.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */

#include <atomic>
#include <iostream>
#include <string>
#include <unordered_map>

#include "error_system.h"
#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"
#include "error_system/plugin/plugin_registry.h"
#include "error_system/utils/json_utils.h"
// IWYU pragma: begin_exports
#include "payment_service_errors.h"
#include "redis_component_errors.h"
#include "trade_service_errors.h"
#include "user_service_errors.h"
// IWYU pragma: end_exports

using namespace error_system::core;
using namespace error_system::config;
using namespace error_system::domain;
using error_system::plugin::i_error_plugin_t;
using error_system::plugin::plugin_registry_t;
using error_system::utils::json_dict_t;

// ========== 辅助函数：打印分隔标题 ==========
namespace {
    void print_section(const std::string& title) {
        std::cout << "\n--- " << title << " ---" << std::endl;
    }
}  // namespace

// ========== 自定义统计插件 ==========
//
// 继承 i_error_plugin_t 实现 on_error()，统计各错误码出现次数。
// 通过 min_level() 过滤只关注 error 及以上级别。
// 注意：on_error 必须 noexcept，内部使用 std::atomic 保证线程安全。

class stats_plugin_t : public i_error_plugin_t {
private:
    std::unordered_map<uint64_t, std::atomic<int>> counters_;

public:
    std::string_view name() const noexcept override {
        return "stats";
    }

    error_level_t min_level() const noexcept override {
        return error_level_t::error;  // 只统计 Error 及更严重级别
    }

    void on_error(const error_context_t& context) noexcept override {
        ++counters_[context.get_code().get_code()];
    }

    int total_count() const noexcept {
        int sum = 0;
        for (const auto& [_, cnt] : counters_) {
            sum += cnt.load();
        }
        return sum;
    }
};

// ========== 1. match() 模式匹配示例 ==========
//
// match() 接受两个回调：成功分支与错误分支，返回类型必须一致。
// 与 if-else 检查 is_error() 相比，match() 强制调用方同时处理两条路径，
// 避免遗漏错误分支。注意：match() 不再标记 noexcept，回调抛出的异常会传播。

static void demo_match() {
    std::cout << "===== Demo 6.1: match() 模式匹配 =====" << std::endl;

    // 场景：模拟一个用户登录函数，返回 result_t<std::string>（token）或错误
    auto login = [](bool success) -> result_t<std::string> {
        if (success) {
            return result_t<std::string>::make_success("token-abc-123");
        }
        return result_t<std::string>::make_error(
            biz::user_errors::ERR_TOKEN_EXPIRED, "用户凭证已过期");
    };

    // 成功路径：match 自动选择 success_fn
    print_section("1.1 成功路径");
    auto ok_result = login(true);
    std::string message = ok_result.match(
        [](const std::string& token) {
            return "登录成功，token=" + token;
        },
        [](const error_context_t& err) {
            return "登录失败：" + std::string(err.message);
        });
    std::cout << "  " << message << std::endl;

    // 错误路径：match 自动选择 error_fn
    print_section("1.2 错误路径");
    auto err_result = login(false);
    message = err_result.match(
        [](const std::string& token) {
            return "登录成功，token=" + token;
        },
        [](const error_context_t& err) {
            // 在错误分支中可以访问完整的 error_context_t
            return "登录失败 [code=0x" +
                   std::to_string(err.get_code().get_identity_code()) +
                   "]: " + std::string(err.message);
        });
    std::cout << "  " << message << std::endl;
}

// ========== 2. JSON 序列化与反序列化往返 ==========
//
// error_context_serializer_t::to_json() 将错误上下文序列化为 JSON 字符串，
// json_dict_t::parse() 可解析该字符串并通过键路径访问字段。
// 这是跨进程 / 跨语言传递错误的常见方式。

static void demo_json_roundtrip() {
    std::cout << "\n===== Demo 6.2: JSON 序列化与反序列化往返 =====" << std::endl;

    // 临时关闭 stacktrace，使 JSON 输出不含 stack_frames 数组。
    const bool was_stacktrace_enabled = feature_flags_t::is_stacktrace_enabled();
    feature_flags_t::set_enable_stacktrace(false);

    // 构造一个带 payload 的错误上下文
    error_context_t ctx{
        biz::trade_errors::ERR_ORDER_NOT_FOUND,
        "订单查询失败"};
    ctx.with("user_id", "8848")
        .with("order_id", "ORD-20260627-0001")
        .with("retry_count", 3);

    // 序列化为 JSON
    print_section("2.1 序列化为 JSON（error_context_serializer_t::to_json）");
    const std::string json_str = error_context_serializer_t::to_json(ctx);
    std::cout << "  " << json_str << std::endl;

    // 反序列化演示
    // 注意：json_dict_t 是简化的 JSON 解析器，仅支持扁平 / 嵌套的字符串键值对，
    //       不支持数字、布尔、数组等非字符串值。error_context_serializer_t::to_json()
    //       输出的 code 字段是数字，因此无法被 json_dict_t 直接解析。
    //       生产环境如需完整 JSON 解析请使用 nlohmann/json 等第三方库。
    //       这里用一个纯字符串 JSON 演示 json_dict_t 的键路径访问能力。
    print_section("2.2 反序列化并访问字段（json_dict_t）");
    const std::string simple_json = R"({
        "service": "trade_service",
        "error": {
            "name": "ERR_ORDER_NOT_FOUND",
            "message": "订单查询失败",
            "payload": {
                "user_id": "8848",
                "order_id": "ORD-20260627-0001",
                "retry_count": "3"
            }
        }
    })";
    auto parsed = json_dict_t::parse(simple_json);
    if (parsed.has_value()) {
        // 顶层字段
        std::cout << "  service: " << parsed->get_value("service").value_or("N/A") << std::endl;
        // 嵌套对象通过 "parent.child" 路径访问
        std::cout << "  error.name: "
                  << parsed->get_value("error.name").value_or("N/A") << std::endl;
        std::cout << "  error.message: "
                  << parsed->get_value("error.message").value_or("N/A") << std::endl;
        // 三层嵌套：error.payload.user_id
        std::cout << "  error.payload.user_id: "
                  << parsed->get_value("error.payload.user_id").value_or("N/A") << std::endl;
        std::cout << "  error.payload.order_id: "
                  << parsed->get_value("error.payload.order_id").value_or("N/A") << std::endl;
        std::cout << "  error.payload.retry_count: "
                  << parsed->get_value("error.payload.retry_count").value_or("N/A") << std::endl;
    } else {
        std::cout << "  JSON 解析失败" << std::endl;
    }

    // 恢复 stacktrace 设置
    feature_flags_t::set_enable_stacktrace(was_stacktrace_enabled);
}

// ========== 3. 二进制序列化与因果链 ==========
//
// to_binary() 生成紧凑的二进制表示，适合日志收集 / 跨网络传输。
// 因果链（cause chain）通过 wrap() 构建，二进制中会递归序列化整条链。

static void demo_binary_with_cause_chain() {
    std::cout << "\n===== Demo 6.3: 二进制序列化与因果链 =====" << std::endl;

    // 构造三层因果链：用户下单失败 → 支付服务不可用 → Redis 连接超时
    print_section("3.1 构造三层因果链");
    error_context_t root_cause{
        infra::redis_errors::ERR_KEY_NOT_FOUND,
        "Redis 键不存在：session:user:8848"};
    root_cause.with("redis_cmd", "GET").with("redis_key", "session:user:8848");

    error_context_t mid_layer{
        biz::payment_errors::ERR_ACCOUNT_FROZEN,
        "支付服务调用失败"};
    mid_layer.with("payment_channel", "alipay");

    // 用 wrap() 将 root_cause 作为 mid_layer 的原因
    auto wrapped = mid_layer.wrap(root_cause);

    error_context_t top_layer{
        biz::trade_errors::ERR_ORDER_NOT_FOUND,
        "订单创建失败：支付环节异常"};
    // 再包一层，形成三层链
    auto full_chain = top_layer.wrap(wrapped);

    // 文本输出会递归展示因果链（↳ Caused by:）
    std::cout << error_context_serializer_t::to_string(full_chain) << std::endl;

    // 二进制序列化：整条链都被编码
    print_section("3.2 二进制序列化大小对比");
    const std::string binary_no_cause = error_context_serializer_t::to_binary(top_layer);
    const std::string binary_full_chain = error_context_serializer_t::to_binary(full_chain);
    std::cout << "  单层错误二进制大小: " << binary_no_cause.size() << " bytes" << std::endl;
    std::cout << "  三层因果链二进制大小: " << binary_full_chain.size() << " bytes" << std::endl;
    std::cout << "  因果链额外开销: "
              << (binary_full_chain.size() - binary_no_cause.size()) << " bytes" << std::endl;
}

// ========== 4. 自定义格式化器 ==========
//
// formatter_config_t::set_custom_formatter() 允许注册全局自定义格式化器，
// 替换默认的文本输出。适合接入公司内部日志格式（如 JSON Lines / logfmt）。

static void demo_custom_formatter() {
    std::cout << "\n===== Demo 6.4: 自定义格式化器 =====" << std::endl;

    error_context_t ctx{
        biz::trade_errors::ERR_ORDER_NOT_FOUND,
        "自定义格式化器演示"};
    ctx.with("order_id", "ORD-001");

    // 默认输出
    print_section("4.1 默认格式化器");
    std::cout << "  " << error_context_serializer_t::to_string(ctx) << std::endl;

    // 注册自定义格式化器：输出 logfmt 风格
    print_section("4.2 自定义 logfmt 格式化器");
    formatter_config_t::set_custom_formatter([](const error_context_t& e) -> std::string {
        std::string out = "level=";
        out += to_string(e.get_code().get_level());
        out += " msg=\"";
        out += e.message;
        out += "\" code=";
        out += std::to_string(e.get_code().get_identity_code());
        e.for_each_payload([&](const std::string& k, const std::string& v) {
            out += " ";
            out += k;
            out += "=";
            out += v;
        });
        return out;
    });

    std::cout << "  " << error_context_serializer_t::to_string(ctx) << std::endl;

    // 清除自定义格式化器，恢复默认
    formatter_config_t::set_custom_formatter(nullptr);
    print_section("4.3 恢复默认格式化器");
    std::cout << "  " << error_context_serializer_t::to_string(ctx) << std::endl;
}

// ========== 5. 重复错误码注册策略 ==========
//
// 当同一错误码被注册多次时，duplicate_policy_handler_t 负责按策略处理：
//   - skip：忽略后续注册（默认）
//   - overwrite：用新元数据覆盖旧的
//   - warn：调用警告回调，保留原有元数据
// 这里演示 warn 策略的用法。

static void demo_duplicate_policy() {
    std::cout << "\n===== Demo 6.5: 重复错误码注册策略 =====" << std::endl;

    auto& registry = error_registry_t::instance();
    // 清空以确保演示干净
    registry.unregister_all();

    const auto code = error_code_t{
        error_level_t::error, system_domain_t::application, 200, 1, 5001};

    // 设置 warn 策略：重复注册时触发回调
    print_section("5.1 设置 warn 策略");
    registry.set_duplicate_policy(duplicate_policy_t::warn);
    registry.set_duplicate_warn_callback(
        [](code_t raw_code, const error_metadata_t& existing) {
            std::cout << "  [WARN] 错误码 0x" << std::hex << raw_code << std::dec
                      << " 已注册为 " << existing.name << "，重复注册被忽略" << std::endl;
        });

    // 首次注册
    print_section("5.2 首次注册");
    registry.register_error(code, "ERR_FIRST_REGISTRATION", "第一次注册的描述");
    if (auto info = registry.get_info(code); info.has_value()) {
        std::cout << "  当前: name=" << info->name
                  << ", desc=" << info->description << std::endl;
    }

    // 重复注册：应触发 warn 回调，保留原有元数据
    print_section("5.3 重复注册（触发 warn）");
    registry.register_error(code, "ERR_SECOND_REGISTRATION", "第二次注册的描述");
    if (auto info = registry.get_info(code); info.has_value()) {
        std::cout << "  当前: name=" << info->name
                  << ", desc=" << info->description << " (保持不变)" << std::endl;
    }

    // 切换为 overwrite 策略
    print_section("5.4 切换为 overwrite 策略");
    registry.set_duplicate_policy(duplicate_policy_t::overwrite);
    registry.register_error(code, "ERR_OVERWRITTEN", "覆盖后的描述");
    if (auto info = registry.get_info(code); info.has_value()) {
        std::cout << "  当前: name=" << info->name
                  << ", desc=" << info->description << " (已被覆盖)" << std::endl;
    }

    // 恢复默认策略
    registry.set_duplicate_policy(duplicate_policy_t::skip);
    registry.set_duplicate_warn_callback(nullptr);
    registry.unregister_all();
}

// ========== 6. 端到端综合场景：订单服务 ==========
//
// 综合演示一个真实的"创建订单"流程，展示：
//   - 用 result_t 串联多个步骤
//   - 错误发生时携带 payload（订单号、用户ID）
//   - 因果链记录根因
//   - 通过插件系统异步通知监控系统

static void demo_end_to_end() {
    std::cout << "\n===== Demo 6.6: 端到端综合场景 =====" << std::endl;

    auto& plugin_registry = plugin_registry_t::instance();
    // 注意：不调用 error_registry.unregister_all()，因为自动生成的错误码
    //       在静态初始化时注册，unregister 后不会自动恢复。

    // 注册一个简单的统计插件：累计错误次数
    print_section("6.1 注册错误统计插件");
    stats_plugin_t stats;
    plugin_registry.register_plugin_ref(stats);
    std::cout << "  已注册统计插件，当前错误数: " << stats.total_count() << std::endl;

    // 模拟订单创建流程
    print_section("6.2 模拟订单创建流程");

    // 步骤 1：校验用户
    auto validate_user = [](int user_id) -> result_t<void> {
        if (user_id <= 0) {
            return result_t<void>::make_error(
                biz::user_errors::ERR_TOKEN_EXPIRED, "无效的用户ID");
        }
        return result_t<void>::make_success();
    };

    // 步骤 2：扣减库存
    auto deduct_stock = [](int /*product_id*/, int qty) -> result_t<void> {
        if (qty <= 0) {
            return result_t<void>::make_error(
                biz::trade_errors::ERR_ORDER_NOT_FOUND, "库存扣减失败：数量必须大于0");
        }
        return result_t<void>::make_success();
    };

    // 步骤 3：支付
    auto process_payment = [](int amount) -> result_t<void> {
        if (amount <= 0) {
            // 支付失败时记录根因
            error_context_t root{
                infra::redis_errors::ERR_KEY_NOT_FOUND,
                "支付通道签名缓存丢失"};
            error_context_t wrapper{
                biz::payment_errors::ERR_ACCOUNT_FROZEN,
                "支付处理失败"};
            return result_t<void>::make_error(wrapper.wrap(root));
        }
        return result_t<void>::make_success();
    };

    // 端到端编排：用 and_then 串联，任一步失败则短路
    auto create_order = [&](int user_id, int product_id, int qty, int amount) -> result_t<void> {
        return validate_user(user_id)
            .and_then([&](void) { return deduct_stock(product_id, qty); })
            .and_then([&](void) { return process_payment(amount); });
    };

    // 场景 A：全部成功
    print_section("6.3 场景A：全部成功");
    auto result_a = create_order(8848, 1001, 2, 199);
    std::cout << "  结果: " << (result_a.is_success() ? "成功" : "失败") << std::endl;

    // 场景 B：支付失败（带因果链）
    print_section("6.4 场景B：支付失败");
    auto result_b = create_order(8848, 1001, 2, 0);
    if (result_b.is_error()) {
        std::cout << "  " << error_context_serializer_t::to_string(result_b.error()) << std::endl;
    }

    // 场景 C：用户校验失败
    print_section("6.5 场景C：用户校验失败");
    auto result_c = create_order(-1, 1001, 2, 199);
    if (result_c.is_error()) {
        std::cout << "  " << error_context_serializer_t::to_string(result_c.error()) << std::endl;
    }

    // 查看插件统计
    // 注意：stats_plugin_t::min_level() 返回 error_level_t::error，因此：
    //   - 场景B 的 ERR_ACCOUNT_FROZEN（fatal >= error）→ 被统计
    //   - 场景C 的 ERR_TOKEN_EXPIRED（info < error）→ 被过滤
    // 故总错误数为 1。
    print_section("6.6 插件统计结果");
    std::cout << "  总错误数: " << stats.total_count() << " (应为 1：fatal 统计，info 被过滤)" << std::endl;

    // 清理
    plugin_registry.unregister_plugin("stats");
}

int main() {
    demo_match();
    demo_json_roundtrip();
    demo_binary_with_cause_chain();
    demo_custom_formatter();
    // 注意：demo_end_to_end 必须在 demo_duplicate_policy 之前执行，
    // 因为 duplicate_policy 演示会调用 unregister_all() 清空注册表，
    // 而自动生成的错误码仅在静态初始化时注册一次，清空后无法恢复。
    demo_end_to_end();
    demo_duplicate_policy();

    std::cout << "\n===== Demo 6 结束 =====" << std::endl;
    return 0;
}
