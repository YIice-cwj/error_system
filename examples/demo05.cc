#include "error_system.h"
#include "error_system/core/error_registry.h"
#include "error_system/translator/error_translator.h"
#include "payment_service_errors.h"
#include "trade_service_errors.h"
#include <iostream>

using namespace error_system::core;
using namespace error_system::translator;
using namespace error_system::domain;

// 使用宏注册传统模块（静态初始化时自动注册）
void init_legacy_services() {
    REGISTER_LEGACY_MODULE(101, 1, "交易服务", "订单模块");
    REGISTER_LEGACY_MODULE(102, 1, "支付服务", "支付网关");
}

int main() {
    init_legacy_services();
    std::cout << "===== Demo 5: 错误翻译器 =====" << std::endl;

    // 1. 动态注册子系统和模块
    std::cout << "\n--- 1. 动态注册子系统和模块 ---" << std::endl;
    auto& translator = error_translator_t::instance();

    // 注册新的子系统
    translator.register_subsystem(201, "用户服务");
    translator.register_subsystem(202, "库存服务");

    // 注册模块
    translator.register_module(201, 1, "认证模块");
    translator.register_module(201, 2, "权限模块");
    translator.register_module(202, 1, "库存查询");
    translator.register_module(202, 2, "库存扣减");

    std::cout << "已注册子系统和模块" << std::endl;

    // 2. 使用翻译器生成错误信息
    std::cout << "\n--- 2. 翻译错误信息 ---" << std::endl;

    // 翻译静态注册的模块（通过宏注册）
    std::string trade_info = translator.translate(101, 1);
    std::cout << "交易服务/订单模块: " << trade_info << std::endl;

    std::string payment_info = translator.translate(102, 1);
    std::cout << "支付服务/支付网关: " << payment_info << std::endl;

    // 翻译动态注册的模块
    std::string user_info = translator.translate(201, 1);
    std::cout << "用户服务/认证模块: " << user_info << std::endl;

    std::string inventory_info = translator.translate(202, 2);
    std::cout << "库存服务/库存扣减: " << inventory_info << std::endl;

    // 3. 在错误上下文中使用翻译器
    std::cout << "\n--- 3. 错误上下文中的翻译 ---" << std::endl;

    // 创建错误上下文
    auto code = error_builder_t::make_error_code(error_level_t::error, system_domain_t::application, 101, 1, 1001);
    error_registry_t::instance().register_error(code, "ERR_ORDER_CREATE_FAILED", "订单创建失败");
    error_context_t ctx(code, "订单ID: 1234567890");

    // 使用翻译器生成描述
    std::string subsys_module = translator.translate(ctx.code.get_subsys(), ctx.code.get_module());
    std::cout << "错误发生位置: " << subsys_module << std::endl;
    std::cout << "错误详情: " << ctx.to_string() << std::endl;

    // 4. 使用静态函数获取名称
    std::cout << "\n--- 4. 静态名称查询 ---" << std::endl;

    auto subsys_name = get_static_subsys_name(101);
    auto module_name = get_static_module_name(101, 1);

    std::cout << "子系统 101 名称: " << subsys_name << std::endl;
    std::cout << "模块 101:1 名称: " << module_name << std::endl;

    // 5. 未注册模块的处理
    std::cout << "\n--- 5. 未注册模块处理 ---" << std::endl;
    std::string unknown = translator.translate(999, 999);
    std::cout << "未注册模块 999:999: " << unknown << std::endl;

    return 0;
}
