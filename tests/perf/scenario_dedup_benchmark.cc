/**
 * @file scenario_dedup_benchmark.cc
 * @brief 场景七：去重采样器性能基准
 * @details 测量 error_dedup_sampler_t 的判定性能，覆盖五种配置组合：
 *          1. 全部放行（dedup=0, rate=1.0）：仅原子计数，最快路径
 *          2. 纯去重（dedup=1000ms, rate=1.0）：同 code 窗口内只放行首次
 *          3. 纯采样（dedup=0, rate=0.1）：放行 10%
 *          4. 去重+采样叠加（dedup=1000ms, rate=0.1）：双关判定
 *          5. 多码去重（32 个不同 code 轮询，模拟真实业务）
 *          采样器先过采样（原子，廉价），再过去重（需 mutex）。
 * @author yiice
 * @version 1.0.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */

#include "perf_common.h"

int main() {
    using namespace perf;

    feature_flags_t::set_enable_stacktrace(false);
    feature_flags_t::set_enable_source_location(false);
    feature_flags_t::set_enable_short_filename(false);

    const error_code_t code = prepare_registry();

    // 准备多码去重的 32 个不同 code
    std::vector<error_code_t> multi_codes;
    multi_codes.reserve(32);
    for (int i = 0; i < 32; ++i) {
        multi_codes.emplace_back(
            error_level_t::error, system_domain_t::database,
            subsystem_id_t{1}, module_id_t{1},
            error_number_t{static_cast<uint16_t>(500 + i)});
    }

    std::cout << "===== 性能基准测试：场景七 去重采样器 =====";
    std::cout << "\n  去重策略: 时间窗口 + 速率采样";
    std::cout << "\n  同步原语: mutex（去重） + atomic（采样）";

    std::vector<benchmark_item_t> items;
    items.push_back({"1. 全部放行 (rate=1.0)", benchmark_dedup_all_forward(
        []() -> error_dedup_sampler_t& {
            static error_dedup_sampler_t s;
            s.set_dedup_window_ms(0);
            s.set_sample_rate(1.0);
            return s;
        }(), code)});

    items.push_back({"2. 纯去重 (1000ms)", benchmark_dedup_window_only(
        []() -> error_dedup_sampler_t& {
            static error_dedup_sampler_t s;
            s.set_dedup_window_ms(1000);
            s.set_sample_rate(1.0);
            return s;
        }(), code)});

    items.push_back({"3. 纯采样 (rate=0.1)", benchmark_dedup_sample_only(
        []() -> error_dedup_sampler_t& {
            static error_dedup_sampler_t s;
            s.set_dedup_window_ms(0);
            s.set_sample_rate(0.1);
            return s;
        }(), code)});

    items.push_back({"4. 去重+采样叠加", benchmark_dedup_combined(
        []() -> error_dedup_sampler_t& {
            static error_dedup_sampler_t s;
            s.set_dedup_window_ms(1000);
            s.set_sample_rate(0.1);
            return s;
        }(), code)});

    items.push_back({"5. 多码去重 (32 codes)", benchmark_dedup_multi_codes(
        []() -> error_dedup_sampler_t& {
            static error_dedup_sampler_t s;
            s.set_dedup_window_ms(1000);
            s.set_sample_rate(1.0);
            return s;
        }(), multi_codes)});

    print_report("场景七 去重采样器", items);
    return 0;
}
