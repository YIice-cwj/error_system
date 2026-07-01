/**
 * @file scenario_plugin_benchmark.cc
 * @brief 场景三：仅开启插件（关闭栈追踪、关闭源位置）
 * @details 注册一个最小开销插件（仅原子计数），测量插件通知对 error_context_t
 *          构造的影响。插件在 error_context_t 构造时通过 RCU 快照无锁读取并回调。
 *          本场景使用同步通知模式（sync），即构造时立即调用插件 on_error。
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

    /// @brief 最小开销插件：仅做原子自增，用于测量通知框架本身的开销
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

    // ===== 场景开关设置：关闭栈追踪/位置，注册插件 =====
    feature_flags_t::set_enable_stacktrace(false);
    feature_flags_t::set_enable_source_location(false);
    feature_flags_t::set_enable_short_filename(false);
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);

    const error_code_t code = prepare_registry();

    // 注册一个空操作插件（同步通知模式）
    noop_plugin_t plugin;
    plugin_registry_t::instance().register_plugin_ref(plugin);

    std::cout << "===== 性能基准测试：场景三 仅插件（同步）=====";
    std::cout << "\n  栈追踪:   关闭";
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

    print_report("场景三 仅插件（栈追踪关 / 同步通知）", items);

    std::cout << "\n  插件回调总次数: " << plugin.count() << " (应等于构造次数)\n";

    plugin_registry_t::instance().clear();
    return 0;
}
