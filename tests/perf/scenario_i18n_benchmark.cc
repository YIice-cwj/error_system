/**
 * @file scenario_i18n_benchmark.cc
 * @brief 场景八：i18n 翻译模块性能基准
 * @details 测量 i18n_t 多语言消息目录与 subsystem_module_catalog_t 子系统/模块目录性能。
 *          覆盖以下热点路径：
 *          1. 单条消息注册（register_message，写锁）
 *          2. 批量消息注册（register_messages，带 reserve 预分配）
 *          3. 查询命中主 locale（get_message，读锁，无回退）
 *          4. 查询回退默认 locale（主 locale 未命中，触发 fallback）
 *          5. 按输出 locale 查询（get_message(code)，含 resolve_output_locale）
 *          6. 查询完全未命中（返回空串）
 *          7. 子系统/模块目录注册（catalog register）
 *          8. 子系统/模块目录查询命中（catalog get hit）
 *          9. 子系统/模块目录查询回退（catalog get fallback）
 *          10. locale_t 字符串转换（to_string / from_string）
 *          读多写少场景使用 shared_mutex，查询路径应极快。
 * @note 不使用 `using namespace perf`，避免 perf::locale_t 与系统 <locale.h> 的 locale_t 冲突。
 * @author yiice
 * @version 1.0.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */

#include "perf_common.h"

int main() {
    using perf::benchmark_catalog_get_fallback;
    using perf::benchmark_catalog_get_hit;
    using perf::benchmark_catalog_register;
    using perf::benchmark_i18n_get_message_by_output_locale;
    using perf::benchmark_i18n_get_message_fallback;
    using perf::benchmark_i18n_get_message_hit;
    using perf::benchmark_i18n_get_message_miss;
    using perf::benchmark_i18n_register_batch;
    using perf::benchmark_i18n_register_single;
    using perf::benchmark_item_t;
    using perf::benchmark_locale_conversion;
    using perf::error_code_t;
    using perf::error_level_t;
    using perf::error_number_t;
    using perf::i18n_config_t;
    using perf::i18n_t;
    using perf::module_id_t;
    using perf::prepare_registry;
    using perf::print_report;
    using perf::subsystem_id_t;
    using perf::subsystem_module_catalog_t;
    using perf::system_domain_t;

    perf::feature_flags_t::set_enable_stacktrace(false);
    perf::feature_flags_t::set_enable_source_location(false);
    perf::feature_flags_t::set_enable_short_filename(false);

    const error_code_t code = prepare_registry();

    // 准备 i18n 环境：启用 i18n，默认/输出 locale 均为 zh_CN
    // 使用完整命名空间路径避免与系统 locale_t 冲突
    i18n_config_t::set_enable_i18n(true);
    i18n_config_t::set_default_locale(error_system::i18n::locale_t::zh_CN);
    i18n_config_t::set_output_locale(error_system::i18n::locale_t::zh_CN);

    // 注册基准 code 的消息（用于查询命中测试）
    auto& i18n = i18n_t::instance();
    i18n.clear_all();
    i18n.register_message(error_system::i18n::locale_t::zh_CN, code, "数据库操作失败");
    // 不注册 en_US，用于触发 fallback 路径

    // 准备 catalog 环境
    auto& catalog = subsystem_module_catalog_t::instance();
    catalog.clear();
    catalog.register_subsystem_module(error_system::i18n::locale_t::zh_CN, 1, 1, "数据库子系统", "连接模块");
    // 不注册 ja_JP，用于触发 fallback 路径

    // 未注册消息的 code（用于查询未命中测试）
    const error_code_t code_unregistered(
        error_level_t::error, system_domain_t::application,
        subsystem_id_t{99}, module_id_t{99}, error_number_t{99});

    std::cout << "===== 性能基准测试：场景八 i18n 翻译模块 =====";
    std::cout << "\n  i18n 总开关:   开启";
    std::cout << "\n  默认 locale:   " << error_system::i18n::to_string(i18n_config_t::get_default_locale());
    std::cout << "\n  输出 locale:   " << error_system::i18n::to_string(i18n_config_t::resolve_output_locale());
    std::cout << "\n  同步原语:      shared_mutex（读多写少）";

    std::vector<benchmark_item_t> items;
    items.push_back({"1. 单条消息注册", benchmark_i18n_register_single()});
    // 重新注册基准消息（上面单条注册测试清空了目录）
    i18n.clear_all();
    i18n.register_message(error_system::i18n::locale_t::zh_CN, code, "数据库操作失败");

    items.push_back({"2. 批量消息注册(1024)", benchmark_i18n_register_batch()});
    // 重新注册
    i18n.clear_all();
    i18n.register_message(error_system::i18n::locale_t::zh_CN, code, "数据库操作失败");

    items.push_back({"3. 查询命中主 locale", benchmark_i18n_get_message_hit(code)});
    items.push_back({"4. 查询回退默认 locale", benchmark_i18n_get_message_fallback(code)});
    items.push_back({"5. 按输出 locale 查询", benchmark_i18n_get_message_by_output_locale(code)});
    items.push_back({"6. 查询完全未命中", benchmark_i18n_get_message_miss(code_unregistered)});
    items.push_back({"7. 目录注册", benchmark_catalog_register()});

    // 重新注册 catalog（上面注册测试清空了目录）
    catalog.clear();
    catalog.register_subsystem_module(error_system::i18n::locale_t::zh_CN, 1, 1, "数据库子系统", "连接模块");

    items.push_back({"8. 目录查询命中", benchmark_catalog_get_hit(1, 1)});
    items.push_back({"9. 目录查询回退", benchmark_catalog_get_fallback(1, 1)});
    items.push_back({"10. locale 字符串转换", benchmark_locale_conversion()});

    print_report("场景八 i18n 翻译模块", items);
    return 0;
}
