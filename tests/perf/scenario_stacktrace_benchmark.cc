/**
 * @file scenario_stacktrace_benchmark.cc
 * @brief 场景二：仅开启栈追踪（关闭源位置、无插件）
 * @details 开启运行时栈追踪，测量堆栈捕获对 error_context_t 构造的影响。
 *          栈追踪在构造 error_context_t 时调用 backtrace 收集调用栈，
 *          是最昂贵的可选特性之一。序列化阶段也会因 stack_frames 非空而增加成本。
 * @note 若编译期未启用栈追踪（ERROR_SYSTEM_ENABLE_STACKTRACE=OFF），
 *       运行时 set_enable_stacktrace(true) 为无操作，本场景将退化为基线。
 * @author yiice
 * @version 1.0.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */

#include "perf_common.h"

int main() {
    using namespace perf;

    // ===== 场景开关设置：仅开启栈追踪 =====
    feature_flags_t::set_enable_stacktrace(true);
    feature_flags_t::set_enable_source_location(false);
    feature_flags_t::set_enable_short_filename(false);
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);

    const error_code_t code = prepare_registry();

    std::cout << "===== 性能基准测试：场景二 仅栈追踪 =====";
    std::cout << "\n  栈追踪:   开启 (编译期=" << (feature_flags_t::STACKTRACE_ENABLED ? "ON" : "OFF") << ")";
    std::cout << "\n  源位置:   关闭";
    std::cout << "\n  短文件名: 关闭";
    std::cout << "\n  插件:     无";

    std::vector<benchmark_item_t> items;
    items.push_back({"1. 构造 context", benchmark_create(code)});
    items.push_back({"2. to_string 序列化", benchmark_to_string(code)});
    items.push_back({"3. to_json 序列化", benchmark_to_json(code)});
    items.push_back({"4. to_binary 序列化", benchmark_to_binary(code)});
    items.push_back({"5. 拷贝构造", benchmark_copy(code)});
    items.push_back({"6. 移动构造", benchmark_move(code)});

    print_report("场景二 仅栈追踪（位置关 / 无插件）", items);

    // 恢复默认，避免影响后续测试
    feature_flags_t::set_enable_stacktrace(false);
    return 0;
}
