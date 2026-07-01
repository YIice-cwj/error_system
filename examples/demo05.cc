/**
 * @file demo05.cc
 * @brief error_registry_t + i18n 多语言 API 全览
 * @details 演示错误码注册表、locale 工具、i18n 配置、子系统/模块目录、
 *          多语言消息目录的全部公共 API。每个示例只做一件事，标题即 API 名。
 * @note 为避免清空自动注册的全局错误码，本示例不调用 unregister_all()，
 *       仅对自定义错误码演示注销操作。
 */

#include <iostream>
#include <string>
#include <vector>

#include "error_system.h"
#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"
#include "error_system/i18n/i18n.h"
#include "error_system/i18n/locale.h"
#include "error_system/i18n/subsystem_module_catalog.h"
// IWYU pragma: begin_exports
#include "payment_service_errors.h"
#include "trade_service_errors.h"
// IWYU pragma: end_exports

using namespace error_system::core;
using namespace error_system::config;
using namespace error_system::domain;
namespace i18n = error_system::i18n;

namespace {

/** @brief 打印小节标题 */
void section(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

/** @brief 构造一个自定义错误码（subsystem=150, module=1, number=5001） */
error_code_t make_custom_code(uint16_t number) {
    return error_code_t{error_level_t::error,
                        system_domain_t::application,
                        subsystem_id_t{150},
                        module_id_t{1},
                        error_number_t{number}};
}

}  // namespace

int main() {
    std::cout << "===== Demo 5: error_registry_t + i18n API 全览 =====" << std::endl;

    auto& registry = error_registry_t::instance();
    auto& catalog = i18n::subsystem_module_catalog_t::instance();
    auto& i18n_msg = i18n::i18n_t::instance();

    // ============================================================
    // 一、注册表基础：register_error / register_errors / is_registered / get_info
    // ============================================================
    section("1.1 register_error(code, name, desc) 单条注册");
    const auto code_a = make_custom_code(5001);
    registry.register_error(code_a, "ERR_DEMO_5001", "演示错误 5001");
    std::cout << "  is_registered = " << registry.is_registered(code_a) << std::endl;

    section("1.2 register_errors 批量注册（返回成功数量）");
    const std::vector<error_code_t> codes = {
        make_custom_code(5002),
        make_custom_code(5003),
        make_custom_code(5004),
    };
    const std::vector<std::string_view> names = {"ERR_DEMO_5002", "ERR_DEMO_5003", "ERR_DEMO_5004"};
    const std::vector<std::string_view> descs = {"演示错误 5002", "演示错误 5003", "演示错误 5004"};
    auto registered_count = registry.register_errors(codes, names, descs);
    std::cout << "  实际注册数量 = " << registered_count << std::endl;

    section("1.3 is_registered 是否已注册");
    std::cout << "  5001 已注册 = " << registry.is_registered(code_a) << std::endl;
    std::cout << "  未注册码   = " << registry.is_registered(make_custom_code(9999)) << std::endl;

    section("1.4 get_info 查询元数据（返回 optional 副本）");
    if (auto info = registry.get_info(code_a); info.has_value()) {
        std::cout << "  name        = " << info->name << std::endl;
        std::cout << "  description = " << info->description << std::endl;
        std::cout << "  module_id   = " << info->module_id << std::endl;
        std::cout << "  error_number= " << info->error_number << std::endl;
    }

    section("1.5 get_info_cached 热路径缓存查询（thread_local）");
    if (auto info = registry.get_info_cached(code_a); info.has_value()) {
        std::cout << "  缓存查询 name = " << info->name << std::endl;
    }

    section("1.6 invalidate_metadata_cache 清除当前线程缓存");
    registry.invalidate_metadata_cache();
    std::cout << "  已清除（下次查询会重建缓存）" << std::endl;

    section("1.7 get_epoch 注册表纪元（变更自增）");
    const uint64_t epoch_before = registry.get_epoch();
    registry.register_error(make_custom_code(5999), "ERR_DEMO_EPOCH", "纪元测试");
    const uint64_t epoch_after = registry.get_epoch();
    std::cout << "  注册前 epoch = " << epoch_before << std::endl;
    std::cout << "  注册后 epoch = " << epoch_after << " (应 +1)" << std::endl;

    // ============================================================
    // 二、注册表查询：find_by_name / get_errors_by_module / get_errors_by_subsystem
    // ============================================================
    section("2.1 find_by_name 按名称查找错误码");
    if (auto found = registry.find_by_name("ERR_DEMO_5001"); found.has_value()) {
        std::cout << "  找到: identity_code = " << found->get_identity_code() << std::endl;
    }
    auto not_found = registry.find_by_name("ERR_NON_EXISTENT");
    std::cout << "  未注册名称 has_value = " << not_found.has_value() << std::endl;

    section("2.2 get_errors_by_module 按模块组查找");
    auto module_errors = registry.get_errors_by_module(code_a.get_module_group_id());
    std::cout << "  模块组 [150,1] 下错误数 = " << module_errors.size() << std::endl;
    for (const auto& meta : module_errors) {
        std::cout << "    " << meta.name << " (" << meta.error_number << ")" << std::endl;
    }

    section("2.3 get_errors_by_subsystem 按子系统查找");
    auto subsys_errors = registry.get_errors_by_subsystem(150);
    std::cout << "  子系统 150 下错误数 = " << subsys_errors.size() << std::endl;

    // ============================================================
    // 三、注销：unregister_error / unregister_module
    // ============================================================
    section("3.1 unregister_error(code) 按错误码注销");
    registry.unregister_error(make_custom_code(5999));
    std::cout << "  注销后 is_registered = " << registry.is_registered(make_custom_code(5999)) << std::endl;

    section("3.2 unregister_error(name) 按名称注销");
    registry.unregister_error("ERR_DEMO_5004");
    std::cout << "  注销后 is_registered = " << registry.is_registered(make_custom_code(5004)) << std::endl;

    section("3.3 unregister_module 注销整个模块组");
    registry.register_error(make_custom_code(6000), "ERR_DEMO_6000", "临时错误");
    std::cout << "  注销前模块组错误数 = " << registry.get_errors_by_module(code_a.get_module_group_id()).size() << std::endl;
    // 注意：这会注销本模块组下所有自定义错误码（5001/5002/5003 也会被清掉）
    // 为后续演示方便，这里仅打印说明，不实际执行
    std::cout << "  (避免清空后续演示用码，跳过实际调用)" << std::endl;

    // ============================================================
    // 四、重复策略：set_duplicate_policy / get_duplicate_policy / warn 回调
    // ============================================================
    section("4.1 get_duplicate_policy 默认策略（skip）");
    std::cout << "  当前策略 = " << static_cast<int>(registry.get_duplicate_policy()) << std::endl;

    section("4.2 set_duplicate_policy(skip) 重复时静默跳过");
    registry.set_duplicate_policy(duplicate_policy_t::skip);
    registry.register_error(code_a, "ERR_DEMO_5001_RENAMED", "重复注册的新描述");
    if (auto info = registry.get_info(code_a); info.has_value()) {
        std::cout << "  保留原 name = " << info->name << " (skip 不覆盖)" << std::endl;
    }

    section("4.3 set_duplicate_policy(overwrite) 重复时覆盖");
    registry.set_duplicate_policy(duplicate_policy_t::overwrite);
    registry.register_error(code_a, "ERR_DEMO_5001_OVERWRITTEN", "覆盖后的描述");
    if (auto info = registry.get_info(code_a); info.has_value()) {
        std::cout << "  新 name = " << info->name << std::endl;
        std::cout << "  新 desc = " << info->description << std::endl;
    }

    section("4.4 set_duplicate_policy(warn) + set_duplicate_warn_callback");
    registry.set_duplicate_policy(duplicate_policy_t::warn);
    registry.set_duplicate_warn_callback(
        [](code_t raw_code, const error_metadata_t& existing) {
            std::cout << "  [WARN] 0x" << std::hex << raw_code << std::dec
                      << " 已存在为 " << existing.name << "，重复注册被忽略" << std::endl;
        });
    registry.register_error(code_a, "ERR_DEMO_5001_AGAIN", "再次重复");
    std::cout << "  (上方应出现 WARN 输出)" << std::endl;

    section("4.5 get_duplicate_warn_callback 获取当前回调");
    const auto& cb = registry.get_duplicate_warn_callback();
    std::cout << "  callback 是否已设置 = " << (cb ? 1 : 0) << std::endl;

    // 恢复默认策略，避免影响后续示例
    registry.set_duplicate_policy(duplicate_policy_t::skip);
    registry.set_duplicate_warn_callback(nullptr);

    // ============================================================
    // 五、locale_t 工具：to_string / from_string / is_valid
    // ============================================================
    section("5.1 to_string(locale) 枚举转字符串");
    std::cout << "  zh_CN -> " << i18n::to_string(i18n::locale_t::zh_CN) << std::endl;
    std::cout << "  en_US -> " << i18n::to_string(i18n::locale_t::en_US) << std::endl;
    std::cout << "  ja_JP -> " << i18n::to_string(i18n::locale_t::ja_JP) << std::endl;

    section("5.2 from_string(text) 字符串转枚举（未识别返回 en_US）");
    const auto parsed = i18n::from_string("ko_KR");
    std::cout << "  from_string(\"ko_KR\") -> " << i18n::to_string(parsed) << std::endl;
    const auto unknown = i18n::from_string("xx_YY");
    std::cout << "  from_string(\"xx_YY\") -> " << i18n::to_string(unknown) << " (回退 en_US)" << std::endl;

    section("5.3 is_valid(text) 校验字符串");
    std::cout << "  is_valid(\"zh_CN\") = " << i18n::is_valid("zh_CN") << std::endl;
    std::cout << "  is_valid(\"xx_YY\") = " << i18n::is_valid("xx_YY") << std::endl;

    // ============================================================
    // 六、i18n_config_t：全局 i18n 开关与 locale 配置
    // ============================================================
    section("6.1 set_enable_i18n / is_i18n_enabled 总开关");
    i18n_config_t::set_enable_i18n(true);
    std::cout << "  is_i18n_enabled = " << i18n_config_t::is_i18n_enabled() << std::endl;

    section("6.2 set_default_locale / get_default_locale 默认 locale");
    i18n_config_t::set_default_locale(i18n::locale_t::zh_CN);
    std::cout << "  default_locale = " << i18n::to_string(i18n_config_t::get_default_locale()) << std::endl;

    section("6.3 set_output_locale / get_output_locale 显式输出 locale");
    i18n_config_t::set_output_locale(i18n::locale_t::en_US);
    if (auto ol = i18n_config_t::get_output_locale(); ol.has_value()) {
        std::cout << "  output_locale = " << i18n::to_string(*ol) << std::endl;
    }

    section("6.4 clear_output_locale 清除显式输出 locale");
    i18n_config_t::clear_output_locale();
    std::cout << "  get_output_locale has_value = " << i18n_config_t::get_output_locale().has_value() << std::endl;

    section("6.5 resolve_output_locale 解析最终 locale（output→default）");
    std::cout << "  无 output 时 resolve = " << i18n::to_string(i18n_config_t::resolve_output_locale()) << std::endl;
    i18n_config_t::set_output_locale(i18n::locale_t::ja_JP);
    std::cout << "  有 output 时 resolve = " << i18n::to_string(i18n_config_t::resolve_output_locale()) << std::endl;
    i18n_config_t::clear_output_locale();

    // ============================================================
    // 七、subsystem_module_catalog_t：子系统/模块名称多语言目录
    // ============================================================
    section("7.1 register_subsystem_module(subsys, module, names) 默认 zh_CN");
    catalog.register_subsystem_module(150, 1, "演示子系统", "演示模块");
    auto info_default = catalog.get_subsystem_module_info(150, 1);
    std::cout << "  子系统 = " << info_default.subsystem_name << std::endl;
    std::cout << "  模块   = " << info_default.module_name << std::endl;

    section("7.2 register_subsystem_module(locale, subsys, module, names) 多语言");
    catalog.register_subsystem_module(i18n::locale_t::zh_CN, 150, 1, "演示子系统", "演示模块");
    catalog.register_subsystem_module(i18n::locale_t::en_US, 150, 1, "Demo Subsystem", "Demo Module");
    catalog.register_subsystem_module(i18n::locale_t::ja_JP, 150, 1, "デモサブシステム", "デモモジュール");

    section("7.3 get_subsystem_module_info(locale, subsys, module) 指定 locale");
    auto info_en = catalog.get_subsystem_module_info(i18n::locale_t::en_US, 150, 1);
    std::cout << "  en_US 子系统 = " << info_en.subsystem_name << std::endl;
    auto info_ja = catalog.get_subsystem_module_info(i18n::locale_t::ja_JP, 150, 1);
    std::cout << "  ja_JP 子系统 = " << info_ja.subsystem_name << std::endl;

    section("7.4 get_subsystem_module_info(locale, fallback, subsys, module) 带回退");
    // ko_KR 未注册，回退到 zh_CN
    auto info_fallback = catalog.get_subsystem_module_info(i18n::locale_t::ko_KR, i18n::locale_t::zh_CN, 150, 1);
    std::cout << "  ko_KR 回退到 zh_CN = " << info_fallback.subsystem_name << std::endl;

    section("7.5 resolve_subsystem_module 接口形式（用于注入）");
    auto info_resolved = catalog.resolve_subsystem_module(
        i18n::locale_t::en_US, i18n::locale_t::zh_CN, 150, 1);
    std::cout << "  resolved 子系统 = " << info_resolved.subsystem_name << std::endl;

    section("7.6 clear_locale 清除指定 locale 的所有名称");
    auto cleared = catalog.clear_locale(i18n::locale_t::ja_JP);
    std::cout << "  清除 ja_JP 条目数 = " << cleared << std::endl;
    auto info_ja_after = catalog.get_subsystem_module_info(i18n::locale_t::ja_JP, 150, 1);
    std::cout << "  清除后 ja_JP 回退到 zh_CN = " << info_ja_after.subsystem_name << std::endl;

    section("7.7 clear 清空所有名称");
    catalog.clear();
    auto info_empty = catalog.get_subsystem_module_info(150, 1);
    std::cout << "  清空后 subsystem_name = \"" << info_empty.subsystem_name << "\" (默认空)" << std::endl;
    // 重新注册以便后续示例使用
    catalog.register_subsystem_module(i18n::locale_t::zh_CN, 150, 1, "演示子系统", "演示模块");
    catalog.register_subsystem_module(i18n::locale_t::en_US, 150, 1, "Demo Subsystem", "Demo Module");

    // ============================================================
    // 八、i18n_t：多语言消息目录
    // ============================================================
    section("8.1 register_message(locale, code, message) 单条注册");
    i18n_msg.register_message(i18n::locale_t::zh_CN, code_a, "演示错误 5001（中文）");
    i18n_msg.register_message(i18n::locale_t::en_US, code_a, "Demo error 5001 (English)");

    section("8.2 register_messages 批量注册");
    const std::vector<std::pair<error_code_t, std::string_view>> msgs = {
        {make_custom_code(5002), "演示错误 5002（中文）"},
        {make_custom_code(5003), "演示错误 5003（中文）"},
    };
    auto msg_count = i18n_msg.register_messages(i18n::locale_t::zh_CN, msgs);
    std::cout << "  批量注册数量 = " << msg_count << std::endl;

    section("8.3 get_message(locale, code) 指定 locale 查询");
    std::cout << "  zh_CN = " << i18n_msg.get_message(i18n::locale_t::zh_CN, code_a) << std::endl;
    std::cout << "  en_US = " << i18n_msg.get_message(i18n::locale_t::en_US, code_a) << std::endl;

    section("8.4 get_message(code) 使用当前输出 locale");
    i18n_config_t::set_default_locale(i18n::locale_t::zh_CN);
    i18n_config_t::clear_output_locale();
    std::cout << "  默认 locale = " << i18n_msg.get_message(code_a) << std::endl;
    i18n_config_t::set_output_locale(i18n::locale_t::en_US);
    std::cout << "  输出 locale = " << i18n_msg.get_message(code_a) << std::endl;
    i18n_config_t::clear_output_locale();

    section("8.5 set_default_locale / get_default_locale 委托 i18n_config_t");
    i18n_msg.set_default_locale(i18n::locale_t::zh_CN);
    std::cout << "  default_locale = " << i18n::to_string(i18n_msg.get_default_locale()) << std::endl;

    section("8.6 set_active_locale / get_active_locale / clear_active_locale");
    i18n_msg.set_active_locale(i18n::locale_t::en_US);
    if (auto active = i18n_msg.get_active_locale(); active.has_value()) {
        std::cout << "  active_locale = " << i18n::to_string(*active) << std::endl;
    }
    i18n_msg.clear_active_locale();
    std::cout << "  clear 后 has_value = " << i18n_msg.get_active_locale().has_value() << std::endl;

    section("8.7 get_locales 已注册 locale 列表");
    auto locales = i18n_msg.get_locales();
    std::cout << "  已注册 locale 数 = " << locales.size() << std::endl;
    for (const auto loc : locales) {
        std::cout << "    " << i18n::to_string(loc) << std::endl;
    }

    section("8.8 message_count 指定 locale 消息数");
    std::cout << "  zh_CN 消息数 = " << i18n_msg.message_count(i18n::locale_t::zh_CN) << std::endl;
    std::cout << "  en_US 消息数 = " << i18n_msg.message_count(i18n::locale_t::en_US) << std::endl;

    section("8.9 clear_locale / clear_all 清除消息");
    auto cleared_msgs = i18n_msg.clear_locale(i18n::locale_t::en_US);
    std::cout << "  清除 en_US 消息数 = " << cleared_msgs << std::endl;
    i18n_msg.clear_all();
    std::cout << "  clear_all 后 zh_CN 消息数 = " << i18n_msg.message_count(i18n::locale_t::zh_CN) << std::endl;

    // ============================================================
    // 九、综合：文本输出开关效果（i18n_config + serializer）
    // ============================================================
    section("9.1 i18n 开启 + 中文 locale（输出名称）");
    i18n_config_t::set_enable_i18n(true);
    i18n_config_t::set_default_locale(i18n::locale_t::zh_CN);
    i18n_config_t::clear_output_locale();
    error_context_t ctx_zh{code_a, "中文输出测试"};
    std::cout << ctx_zh.to_string() << std::endl;

    section("9.2 i18n 开启 + 英文 locale（切换输出语言）");
    i18n_config_t::set_output_locale(i18n::locale_t::en_US);
    error_context_t ctx_en{code_a, "English output test"};
    std::cout << ctx_en.to_string() << std::endl;
    i18n_config_t::clear_output_locale();

    section("9.3 i18n 关闭（回退为原始 ID 数字）");
    i18n_config_t::set_enable_i18n(false);
    error_context_t ctx_raw{code_a, "i18n 关闭"};
    std::cout << ctx_raw.to_string() << std::endl;
    i18n_config_t::set_enable_i18n(true);

    // ============================================================
    // 十、JSON 多语言注册效果（trade_service 子系统/模块名称三语言切换）
    // 说明：trade_service_errors.json 中为子系统/模块名称配置了
    //       zh_CN/en_US/ja_JP 三语言，生成器自动注册到
    //       subsystem_module_catalog_t。切换 output_locale 即可输出
    //       对应语言的子系统/模块名称。
    // 注意：第七章 catalog.clear() 清空了全局 catalog（含静态注册数据），
    //       此处重新注册 trade_service 订单模块的三语言名称以演示效果。
    // ============================================================
    catalog.register_subsystem_module(i18n::locale_t::zh_CN, 101, 1, "交易服务", "订单模块");
    catalog.register_subsystem_module(i18n::locale_t::en_US, 101, 1, "Trade Service", "Order Module");
    catalog.register_subsystem_module(i18n::locale_t::ja_JP, 101, 1, "取引サービス", "注文モジュール");

    section("10.1 JSON 多语言注册 - zh_CN（交易服务/订单模块）");
    i18n_config_t::set_enable_i18n(true);
    i18n_config_t::set_output_locale(i18n::locale_t::zh_CN);
    error_context_t ctx_trade_zh{biz::trade_errors::ERR_ORDER_NOT_FOUND, "测试消息"};
    std::cout << ctx_trade_zh.to_string() << std::endl;

    section("10.2 JSON 多语言注册 - en_US（Trade Service/Order Module）");
    i18n_config_t::set_output_locale(i18n::locale_t::en_US);
    error_context_t ctx_trade_en{biz::trade_errors::ERR_ORDER_NOT_FOUND, "test message"};
    std::cout << ctx_trade_en.to_string() << std::endl;

    section("10.3 JSON 多语言注册 - ja_JP（取引サービス/注文モジュール）");
    i18n_config_t::set_output_locale(i18n::locale_t::ja_JP);
    error_context_t ctx_trade_ja{biz::trade_errors::ERR_ORDER_NOT_FOUND, "テストメッセージ"};
    std::cout << ctx_trade_ja.to_string() << std::endl;
    i18n_config_t::clear_output_locale();

    return 0;
}
