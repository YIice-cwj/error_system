/**
 * @file scenario_router_benchmark.cc
 * @brief 场景六：错误路由插件性能基准
 * @details 测量 error_router_plugin_t 的分发性能，覆盖四种命中路径：
 *          1. 按错误码命中（最高优先级，O(1) 哈希查找）
 *          2. 按模块组命中（次优先级，未命中具体码时回退）
 *          3. 按系统域命中（最低优先级，未命中码与模块组时回退）
 *          4. 完全未命中（无任何处理函数，仍走完三级查找）
 *          另测 register_handler_by_code 的注册性能。
 *          路由查找使用 shared_mutex 读取，读多写少场景下开销应极小。
 * @author yiice
 * @version 1.0.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */

#include "perf_common.h"

int main() {
    using namespace perf;

    // 关闭栈追踪/位置，避免污染路由分发测量
    feature_flags_t::set_enable_stacktrace(false);
    feature_flags_t::set_enable_source_location(false);
    feature_flags_t::set_enable_short_filename(false);

    const error_code_t code = prepare_registry();
    auto& router = error_router_plugin_t::instance();

    // 按错误码注册处理函数（命中路径 1）
    auto handler = [](const error_context_t&) noexcept {};
    router.register_handler_by_code(code, handler);
    // 按模块组注册（命中路径 2 使用不同 code，相同模块组）
    const error_code_t code_module_only(
        error_level_t::error, system_domain_t::database,
        subsystem_id_t{1}, module_id_t{1}, error_number_t{2});
    router.register_handler_by_module_group_id(code_module_only.get_module_group_id(), handler);
    // 按系统域注册（命中路径 3 使用不同模块组，相同域）
    const error_code_t code_domain_only(
        error_level_t::error, system_domain_t::database,
        subsystem_id_t{2}, module_id_t{2}, error_number_t{1});
    router.register_handler_by_domain(system_domain_t::database, handler);
    // 完全未命中使用其他域
    const error_code_t code_miss(
        error_level_t::error, system_domain_t::application,
        subsystem_id_t{3}, module_id_t{3}, error_number_t{1});

    std::cout << "===== 性能基准测试：场景六 错误路由插件 =====";
    std::cout << "\n  路由策略: 三级查找（错误码 → 模块组 → 系统域 → 未命中）";
    std::cout << "\n  同步原语: shared_mutex（读多写少）";

    std::vector<benchmark_item_t> items;
    items.push_back({"1. 按错误码命中分发", benchmark_router_dispatch_by_code(router, code)});
    items.push_back({"2. 按模块组命中分发", benchmark_router_dispatch_by_module_group(router, code_module_only)});
    items.push_back({"3. 按系统域命中分发", benchmark_router_dispatch_by_domain(router, code_domain_only)});
    items.push_back({"4. 完全未命中分发", benchmark_router_dispatch_miss(router, code_miss)});
    items.push_back({"5. 注册处理函数", benchmark_router_register_handler(router)});

    print_report("场景六 错误路由插件", items);
    return 0;
}
