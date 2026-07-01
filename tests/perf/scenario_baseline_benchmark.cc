/**
 * @file scenario_baseline_benchmark.cc
 * @brief 场景一：基线（关闭栈追踪、关闭源位置、无插件）
 * @details 关闭所有可选运行时特性，测量 error_context_t 的最小开销路径。
 *          此场景代表"最热路径"——无堆栈捕获、无源位置记录、无插件通知，
 *          是评估 error_context_t 基础构造/序列化/拷贝/移动成本的基准。
 * @author yiice
 * @version 1.0.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */

#include "perf_common.h"

int main() {
    using namespace perf;

    // ===== 场景开关设置：全部关闭 =====
    feature_flags_t::set_enable_stacktrace(false);
    feature_flags_t::set_enable_source_location(false);
    feature_flags_t::set_enable_short_filename(false);
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);

    const error_code_t code = prepare_registry();

    std::cout << "===== 性能基准测试：场景一 基线（全关）=====";
    std::cout << "\n  栈追踪:   关闭";
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

    print_report("场景一 基线（栈追踪关 / 位置关 / 无插件）", items);
    return 0;
}
