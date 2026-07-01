#include <iostream>

#include "error_system.h"
#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"
#include "error_system/i18n/locale.h"
#include "error_system/i18n/subsystem_module_catalog.h"
// IWYU pragma: begin_exports
#include "payment_service_errors.h"
#include "trade_service_errors.h"
// IWYU pragma: end_exports

using namespace error_system::core;
using namespace error_system::config;
using namespace error_system::domain;

int main() {
    std::cout << "===== Demo 5: 错误码元数据注册与文本输出 =====" << std::endl;

    std::cout << "\n--- 1. 注册子系统/模块名称和错误码 ---" << std::endl;
    auto& registry = error_registry_t::instance();
    auto& catalog = error_system::i18n::subsystem_module_catalog_t::instance();

    auto code1 = error_code_t{error_level_t::error, system_domain_t::application, subsystem_id_t{101}, module_id_t{1}, error_number_t{1001}};
    catalog.register_subsystem_module(101, 1, "交易服务", "订单模块");
    registry.register_error(code1, "ERR_ORDER_CREATE_FAILED", "订单创建失败");

    auto code2 = error_code_t{error_level_t::error, system_domain_t::application, subsystem_id_t{102}, module_id_t{1}, error_number_t{2001}};
    catalog.register_subsystem_module(102, 1, "支付服务", "支付网关");
    registry.register_error(code2, "ERR_PAYMENT_FAILED", "支付失败");

    auto code3 = error_code_t{error_level_t::warn, system_domain_t::application, subsystem_id_t{999}, module_id_t{99}, error_number_t{9999}};
    registry.register_error(code3, "ERR_UNKNOWN_MODULE", "未知模块错误");

    std::cout << "已注册 3 个错误码" << std::endl;

    std::cout << "\n--- 2. 查看元数据 ---" << std::endl;
    if (auto info = registry.get_info(code1); info.has_value()) {
        std::cout << "错误码: " << info->name << std::endl;
        std::cout << "描述: " << info->description << std::endl;
    }
    const auto sm_info = catalog.get_subsystem_module_info(code1.get_subsys(), code1.get_module());
    std::cout << "子系统: " << sm_info.subsystem_name << std::endl;
    std::cout << "模块: " << sm_info.module_name << std::endl;

    std::cout << "\n--- 3. 文本输出模式（默认开启） ---" << std::endl;
    error_context_t ctx1{located_code_t{code1}, "订单ID: 1234567890"};
    std::cout << error_context_serializer_t::to_string(ctx1) << std::endl;

    std::cout << "\n--- 4. 关闭文本输出模式（显示原始 ID） ---" << std::endl;
    i18n_config_t::set_enable_i18n(false);
    error_context_t ctx2{located_code_t{code1}, "订单ID: 1234567890"};
    std::cout << error_context_serializer_t::to_string(ctx2) << std::endl;

    std::cout << "\n--- 5. 默认名称（未指定子系统/模块名称） ---" << std::endl;
    i18n_config_t::set_enable_i18n(true);
    error_context_t ctx3{located_code_t{code3}, "未知模块测试"};
    std::cout << error_context_serializer_t::to_string(ctx3) << std::endl;

    std::cout << "\n--- 6. 按子系统查询错误码 ---" << std::endl;
    auto code1b = error_code_t{error_level_t::error, system_domain_t::application, subsystem_id_t{101}, module_id_t{2}, error_number_t{1002}};
    catalog.register_subsystem_module(101, 2, "交易服务", "结算模块");
    registry.register_error(code1b, "ERR_SETTLE_FAILED", "结算失败");

    auto subsystem_errors = registry.get_errors_by_subsystem(101);
    std::cout << "子系统 101 (交易服务) 下共有 " << subsystem_errors.size() << " 个错误码:" << std::endl;
    for (const auto& meta : subsystem_errors) {
        const auto sm = catalog.get_subsystem_module_info(101, meta.module_id);
        std::cout << "  [" << sm.module_name << "] " << meta.name
                  << " (编号: " << meta.error_number << ")" << std::endl;
    }

    std::cout << "\n--- 7. find_by_name() 按名称查找 ---" << std::endl;

    auto found = registry.find_by_name("ERR_ORDER_CREATE_FAILED");
    if (found.has_value()) {
        std::cout << "找到 ERR_ORDER_CREATE_FAILED: 0x"
                  << std::hex << found->get_code() << std::dec << std::endl;
    }

    auto not_found = registry.find_by_name("ERR_NON_EXISTENT");
    if (!not_found.has_value()) {
        std::cout << "ERR_NON_EXISTENT 未找到" << std::endl;
    }

    std::cout << "\n--- 8. identity_code 解析示例 ---" << std::endl;
    error_context_t ctx8{located_code_t{code1}, "测试 identity_code"};
    std::cout << "identity_code: " << code1.get_identity_code() << std::endl;
    std::cout << "JSON 输出: " << error_context_serializer_t::to_json(ctx8) << std::endl;

    std::cout << "\n--- 9. i18n 多语言子系统/模块名称 ---" << std::endl;

    namespace i18n = error_system::i18n;
    catalog.register_subsystem_module(i18n::locale_t::zh_CN, 101, 1, "交易服务", "订单模块");
    catalog.register_subsystem_module(i18n::locale_t::en_US, 101, 1, "Trade Service", "Order Module");
    catalog.register_subsystem_module(i18n::locale_t::ja_JP, 101, 1, "取引サービス", "注文モジュール");

    std::cout << "locale_t::en_US -> " << i18n::to_string(i18n::locale_t::en_US) << std::endl;
    const auto parsed_locale = i18n::from_string("ja_JP");
    std::cout << "from_string(\"ja_JP\") -> "
              << i18n::to_string(parsed_locale) << std::endl;

    namespace cfg = error_system::config;
    cfg::i18n_config_t::set_enable_i18n(true);
    cfg::i18n_config_t::set_default_locale(i18n::locale_t::zh_CN);

    using error_system::domain::system_domain_t;
    const auto code_i18n = error_code_t(error_level_t::error, system_domain_t::database, subsystem_id_t{101}, module_id_t{1}, error_number_t{9});
    registry.register_error(code_i18n, "ERR_I18N_DEMO", "i18n 演示");

    cfg::i18n_config_t::clear_output_locale();
    error_context_t ctx_zh{located_code_t{code_i18n}, "中文输出"};
    std::cout << "\n[default=zh_CN] " << error_context_serializer_t::to_string(ctx_zh) << std::endl;

    cfg::i18n_config_t::set_output_locale(i18n::locale_t::en_US);
    error_context_t ctx_en{located_code_t{code_i18n}, "English output"};
    std::cout << "[output=en_US] " << error_context_serializer_t::to_string(ctx_en) << std::endl;

    cfg::i18n_config_t::set_output_locale(i18n::locale_t::ja_JP);
    error_context_t ctx_ja{located_code_t{code_i18n}, "日本語出力"};
    std::cout << "[output=ja_JP] " << error_context_serializer_t::to_string(ctx_ja) << std::endl;

    cfg::i18n_config_t::set_enable_i18n(false);
    error_context_t ctx_raw{located_code_t{code_i18n}, "i18n disabled"};
    std::cout << "[i18n=false]  " << error_context_serializer_t::to_string(ctx_raw) << std::endl;

    cfg::i18n_config_t::set_enable_i18n(true);
    cfg::i18n_config_t::clear_output_locale();

    return 0;
}
