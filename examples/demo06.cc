/**
 * @file demo06.cc
 * @brief 高级特性 API 全览：序列化往返 / 自定义格式化器 / join_errors / TRY 宏 / 端到端
 * @details 演示 error_context_serializer_t 的 JSON/二进制往返、formatter_config_t 自定义
 *          格式器、join_errors 聚合多个错误、ERROR_SYSTEM_TRY 早返回宏、以及一个综合
 *          的订单服务端到端场景。每个示例只做一件事，标题即 API 名。
 * @note 项目规范要求函数 noexcept，生产代码优先用 result_t；本示例仅演示 API 用法。
 */

#include <atomic>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "error_system.h"
#include "error_system/config/error_config.h"
#include "error_system/core/error_context_serializer.h"
#include "error_system/core/error_registry.h"
#include "error_system/plugin/plugin_registry.h"
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

namespace {

/** @brief 打印小节标题 */
void section(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

/** @brief 自定义统计插件：统计 error 及以上级别错误次数 */
class stats_plugin_t : public i_error_plugin_t {
    std::unordered_map<uint64_t, std::atomic<int>> counters_;

public:
    std::string_view name() const noexcept override { return "stats"; }

    error_level_t min_level() const noexcept override {
        return error_level_t::error;
    }

    void on_error(const error_context_t& context) noexcept override {
        ++counters_[context.get_code().get_code()];
    }

    int total() const noexcept {
        int sum = 0;
        for (const auto& [_, c] : counters_) { sum += c.load(); }
        return sum;
    }
};

/** @brief 模拟订单查询：成功返回金额，失败返回错误 */
result_t<int> query_order(int order_id) {
    if (order_id <= 0) {
        return result_t<int>::make_error(biz::trade_errors::ERR_ORDER_NOT_FOUND, "无效的订单ID");
    }
    if (order_id == 404) {
        return result_t<int>::make_error(biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单不存在");
    }
    return result_t<int>{order_id * 1000};
}

}  // namespace

int main() {
    std::cout << "===== Demo 6: 高级特性 API 全览 =====" << std::endl;

    // ============================================================
    // 一、序列化往返：to_json / from_json / to_binary / from_binary
    // ============================================================
    section("1.1 to_json 序列化为 JSON");
    error_context_t ctx1{biz::trade_errors::ERR_ORDER_NOT_FOUND, "JSON 往返测试"};
    ctx1.with("user_id", "8848").with("retry_count", 3);
    const std::string json_str = ctx1.to_json();
    std::cout << "  " << json_str << std::endl;

    section("1.2 from_json 反序列化（error_context_serializer_t::from_json）");
    auto from_json = error_context_serializer_t::from_json(json_str);
    if (from_json.has_value()) {
        std::cout << "  code 还原    = " << from_json->get_code().get_code() << std::endl;
        std::cout << "  message 还原 = " << from_json->message << std::endl;
        std::cout << "  payload 项数 = " << from_json->payload_size() << std::endl;
        const bool same = from_json->get_code().get_code() == ctx1.get_code().get_code()
                          && from_json->message == ctx1.message;
        std::cout << "  往返一致性   = " << (same ? "通过" : "失败") << std::endl;
    }

    section("1.3 to_binary 序列化为紧凑二进制");
    const std::string binary_blob = ctx1.to_binary();
    std::cout << "  二进制大小 = " << binary_blob.size() << " bytes" << std::endl;

    section("1.4 from_binary 反序列化");
    auto from_binary = error_context_serializer_t::from_binary(binary_blob);
    if (from_binary.has_value()) {
        std::cout << "  code 还原    = " << from_binary->get_code().get_code() << std::endl;
        std::cout << "  message 还原 = " << from_binary->message << std::endl;
        const bool same = from_binary->get_code().get_code() == ctx1.get_code().get_code()
                          && from_binary->message == ctx1.message;
        std::cout << "  往返一致性   = " << (same ? "通过" : "失败") << std::endl;
    }

    section("1.5 from_json 处理非法 JSON（返回 nullopt）");
    auto bad = error_context_serializer_t::from_json("{not a json");
    std::cout << "  非法 JSON has_value = " << bad.has_value() << std::endl;

    section("1.6 from_binary 处理截断数据（返回 nullopt）");
    auto bad_bin = error_context_serializer_t::from_binary(std::string_view{"\x00\x01"});
    std::cout << "  截断数据 has_value = " << bad_bin.has_value() << std::endl;

    // ============================================================
    // 二、因果链序列化：wrap + to_binary
    // ============================================================
    section("2.1 构造三层因果链");
    error_context_t root{infra::redis_errors::ERR_KEY_NOT_FOUND, "Redis 键不存在"};
    root.with("redis_key", "session:user:8848");

    error_context_t mid{biz::payment_errors::ERR_ACCOUNT_FROZEN, "支付服务调用失败"};
    mid.with("payment_channel", "alipay");

    error_context_t top{biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单创建失败"};
    auto chain = top.wrap(mid.wrap(root));
    std::cout << chain.to_string() << std::endl;

    section("2.2 因果链二进制大小对比");
    std::cout << "  单层大小 = " << top.to_binary().size() << " bytes" << std::endl;
    std::cout << "  三层大小 = " << chain.to_binary().size() << " bytes" << std::endl;

    section("2.3 因果链反序列化还原");
    auto chain_restored = error_context_serializer_t::from_binary(chain.to_binary());
    if (chain_restored.has_value() && chain_restored->cause) {
        std::cout << "  顶层 message = " << chain_restored->message << std::endl;
        std::cout << "  中层 message = " << chain_restored->cause->message << std::endl;
        std::cout << "  底层 message = " << chain_restored->cause->cause->message << std::endl;
    }

    // ============================================================
    // 三、自定义格式化器：formatter_config_t
    // ============================================================
    section("3.1 set_custom_formatter 注册 logfmt 格式器");
    formatter_config_t::set_custom_formatter(
        [](const error_context_t& ctx) -> std::string {
            std::string out = "level=";
            out += to_string(ctx.get_code().get_level());
            out += " msg=\"";
            out += ctx.message;
            out += "\" code=";
            out += std::to_string(ctx.get_code().get_identity_code());
            ctx.for_each_payload([&out](const std::string& k, const std::string& v) {
                out += " ";
                out += k;
                out += "=";
                out += v;
            });
            return out;
        });
    // 注：error_context_t::to_string() 会优先调用自定义格式化器
    std::cout << "  " << ctx1.to_string() << std::endl;

    section("3.2 get_custom_formatter 获取当前格式化器");
    auto current_formatter = formatter_config_t::get_custom_formatter();
    std::cout << "  已设置 = " << (current_formatter ? 1 : 0) << std::endl;

    section("3.3 set_custom_formatter(nullptr) 恢复默认");
    formatter_config_t::set_custom_formatter(nullptr);
    std::cout << "  " << ctx1.to_string() << std::endl;

    // ============================================================
    // 四、join_errors：聚合多个错误
    // ============================================================
    section("4.1 join_errors 空列表（返回默认成功上下文）");
    std::vector<error_context_t> empty_errs;
    auto joined_empty = join_errors(std::move(empty_errs));
    std::cout << "  is_success = " << joined_empty.is_success() << std::endl;

    section("4.2 join_errors 单元素（直接返回，零开销）");
    std::vector<error_context_t> single_errs = {
        error_context_t{biz::trade_errors::ERR_ORDER_NOT_FOUND, "单条错误"},
    };
    auto joined_single = join_errors(std::move(single_errs));
    std::cout << "  message = " << joined_single.message << std::endl;
    std::cout << "  payload 项数 = " << joined_single.payload_size() << " (单元素不附加 payload)" << std::endl;

    section("4.3 join_errors 多元素（主错误 + joined_error_N payload）");
    std::vector<error_context_t> multi_errs = {
        error_context_t{biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单不存在"},
        error_context_t{biz::payment_errors::ERR_INSUFFICIENT_BALANCE, "余额不足"},
        error_context_t{biz::user_errors::ERR_TOKEN_EXPIRED, "Token 过期"},
    };
    auto joined = join_errors(std::move(multi_errs));
    std::cout << "  主错误 message = " << joined.message << std::endl;
    std::cout << "  聚合后 payload 项数 = " << joined.payload_size() << std::endl;
    joined.for_each_payload([](const std::string& k, const std::string& v) {
        std::cout << "    " << k << " = " << v << std::endl;
    });

    // ============================================================
    // 五、ERROR_SYSTEM_TRY 宏：早返回
    // ============================================================
    section("5.1 ERROR_SYSTEM_TRY 保留成功值并早返回错误");
    auto try_demo = [](int id) -> result_t<int> {
        ERROR_SYSTEM_TRY(amount, query_order(id));
        return result_t<int>{amount.value() * 2};
    };
    std::cout << "  成功: " << try_demo(123).value() << std::endl;
    std::cout << "  失败: " << try_demo(404).error().message << std::endl;

    section("5.2 ERROR_SYSTEM_TRY_DISCARD 丢弃成功值（仅传播错误）");
    auto try_discard = [](int id) -> result_t<int> {
        ERROR_SYSTEM_TRY_DISCARD(query_order(id));
        return result_t<int>{0};
    };
    std::cout << "  成功: " << try_discard(123).value() << " (丢弃查询值)" << std::endl;
    std::cout << "  失败: " << try_discard(404).error().message << std::endl;

    section("5.3 and_then 链式（跨类型：查订单 → 校验金额 → 支付）");
    auto validate_amount = [](int amount) -> result_t<int> {
        if (amount <= 0) {
            return result_t<int>::make_error(biz::payment_errors::ERR_INSUFFICIENT_BALANCE, "金额必须大于 0");
        }
        return result_t<int>{amount};
    };
    auto pay = [](int amount) -> result_t<std::string> {
        return result_t<std::string>::make_success("PAY_" + std::to_string(amount));
    };
    auto pipeline = [&](int order_id) -> result_t<std::string> {
        return query_order(order_id)
            .and_then(validate_amount)
            .and_then(pay);
    };
    std::cout << "  成功: " << pipeline(123).value() << std::endl;
    std::cout << "  失败: " << pipeline(404).error().message << std::endl;

    // ============================================================
    // 六、端到端综合场景：订单服务
    // ============================================================
    section("6.1 注册统计插件");
    auto& plugin_registry = plugin_registry_t::instance();
    stats_plugin_t stats;
    plugin_registry.register_plugin_ref(stats);
    std::cout << "  初始错误数 = " << stats.total() << std::endl;

    section("6.2 综合场景：用 and_then + context + wrap 串联订单流程");
    auto validate_user = [](int user_id) -> result_t<void> {
        if (user_id <= 0) {
            return result_t<void>::make_error(biz::user_errors::ERR_TOKEN_EXPIRED, "无效的用户ID");
        }
        return result_t<void>::make_success();
    };

    auto deduct_stock = [](int quantity) -> result_t<void> {
        if (quantity <= 0) {
            return result_t<void>::make_error(biz::trade_errors::ERR_ORDER_NOT_FOUND, "库存数量必须大于 0");
        }
        return result_t<void>::make_success();
    };

    auto process_payment = [](int amount) -> result_t<void> {
        if (amount <= 0) {
            // 构造因果链：根因 = Redis 缓存丢失，包装层 = 支付失败
            error_context_t root_cause{
                infra::redis_errors::ERR_KEY_NOT_FOUND,
                "支付通道签名缓存丢失"};
            error_context_t wrapper{
                biz::payment_errors::ERR_ACCOUNT_FROZEN,
                "支付处理失败"};
            return result_t<void>::make_error(wrapper.wrap(root_cause));
        }
        return result_t<void>::make_success();
    };

    auto create_order = [&](int user_id, int quantity, int amount) -> result_t<void> {
        return validate_user(user_id)
            .and_then([&](void) { return deduct_stock(quantity); })
            .and_then([&](void) { return process_payment(amount); })
            // 错误传播时附加上下文（context 方法）
            .context("user_id", std::to_string(user_id))
            .context("quantity", std::to_string(quantity));
    };

    section("6.3 场景A：全部成功");
    auto result_a = create_order(8848, 2, 199);
    std::cout << "  结果 = " << (result_a.is_success() ? "成功" : "失败") << std::endl;

    section("6.4 场景B：支付失败（带因果链 + context 附加）");
    auto result_b = create_order(8848, 2, 0);
    if (result_b.is_error()) {
        std::cout << result_b.error().to_string() << std::endl;
        std::cout << "  payload 项数 = " << result_b.error().payload_size() << std::endl;
        if (result_b.error().cause) {
            std::cout << "  cause message = " << result_b.error().cause->message << std::endl;
        }
    }

    section("6.5 场景C：用户校验失败");
    auto result_c = create_order(-1, 2, 199);
    if (result_c.is_error()) {
        std::cout << "  " << result_c.error().message << std::endl;
    }

    section("6.6 插件统计结果（min_level=error 过滤 info）");
    std::cout << "  总错误数 = " << stats.total() << std::endl;
    std::cout << "  (场景B 的 fatal 被统计，场景C 的 info 被过滤)" << std::endl;

    plugin_registry.unregister_plugin("stats");

    return 0;
}
