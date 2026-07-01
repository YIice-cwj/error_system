/**
 * @file scenario_stacktrace_plugin_benchmark.cc
 * @brief 场景四：同时开启栈追踪与插件（关闭源位置）
 * @details 叠加两个最昂贵的可选特性：构造时既捕获堆栈又通知插件。
 *          代表"全观测"但不含源位置的中间配置，测量特性叠加后的综合开销。
 *          堆栈捕获与插件通知在构造时串行执行，开销近似可加。
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

    // ===== 场景开关设置：开启栈追踪 + 注册插件 =====
    feature_flags_t::set_enable_stacktrace(true);
    feature_flags_t::set_enable_source_location(false);
    feature_flags_t::set_enable_short_filename(false);
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);

    const error_code_t code = prepare_registry();

    noop_plugin_t plugin;
    plugin_registry_t::instance().register_plugin_ref(plugin);

    std::cout << "===== 性能基准测试：场景四 栈追踪 + 插件 =====";
    std::cout << "\n  栈追踪:   开启 (编译期=" << (feature_flags_t::STACKTRACE_ENABLED ? "ON" : "OFF") << ")";
    std::cout << "\n  源位置:   关闭";
    std::cout << "\n  短文件名: 关闭";
    std::cout << "\n  插件:     1 个 noop 插件（同步通知）";

    std::vector<benchmark_item_t> items;
    items.push_back({"1. 构造 context", benchmark_create(code)});
    items.push_back({"2. to_string 序列化", benchmark_to_string(code)});
    items.push_back({"3. to_json 序列化", benchmark_to_json(code)});
    items.push_back({"4. to_binary 序列化", benchmark_to_binary(code)});
    items.push_back({"5. 拷贝构造", benchmark_copy(code)});
    items.push_back({"6. 移动构造", benchmark_move(code)});

    print_report("场景四 栈追踪 + 插件（同步通知）", items);

    std::cout << "\n  插件回调总次数: " << plugin.count() << " (应等于构造次数)\n";

    feature_flags_t::set_enable_stacktrace(false);
    plugin_registry_t::instance().clear();
    return 0;
}
