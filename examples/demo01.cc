#include "error_system.h"
#include "payment_service_errors.h"
#include "redis_component_errors.h"
#include "trade_service_errors.h"
#include "user_service_errors.h"
#include <iostream>

using namespace error_system::core;
using namespace error_system::config;
using namespace error_system::domain;

int main() {
    std::cout << "===== Demo 1: 基础错误码使用 =====" << std::endl;

    // 1. 使用自动生成的错误码
    std::cout << "\n--- 1. 使用自动生成的错误码 ---" << std::endl;
    error_context_t ctx1(biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单查询失败");
    ctx1.with("user_id", "8848").with("action", "query_order");
    std::cout << ctx1 << std::endl;

    // 2. 使用不同等级的错误码
    std::cout << "\n--- 2. 不同等级的错误 ---" << std::endl;
    error_context_t ctx2(biz::user_errors::ERR_TOKEN_EXPIRED, "Token 已过期");
    std::cout << "Info 级别: " << ctx2 << std::endl;

    error_context_t ctx3(biz::payment_errors::ERR_ACCOUNT_FROZEN, "账户已被冻结");
    std::cout << "Fatal 级别: " << ctx3 << std::endl;

    // 3. 错误码解析
    std::cout << "\n--- 3. 错误码解析 ---" << std::endl;
    auto code = biz::trade_errors::ERR_ORDER_NOT_FOUND;
    std::cout << "错误码: 0x" << std::hex << code.get_code() << std::dec << std::endl;
    std::cout << "  - 等级: " << static_cast<int>(code.get_level()) << std::endl;
    std::cout << "  - 系统域: " << static_cast<int>(code.get_system()) << std::endl;
    std::cout << "  - 子系统: " << code.get_subsys() << std::endl;
    std::cout << "  - 模块: " << code.get_module() << std::endl;
    std::cout << "  - 编号: " << code.get_number() << std::endl;

    // 4. JSON 序列化
    std::cout << "\n--- 4. JSON 序列化 ---" << std::endl;
    std::cout << ctx1.to_json() << std::endl;

    // 5. 因果链
    std::cout << "\n--- 5. 错误因果链 ---" << std::endl;
    error_context_t root(infra::redis_errors::ERR_KEY_NOT_FOUND, "Redis 键不存在");
    error_context_t wrapper(biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单服务不可用");
    auto chained = wrapper.wrap(root);
    std::cout << chained << std::endl;

    return 0;
}
