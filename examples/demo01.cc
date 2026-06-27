#include <iostream>

#include "error_system.h"
#include "payment_service_errors.h"
#include "redis_component_errors.h"
#include "trade_service_errors.h"
#include "user_service_errors.h"

using namespace error_system::core;
using namespace error_system::config;
using namespace error_system::domain;

int main() {
    std::cout << "===== Demo 1: 基础错误码使用 =====" << std::endl;

    // 1. 使用自动生成的错误码
    std::cout << "\n--- 1. 使用自动生成的错误码 ---" << std::endl;
    error_context_t ctx1{biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单查询失败"};
    ctx1.with("user_id", "8848").with("action", "query_order");
    std::cout << ctx1 << std::endl;

    // 2. 不同等级的错误码
    std::cout << "\n--- 2. 不同等级的错误 ---" << std::endl;
    error_context_t ctx2{biz::user_errors::ERR_TOKEN_EXPIRED, "Token 已过期"};
    std::cout << "Info 级别: " << ctx2 << std::endl;

    error_context_t ctx3{biz::payment_errors::ERR_ACCOUNT_FROZEN, "账户已被冻结"};
    std::cout << "Fatal 级别: " << ctx3 << std::endl;

    // 3. Sign 位语义（v2.1: 0=false=错误, 1=true=成功）
    std::cout << "\n--- 3. Sign 位语义 (0=错误, 1=成功) ---" << std::endl;
    auto code = biz::trade_errors::ERR_ORDER_NOT_FOUND;
    std::cout << "错误码: 0x" << std::hex << code.get_code() << std::dec << std::endl;
    std::cout << "  Sign (成功=1): " << static_cast<int>(code.get_sign()) << std::endl;
    std::cout << "  is_error(): " << ctx1.is_error() << " (sign=0=错误)" << std::endl;
    std::cout << "  is_success(): " << ctx1.is_success() << " (sign=1=成功)" << std::endl;

    // 默认构造的 error_code_t 为成功码（sign=1）
    error_code_t default_code;
    std::cout << "  默认构造函数 sign: " << static_cast<int>(default_code.get_sign()) << " (应为 1=成功)" << std::endl;

    // 4. 错误码解析
    std::cout << "\n--- 4. 错误码解析 ---" << std::endl;
    std::cout << "  - identity_code: " << code.get_identity_code() << std::endl;
    std::cout << "  - 等级: " << static_cast<int>(code.get_level()) << std::endl;
    std::cout << "  - 系统域: " << static_cast<int>(code.get_system()) << std::endl;
    std::cout << "  - 子系统: " << code.get_subsys() << std::endl;
    std::cout << "  - 模块: " << code.get_module() << std::endl;
    std::cout << "  - 编号: " << code.get_number() << std::endl;

    // 5. JSON 序列化（v2.1: code 字段使用 identity_code）
    std::cout << "\n--- 5. JSON 序列化（identity_code） ---" << std::endl;
    std::cout << error_context_serializer_t::to_json(ctx1) << std::endl;

    // 6. 二进制序列化（v2.1: 含因果链标志位）
    std::cout << "\n--- 6. 二进制序列化 ---" << std::endl;
    std::string binary = error_context_serializer_t::to_binary(ctx1);
    std::cout << "  无 cause 二进制大小: " << binary.size() << " bytes" << std::endl;

    // 7. 因果链 + 二进制序列化对比
    std::cout << "\n--- 7. 错误因果链（含二进制序列化） ---" << std::endl;
    error_context_t root{infra::redis_errors::ERR_KEY_NOT_FOUND, "Redis 键不存在"};
    error_context_t wrapper{biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单服务不可用"};
    auto chained = wrapper.wrap(root);
    std::cout << chained << std::endl;

    std::string binary_with_cause = error_context_serializer_t::to_binary(chained);
    std::cout << "  有 cause 二进制大小: " << binary_with_cause.size() << " bytes" << std::endl;
    std::cout << "  (无 cause: " << binary.size() << " bytes, 差距 = cause 链大小)" << std::endl;

    // 8. with_batch 批量添加 payload（v2.1）
    std::cout << "\n--- 8. with_batch 批量添加 payload ---" << std::endl;
    error_context_t ctx4{biz::payment_errors::ERR_INSUFFICIENT_BALANCE, "余额不足"};
    ctx4.with_batch({
        {"user_id", "8848"},
        {"balance", "150"},
        {"required", "500"},
        {"is_vip", "false"},
        {"exchange_rate", "7.25"}
    });
    std::cout << ctx4 << std::endl;

    // 9. payload 多类型值（模板 with）
    std::cout << "\n--- 9. payload 多类型值 ---" << std::endl;
    error_context_t ctx5{biz::payment_errors::ERR_INSUFFICIENT_BALANCE, "余额不足"};
    ctx5.with("user_id", 8848)
        .with("balance", 150)
        .with("required", 500)
        .with("is_vip", false)
        .with("exchange_rate", 7.25);
    std::cout << ctx5 << std::endl;

    // 10. compare operators（v2.1）
    std::cout << "\n--- 10. compare operators ---" << std::endl;
    error_context_t ctx_a{biz::trade_errors::ERR_ORDER_NOT_FOUND, "相同消息"};
    error_context_t ctx_b{biz::trade_errors::ERR_ORDER_NOT_FOUND, "相同消息"};
    std::cout << "  ctx_a == ctx_b: " << (ctx_a == ctx_b) << " (应为 1)" << std::endl;
    std::cout << "  ctx_a != ctx1: " << (ctx_a != ctx1) << " (应为 1, 消息不同)" << std::endl;

    return 0;
}