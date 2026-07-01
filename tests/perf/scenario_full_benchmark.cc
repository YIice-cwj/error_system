/**
 * @file scenario_full_benchmark.cc
 * @brief 场景五：全特性开启（栈追踪 + 源位置 + 短文件名 + 插件）
 * @details 开启所有可选运行时特性，测量 error_context_t 在"全观测"模式下的开销。
 *          此为最坏情况性能基线：构造时捕获堆栈、记录源位置、缩写文件名、通知插件。
 *          序列化阶段也会输出完整的位置信息与堆栈帧。
 * @note 短文件名开启后，构造时会额外执行路径截断操作（仅一次，影响较小）。
 * @author yiice
 * @version 1.0.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */

#include <atomic>

#include "perf_common.h"

namespace {

    using perf::error_context_t;
    using perf::i_error_plugin_t;

    /// @brief 最小开销插件：仅做原子自增
    class noop_plugin_t : public i_error_plugin_t {
    public:
        [[nodiscard]] std::string_view name() const noexcept override { return "perf_noop"; }

        void on_error(const error_context_t&) noexcept override { counter_.fetch_add(1); }

        [[nodiscard]] std::size_t count() const noexcept { return counter_.load(); }

    private:
        std::atomic<std::size_t> counter_{0};
    };

}  // namespace

int main() {
    using namespace perf;

    // ===== 场景开关设置：全特性开启 =====
    feature_flags_t::set_enable_stacktrace(true);
    feature_flags_t::set_enable_source_location(true);
    feature_flags_t::set_enable_short_filename(true);
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);

    const error_code_t code = prepare_registry();

    noop_plugin_t plugin;
    plugin_registry_t::instance().register_plugin_ref(plugin);

    std::cout << "===== 性能基准测试：场景五 全特性开启 =====";
    std::cout << "\n  栈追踪:   开启 (编译期=" << (feature_flags_t::STACKTRACE_ENABLED ? "ON" : "OFF") << ")";
    std::cout << "\n  源位置:   开启 (编译期=" << (feature_flags_t::LOCATION_ENABLED ? "ON" : "OFF") << ")";
    std::cout << "\n  短文件名: 开启";
    std::cout << "\n  插件:     1 个 noop 插件（同步通知）";

    std::vector<benchmark_item_t> items;
    items.push_back({"1. 构造 context", benchmark_create(code)});
    items.push_back({"2. to_string 序列化", benchmark_to_string(code)});
    items.push_back({"3. to_json 序列化", benchmark_to_json(code)});
    items.push_back({"4. to_binary 序列化", benchmark_to_binary(code)});
    items.push_back({"5. 拷贝构造", benchmark_copy(code)});
    items.push_back({"6. 移动构造", benchmark_move(code)});

    print_report("场景五 全特性（栈追踪 + 位置 + 短文件名 + 插件）", items);

    std::cout << "\n  插件回调总次数: " << plugin.count() << " (应等于构造次数)\n";

    // 恢复默认，避免影响后续测试
    feature_flags_t::set_enable_stacktrace(false);
    feature_flags_t::set_enable_source_location(false);
    feature_flags_t::set_enable_short_filename(false);
    plugin_registry_t::instance().clear();
    return 0;
}
