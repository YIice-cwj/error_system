#include <chrono>
#include <cstddef>
#include <iostream>

#include "error_system/config/error_config.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_registry.h"
#include "error_system/domain/system_domain.h"
#include "error_system/i18n/subsystem_module_catalog.h"
#include "error_system/plugin/plugin_registry.h"

namespace {
using error_system::config::feature_flags_t;
using error_system::core::error_builder_t;
using error_system::core::error_code_t;
using error_system::core::error_context_t;
using error_system::core::error_level_t;
using error_system::core::error_number_t;
using error_system::core::error_registry_t;
using error_system::core::located_code_t;
using error_system::core::module_id_t;
using error_system::core::subsystem_id_t;
using error_system::domain::system_domain_t;
using error_system::i18n::subsystem_module_catalog_t;
using error_system::plugin::plugin_registry_t;

constexpr int WARMUP_ITERATIONS = 10000;
constexpr int MEASURE_ITERATIONS = 200000;
constexpr double TASK2_BASELINE_NS_PER_OP = 4591.1;

error_code_t make_benchmark_code() noexcept {
    return error_code_t(error_level_t::error, system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
}

void prepare_registry() noexcept {
    auto& registry = error_registry_t::instance();
    registry.unregister_all();
    registry.set_duplicate_warn_callback(nullptr);

    auto& catalog = subsystem_module_catalog_t::instance();
    catalog.clear();

    const auto code = make_benchmark_code();
    catalog.register_subsystem_module(code.get_subsys(), code.get_module(), "perf", "error_context");
    registry.register_error(code, "ERR_PERF_CONTEXT", "performance benchmark context");
}

std::size_t run_loop(int iterations) noexcept {
    const auto code = make_benchmark_code();
    std::size_t checksum = 0;
    for (int i = 0; i < iterations; ++i) {
        error_context_t context(located_code_t{code}, "benchmark {}", i);
        checksum += context.message.size();
        checksum += static_cast<std::size_t>(context.get_code().get_number());
    }
    return checksum;
}

double measure_ns_per_op() noexcept {
    run_loop(WARMUP_ITERATIONS);

    const auto start = std::chrono::steady_clock::now();
    const std::size_t checksum = run_loop(MEASURE_ITERATIONS);
    const auto end = std::chrono::steady_clock::now();

    std::cout << "checksum=" << checksum << "\n";
    const auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return static_cast<double>(total_ns) / static_cast<double>(MEASURE_ITERATIONS);
}

void print_flag(const char* name, bool enabled) {
    std::cout << name << "=" << (enabled ? "ON" : "OFF") << "\n";
}
}  // namespace

int main() {
    plugin_registry_t::instance().clear();
    prepare_registry();

    std::cout << "benchmark=error_context_create\n";
    std::cout << "iterations=" << MEASURE_ITERATIONS << "\n";
    print_flag("stacktrace", feature_flags_t::is_stacktrace_enabled());
    print_flag("validation", feature_flags_t::is_validation_enabled());
    print_flag("location", feature_flags_t::is_source_location_enabled());

    const double default_ns_per_op = measure_ns_per_op();

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
    feature_flags_t::set_enable_stacktrace(false);
#endif
    const double fast_path_ns_per_op = measure_ns_per_op();
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
    feature_flags_t::set_enable_stacktrace(true);
#endif

    const double fast_path_ratio_to_baseline = fast_path_ns_per_op / TASK2_BASELINE_NS_PER_OP;
    std::cout << "default_ns_per_op=" << default_ns_per_op << "\n";
    std::cout << "fast_path_ns_per_op=" << fast_path_ns_per_op << "\n";
    std::cout << "fast_path_ratio_to_baseline=" << fast_path_ratio_to_baseline << "\n";
    std::cout << "task2_target_passed=" << (fast_path_ratio_to_baseline < 0.70 ? "true" : "false") << "\n";
    return 0;
}
