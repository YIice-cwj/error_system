#include "error_system/core/error_context.h"
#include "error_system/plugin/i_error_plugin.h"
#include "error_system/plugin/plugin_registry.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>

namespace {
using error_system::core::error_context_t;
using error_system::plugin::i_error_plugin_t;
using error_system::plugin::plugin_registry_t;

constexpr int kPluginCount = 4;
constexpr int kWarmupIterations = 20000;
constexpr int kMeasureIterations = 200000;

class benchmark_plugin_t : public i_error_plugin_t {
    public:
    explicit benchmark_plugin_t(const char* plugin_name) noexcept : name_(plugin_name) {}

    std::string_view name() const noexcept override { return name_; }

    void on_error(const error_context_t&) noexcept override { counter_.fetch_add(1, std::memory_order_relaxed); }

    std::size_t count() const noexcept { return counter_.load(std::memory_order_relaxed); }

    private:
    const char* name_;
    std::atomic<std::size_t> counter_{0};
};

std::size_t run_loop(const error_context_t& context, int iterations) noexcept {
    auto& registry = plugin_registry_t::instance();
    for (int i = 0; i < iterations; ++i) {
        registry.notify_error(context);
    }
    return static_cast<std::size_t>(iterations);
}
}  // namespace

int main() {
    auto& registry = plugin_registry_t::instance();
    registry.clear();

    benchmark_plugin_t plugins[] = {
        benchmark_plugin_t("perf_plugin_1"),
        benchmark_plugin_t("perf_plugin_2"),
        benchmark_plugin_t("perf_plugin_3"),
        benchmark_plugin_t("perf_plugin_4"),
    };

    for (auto& plugin : plugins) {
        registry.register_plugin(&plugin);
    }

    error_context_t context;
    context.message = "plugin benchmark";

    run_loop(context, kWarmupIterations);

    const auto start = std::chrono::steady_clock::now();
    const std::size_t iterations = run_loop(context, kMeasureIterations);
    const auto end = std::chrono::steady_clock::now();

    std::size_t total_callbacks = 0;
    for (const auto& plugin : plugins) {
        total_callbacks += plugin.count();
    }

    const auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    const auto ns_per_op = static_cast<double>(total_ns) / static_cast<double>(iterations);

    std::cout << "benchmark=plugin_registry_notify\n";
    std::cout << "plugins=" << kPluginCount << "\n";
    std::cout << "iterations=" << iterations << "\n";
    std::cout << "callbacks=" << total_callbacks << "\n";
    std::cout << "ns_per_op=" << ns_per_op << "\n";

    constexpr double baseline_ns_per_op = 24.721;
    const double ratio = ns_per_op / baseline_ns_per_op;
    std::cout << "ratio_to_baseline=" << ratio << "\n";
    std::cout << "task4_target_passed=" << (ratio < 0.50 ? "true" : "false") << "\n";

    registry.clear();
    return 0;
}
