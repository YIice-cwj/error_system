/**
 * @file demo01.cc
 * @brief error_context_t 错误上下文 API 全览
 * @details 演示 error_context_t 的全部公共 API：构造、payload、谓词、序列化、比较、因果链。
 *          每个示例只做一件事，标题即 API 名，便于快速查找。
 */

#include <iostream>

#include "error_system.h"
#include "payment_service_errors.h"
#include "redis_component_errors.h"
#include "trade_service_errors.h"
#include "user_service_errors.h"

using namespace error_system::core;
using namespace error_system::domain;

namespace {

/** @brief 打印小节标题 */
void section(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

}  // namespace

int main() {
    std::cout << "===== Demo 1: error_context_t API 全览 =====" << std::endl;

    // ============================================================
    // 一、构造：5 种方式
    // ============================================================
    section("1.1 构造：错误码 + 消息（最常用）");
    error_context_t ctx1{biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单查询失败"};
    std::cout << "  " << ctx1 << std::endl;

    section("1.2 构造：错误码 + 消息 + 源位置");
    error_context_t ctx2{biz::trade_errors::ERR_ORDER_NOT_FOUND, "带源位置"};
    std::cout << "  " << ctx2 << std::endl;

    section("1.3 构造：默认构造（sign=1=成功）");
    error_context_t default_ctx;
    std::cout << "  is_success=" << default_ctx.is_success() << " is_error=" << default_ctx.is_error() << std::endl;

    section("1.4 构造：from_exception 从 std::exception 创建");
    try {
        throw std::runtime_error("数据库连接超时");
    } catch (const std::exception& e) {
        auto ctx = error_context_t::from_exception(biz::trade_errors::ERR_ORDER_NOT_FOUND, e);
        std::cout << "  " << ctx.message << std::endl;
    }

    section("1.5 构造：错误码解析（sign/level/system/subsystem/module/number）");
    auto code = biz::trade_errors::ERR_ORDER_NOT_FOUND;
    std::cout << "  sign=" << static_cast<int>(code.get_sign()) << " (0=错误,1=成功)" << std::endl;
    std::cout << "  level=" << static_cast<int>(code.get_level()) << std::endl;
    std::cout << "  system=" << static_cast<int>(code.get_system()) << std::endl;
    std::cout << "  subsystem=" << code.get_subsys() << std::endl;
    std::cout << "  module=" << code.get_module() << std::endl;
    std::cout << "  number=" << code.get_number() << std::endl;
    std::cout << "  identity_code=" << code.get_identity_code() << std::endl;

    // ============================================================
    // 二、payload：5 种添加方式 + 3 种查询方式
    // ============================================================
    section("2.1 with(string,string) 字符串键值");
    error_context_t p1{biz::trade_errors::ERR_ORDER_NOT_FOUND, "payload 演示"};
    p1.with("user_id", "8848");
    std::cout << "  " << p1 << std::endl;

    section("2.2 with(const char*, const char*) C 字符串");
    error_context_t p2{biz::trade_errors::ERR_ORDER_NOT_FOUND, "C 字符串"};
    p2.with("host", "192.168.1.1");
    std::cout << "  " << p2 << std::endl;

    section("2.3 with(string_view, string_view) 视图");
    error_context_t p3{biz::trade_errors::ERR_ORDER_NOT_FOUND, "string_view"};
    p3.with(std::string_view("port"), std::string_view("3306"));
    std::cout << "  " << p3 << std::endl;

    section("2.4 with(string&&, string&&) 移动语义");
    error_context_t p4{biz::trade_errors::ERR_ORDER_NOT_FOUND, "移动语义"};
    p4.with(std::string("moved_key"), std::string("moved_value"));
    std::cout << "  " << p4 << std::endl;

    section("2.5 with(key, T) 多类型（int/double/bool）");
    error_context_t p5{biz::payment_errors::ERR_INSUFFICIENT_BALANCE, "多类型"};
    p5.with("retry_count", 3)
        .with("balance", 150)
        .with("exchange_rate", 7.25)
        .with("is_vip", false);
    std::cout << "  " << p5 << std::endl;

    section("2.6 with_batch 批量添加");
    error_context_t p6{biz::payment_errors::ERR_INSUFFICIENT_BALANCE, "批量"};
    p6.with_batch({
        {"user_id",  "8848"},
        {"balance",  "150" },
        {"required", "500" }
    });
    std::cout << "  " << p6 << std::endl;

    section("2.7 get_payload_value 按 key 取值");
    auto val = p1.get_payload_value("user_id");
    std::cout << "  user_id = " << val.value_or("未找到") << std::endl;

    section("2.8 payload_size 项数");
    std::cout << "  p5 项数 = " << p5.payload_size() << std::endl;

    section("2.9 is_payload_empty 判空");
    std::cout << "  p1 empty = " << p1.is_payload_empty() << " (应为 0)" << std::endl;
    std::cout << "  ctx1 empty = " << ctx1.is_payload_empty() << " (应为 1)" << std::endl;

    section("2.10 get_payload 获取全部副本");
    auto all = p6.get_payload();
    std::cout << "  共 " << all.size() << " 项：" << std::endl;
    for (const auto& [k, v] : all) {
        std::cout << "    " << k << " = " << v << std::endl;
    }

    section("2.11 for_each_payload 遍历");
    std::cout << "  p5 内容：" << std::endl;
    p5.for_each_payload([](const std::string& k, const std::string& v) {
        std::cout << "    " << k << " = " << v << std::endl;
    });

    // ============================================================
    // 三、谓词判断：is_success/is_error/is_fatal/is_retryable/is_transient
    // ============================================================
    section("3.1 is_success / is_error（sign 位）");
    std::cout << "  ctx1: is_success=" << ctx1.is_success() << " is_error=" << ctx1.is_error() << std::endl;

    section("3.2 is_fatal 是否 fatal 级别");
    error_context_t fatal_ctx{biz::payment_errors::ERR_ACCOUNT_FROZEN, "账户冻结"};
    std::cout << "  fatal_ctx.is_fatal() = " << fatal_ctx.is_fatal() << std::endl;
    std::cout << "  ctx1.is_fatal() = " << ctx1.is_fatal() << std::endl;

    section("3.3 is_retryable 是否可重试");
    std::cout << "  ctx1.is_retryable() = " << ctx1.is_retryable() << std::endl;

    section("3.4 is_transient 是否瞬态");
    std::cout << "  ctx1.is_transient() = " << ctx1.is_transient() << std::endl;

    // ============================================================
    // 四、序列化：3 种格式
    // ============================================================
    section("4.1 to_string 可读文本");
    error_context_t s1{biz::trade_errors::ERR_ORDER_NOT_FOUND, "序列化演示"};
    s1.with("user_id", "8848").with("action", "query");
    std::cout << s1.to_string() << std::endl;

    section("4.2 to_json JSON 格式");
    std::cout << s1.to_json() << std::endl;

    section("4.3 to_binary 紧凑二进制");
    std::string binary = s1.to_binary();
    std::cout << "  二进制大小: " << binary.size() << " bytes" << std::endl;

    section("4.4 what() C 字符串消息");
    std::cout << "  message = " << s1.what() << std::endl;

    section("4.5 get_code() 获取错误码引用");
    std::cout << "  identity_code = " << s1.get_code().get_identity_code() << std::endl;

    // ============================================================
    // 五、比较：operator== / != / equals_by_code / equals_strict
    // ============================================================
    section("5.1 operator== 完整比较（code+message+payload）");
    error_context_t eq_a{biz::trade_errors::ERR_ORDER_NOT_FOUND, "相同消息"};
    error_context_t eq_b{biz::trade_errors::ERR_ORDER_NOT_FOUND, "相同消息"};
    std::cout << "  eq_a == eq_b: " << (eq_a == eq_b) << " (应为 1)" << std::endl;

    section("5.2 operator!=");
    std::cout << "  eq_a != ctx1: " << (eq_a != ctx1) << " (应为 1, 消息不同)" << std::endl;

    section("5.3 equals_by_code 仅按错误码比较");
    error_context_t same_code_a{biz::trade_errors::ERR_ORDER_NOT_FOUND, "消息A"};
    error_context_t same_code_b{biz::trade_errors::ERR_ORDER_NOT_FOUND, "消息B"};
    std::cout << "  消息不同但码相同: " << same_code_a.equals_by_code(same_code_b) << " (应为 1)" << std::endl;

    section("5.4 equals_strict 严格比较（含 cause/stack）");
    std::cout << "  eq_a.equals_strict(eq_b) = " << eq_a.equals_strict(eq_b) << " (应为 1)" << std::endl;

    // ============================================================
    // 六、因果链：wrap()
    // ============================================================
    section("6.1 wrap(底层错误) 构建因果链");
    error_context_t root{infra::redis_errors::ERR_KEY_NOT_FOUND, "Redis 键不存在"};
    error_context_t wrapper{biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单服务不可用"};
    auto chained = wrapper.wrap(root);
    std::cout << chained << std::endl;

    section("6.2 wrap(右值错误) 移动语义版本");
    auto chained2 = wrapper.wrap(error_context_t{infra::redis_errors::ERR_KEY_NOT_FOUND, "移动的 root"});
    std::cout << chained2 << std::endl;

    section("6.3 因果链二进制序列化");
    std::string binary_chain = chained.to_binary();
    std::cout << "  单层大小: " << wrapper.to_binary().size() << " bytes" << std::endl;
    std::cout << "  链大小: " << binary_chain.size() << " bytes" << std::endl;

    return 0;
}
