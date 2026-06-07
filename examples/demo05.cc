#include "error_system.h"
#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"
// IWYU pragma: begin_exports
#include "payment_service_errors.h"
#include "trade_service_errors.h"
// IWYU pragma: end_exports
#include <iostream>

using namespace error_system::core;
using namespace error_system::config;
using namespace error_system::domain;

int main() {
    std::cout << "===== Demo 5: 错误码元数据注册与文本输出 =====" << std::endl;

    // 1. 注册子系统/模块名称 + 错误码
    std::cout << "\n--- 1. 注册子系统/模块名称和错误码 ---" << std::endl;
    auto& registry = error_registry_t::instance();

    auto code1 = error_builder_t::make_error_code(error_level_t::error, system_domain_t::application, 101, 1, 1001);
    registry.register_subsystem_module(101, 1, "交易服务", "订单模块");
    registry.register_error(code1, "ERR_ORDER_CREATE_FAILED", "订单创建失败");

    auto code2 = error_builder_t::make_error_code(error_level_t::error, system_domain_t::application, 102, 1, 2001);
    registry.register_subsystem_module(102, 1, "支付服务", "支付网关");
    registry.register_error(code2, "ERR_PAYMENT_FAILED", "支付失败");

    // 不注册子系统/模块名称，使用默认值
    auto code3 = error_builder_t::make_error_code(error_level_t::warn, system_domain_t::application, 999, 99, 9999);
    registry.register_error(code3, "ERR_UNKNOWN_MODULE", "未知模块错误");

    std::cout << "已注册 3 个错误码" << std::endl;

    // 2. 查看元数据
    std::cout << "\n--- 2. 查看元数据 ---" << std::endl;
    if (auto info = registry.get_info(code1)) {
        const auto& meta = info.value().get();
        std::cout << "错误码: " << meta.name << std::endl;
        std::cout << "描述: " << meta.description << std::endl;
    }
    const auto& sm_info = registry.get_subsystem_module_info(code1.get_subsys(), code1.get_module());
    std::cout << "子系统: " << sm_info.subsystem_name << std::endl;
    std::cout << "模块: " << sm_info.module_name << std::endl;

    // 3. 文本输出模式（默认开启）
    std::cout << "\n--- 3. 文本输出模式（默认开启） ---" << std::endl;
    error_context_t ctx1(code1, "订单ID: 1234567890");
    std::cout << ctx1.to_string() << std::endl;

    // 4. 关闭文本输出模式（显示原始 ID）
    std::cout << "\n--- 4. 关闭文本输出模式（显示原始 ID） ---" << std::endl;
    error_config_t::set_enable_text_output(false);
    error_context_t ctx2(code1, "订单ID: 1234567890");
    std::cout << ctx2.to_string() << std::endl;

    // 5. 恢复文本输出，查看默认名称的错误码
    std::cout << "\n--- 5. 默认名称（未指定子系统/模块名称） ---" << std::endl;
    error_config_t::set_enable_text_output(true);
    error_context_t ctx3(code3, "未知模块测试");
    std::cout << ctx3.to_string() << std::endl;

    return 0;
}
