#include "error_system/core/error_registry.h"
#include "error_system/domain/system_domain.h"
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
using error_system::core::error_builder_t;
using error_system::core::error_code_t;
using error_system::core::error_level_t;
using error_system::core::error_registry_t;
using error_system::domain::system_domain_t;

constexpr std::size_t kBatchSize = 4096;
constexpr int kRegisterRounds = 20;
constexpr int kLookupRounds = 200;
constexpr double kTask3BaselineBatchRegisterNsPerOp = 210.926;
constexpr double kTask3BaselineLookupNsPerOp = 18.526;

struct dataset_t {
    std::vector<error_code_t> codes;
    std::vector<std::string> names;
    std::vector<std::string> descriptions;
    std::vector<std::string_view> name_views;
    std::vector<std::string_view> description_views;
};

dataset_t build_dataset() {
    dataset_t data;
    data.codes.reserve(kBatchSize);
    data.names.reserve(kBatchSize);
    data.descriptions.reserve(kBatchSize);
    data.name_views.reserve(kBatchSize);
    data.description_views.reserve(kBatchSize);

    for (std::size_t i = 0; i < kBatchSize; ++i) {
        data.codes.push_back(error_code_t(
            error_level_t::error, system_domain_t::database, 2, static_cast<uint16_t>((i / 128) + 1), static_cast<uint16_t>(i + 1)));
        data.names.push_back("ERR_PERF_REGISTRY_" + std::to_string(i));
        data.descriptions.push_back("registry benchmark item " + std::to_string(i));
    }

    for (std::size_t i = 0; i < kBatchSize; ++i) {
        data.name_views.push_back(data.names[i]);
        data.description_views.push_back(data.descriptions[i]);
    }
    return data;
}

double benchmark_batch_register(const dataset_t& data) {
    auto& registry = error_registry_t::instance();
    registry.set_duplicate_warn_callback(nullptr);

    const auto start = std::chrono::steady_clock::now();
    for (int round = 0; round < kRegisterRounds; ++round) {
        registry.unregister_all();
        registry.register_errors(data.codes, data.name_views, data.description_views);
    }
    const auto end = std::chrono::steady_clock::now();
    const auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return static_cast<double>(total_ns) / static_cast<double>(kRegisterRounds * kBatchSize);
}

double benchmark_lookup(const dataset_t& data, std::size_t& checksum) {
    auto& registry = error_registry_t::instance();
    registry.unregister_all();
    registry.register_errors(data.codes, data.name_views, data.description_views);

    const auto start = std::chrono::steady_clock::now();
    for (int round = 0; round < kLookupRounds; ++round) {
        for (const auto& code : data.codes) {
            const error_system::core::error_metadata_t* info = registry.get_info(code);
            if (info != nullptr) {
                checksum += info->name.size();
            }
        }
    }
    const auto end = std::chrono::steady_clock::now();
    const auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return static_cast<double>(total_ns) / static_cast<double>(kLookupRounds * kBatchSize);
}
}  // namespace

int main() {
    const dataset_t data = build_dataset();

    const double batch_register_ns_per_op = benchmark_batch_register(data);

    std::size_t checksum = 0;
    const double lookup_ns_per_op = benchmark_lookup(data, checksum);

    std::cout << "benchmark=error_registry\n";
    std::cout << "batch_size=" << kBatchSize << "\n";
    std::cout << "register_rounds=" << kRegisterRounds << "\n";
    std::cout << "lookup_rounds=" << kLookupRounds << "\n";
    std::cout << "checksum=" << checksum << "\n";
    std::cout << "batch_register_ns_per_op=" << batch_register_ns_per_op << "\n";
    std::cout << "lookup_ns_per_op=" << lookup_ns_per_op << "\n";
    std::cout << "batch_register_ratio_to_baseline="
              << (batch_register_ns_per_op / kTask3BaselineBatchRegisterNsPerOp) << "\n";
    std::cout << "lookup_ratio_to_baseline=" << (lookup_ns_per_op / kTask3BaselineLookupNsPerOp) << "\n";
    std::cout << "task3_target_passed="
              << ((batch_register_ns_per_op < (kTask3BaselineBatchRegisterNsPerOp * 0.60) &&
                   lookup_ns_per_op < (kTask3BaselineLookupNsPerOp * 0.85))
                      ? "true"
                      : "false")
              << "\n";
    return 0;
}
