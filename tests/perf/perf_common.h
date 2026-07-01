#pragma once
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "error_system/config/feature_flags.h"
#include "error_system/config/i18n_config.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_registry.h"
#include "error_system/domain/system_domain.h"
#include "error_system/i18n/i18n.h"
#include "error_system/i18n/locale.h"
#include "error_system/i18n/subsystem_module_catalog.h"
#include "error_system/plugin/error_dedup_sampler.h"
#include "error_system/plugin/error_router_plugin.h"
#include "error_system/plugin/i_error_plugin.h"
#include "error_system/plugin/plugin_registry.h"

/**
 * @file perf_common.h
 * @brief 性能基准测试共享工具
 * @details 提供计时器、注册表/目录准备、各操作基准函数与中文报告输出。
 *          各场景文件通过设置 feature_flags_t 开关组合后调用本头文件中的基准函数，
 *          避免重复代码（DRY），同时保证各场景测量口径一致，便于横向对比。
 * @note 所有基准函数返回 ns/op（每次操作纳秒数），值越小越快。
 * @author yiice
 * @version 1.1.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */
namespace perf {

    using error_system::config::feature_flags_t;
    using error_system::config::i18n_config_t;
    using error_system::core::error_context_t;
    using error_system::core::error_code_t;
    using error_system::core::error_level_t;
    using error_system::core::error_number_t;
    using error_system::core::error_registry_t;
    using error_system::core::located_code_t;
    using error_system::core::module_id_t;
    using error_system::core::module_group_id_t;
    using error_system::core::subsystem_id_t;
    using error_system::domain::system_domain_t;
    using error_system::i18n::i18n_t;
    using error_system::i18n::locale_t;
    using error_system::i18n::subsystem_module_catalog_t;
    using error_system::plugin::error_dedup_sampler_t;
    using error_system::plugin::error_router_plugin_t;
    using error_system::plugin::i_error_plugin_t;
    using error_system::plugin::plugin_registry_t;

    /// @brief 预热迭代次数，触发缓存预热与分支预测器稳定
    constexpr int WARMUP_ITERATIONS = 10000;

    /// @brief 正式测量迭代次数，足以稳定均值
    constexpr int MEASURE_ITERATIONS = 100000;

    /**
     * @brief 计时器，基于 steady_clock（单调时钟，不受系统时间调整影响）
     */
    class stopwatch_t {
    public:
        void start() noexcept { start_ = std::chrono::steady_clock::now(); }

        [[nodiscard]] int64_t elapsed_ns() const noexcept {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now() - start_)
                .count();
        }

    private:
        std::chrono::steady_clock::time_point start_{};
    };

    /**
     * @brief 准备注册表与子系统/模块目录，返回基准用错误码
     * @details 清空全局注册表与目录后，注册一个固定的基准错误码，
     *          保证各场景测量起点一致。插件注册表也一并清空。
     * @return error_code_t 基准用错误码（database 域, subsystem=1, module=1, number=1）
     */
    inline error_code_t prepare_registry() noexcept {
        plugin_registry_t::instance().clear();
        auto& registry = error_registry_t::instance();
        registry.unregister_all();
        registry.set_duplicate_warn_callback(nullptr);

        auto& catalog = subsystem_module_catalog_t::instance();
        catalog.clear();

        const error_code_t code(error_level_t::error, system_domain_t::database,
                                subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog.register_subsystem_module(code.get_subsys(), code.get_module(), "perf", "benchmark");
        registry.register_error(code, "ERR_PERF_BENCH", "性能基准测试错误码");
        return code;
    }

    /**
     * @brief 测量 error_context_t 构造性能（含格式化消息）
     * @param code 基准错误码
     * @return double ns/op（每次构造纳秒数）
     */
    inline double benchmark_create(const error_code_t& code) noexcept {
        // 预热
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            error_context_t ctx(code, "bench {}", i);
            (void)ctx.message.size();
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            error_context_t ctx(code, "bench {}", i);
            checksum += ctx.message.size();
            checksum += static_cast<std::size_t>(ctx.get_code().get_number());
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[create] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量 to_string 文本序列化性能
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_to_string(const error_code_t& code) noexcept {
        const error_context_t ctx(code, "序列化基准测试消息");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)ctx.to_string();
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            checksum += ctx.to_string().size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[to_string] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量 to_json 序列化性能
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_to_json(const error_code_t& code) noexcept {
        const error_context_t ctx(code, "JSON序列化基准");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)ctx.to_json();
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            checksum += ctx.to_json().size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[to_json] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量 to_binary 序列化性能
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_to_binary(const error_code_t& code) noexcept {
        const error_context_t ctx(code, "二进制序列化基准");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)ctx.to_binary();
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            checksum += ctx.to_binary().size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[to_binary] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量拷贝构造性能（深拷贝 payload/cause/stack）
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_copy(const error_code_t& code) noexcept {
        const error_context_t src(code, "拷贝基准源对象");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            error_context_t dst(src);
            (void)dst.message.size();
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            error_context_t dst(src);
            checksum += dst.message.size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[copy] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量移动构造性能
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_move(const error_code_t& code) noexcept {
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            error_context_t src(code, "移动基准源对象");
            error_context_t dst(std::move(src));
            (void)dst.message.size();
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            error_context_t src(code, "移动基准源对象");
            error_context_t dst(std::move(src));
            checksum += dst.message.size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[move] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测试项描述
     */
    struct benchmark_item_t {
        const char* name;
        double ns_per_op;
    };

    /**
     * @brief 打印中文性能报告（对齐表格）
     * @param scenario_name 场景名称（中文）
     * @param items 各测试项结果
     */
    inline void print_report(const char* scenario_name, const std::vector<benchmark_item_t>& items) noexcept {
        std::cout << "\n========================================================\n";
        std::cout << "  场景: " << scenario_name << "\n";
        std::cout << "  迭代次数: " << MEASURE_ITERATIONS << " (预热 " << WARMUP_ITERATIONS << ")\n";
        std::cout << "--------------------------------------------------------\n";
        std::cout << "  测试项目                  |   ns/op\n";
        std::cout << "--------------------------------------------------------\n";
        for (const auto& item : items) {
            std::printf("  %-24s | %10.2f\n", item.name, item.ns_per_op);
        }
        std::cout << "========================================================\n";
    }

    // ============================================================
    // 路由插件基准函数
    // ============================================================

    /**
     * @brief 测量按错误码路由的分发性能
     * @param router 已注册处理函数的路由插件
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_router_dispatch_by_code(error_router_plugin_t& router,
                                                     const error_code_t& code) noexcept {
        const error_context_t ctx(code, "路由按错误码分发基准");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            router.on_error(ctx);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            router.on_error(ctx);
            checksum += ctx.message.size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[router_by_code] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量按模块组分发的性能（未命中具体码，走模块组回退路径）
     * @param router 路由插件
     * @param code 基准错误码（未注册具体处理函数）
     * @return double ns/op
     */
    inline double benchmark_router_dispatch_by_module_group(error_router_plugin_t& router,
                                                             const error_code_t& code) noexcept {
        const error_context_t ctx(code, "路由按模块组分发基准");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            router.on_error(ctx);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            router.on_error(ctx);
            checksum += ctx.message.size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[router_by_module_group] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量按系统域分发的性能（未命中具体码与模块组，走域回退路径）
     * @param router 路由插件
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_router_dispatch_by_domain(error_router_plugin_t& router,
                                                       const error_code_t& code) noexcept {
        const error_context_t ctx(code, "路由按系统域分发基准");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            router.on_error(ctx);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            router.on_error(ctx);
            checksum += ctx.message.size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[router_by_domain] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量完全未命中（无任何处理函数）的分发性能
     * @param router 路由插件
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_router_dispatch_miss(error_router_plugin_t& router,
                                                  const error_code_t& code) noexcept {
        const error_context_t ctx(code, "路由未命中基准");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            router.on_error(ctx);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            router.on_error(ctx);
            checksum += ctx.message.size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[router_miss] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量 register_handler_by_code 的注册性能
     * @param router 路由插件
     * @return double ns/op
     */
    inline double benchmark_router_register_handler(error_router_plugin_t& router) noexcept {
        // 用不同 number 构造不同 code 避免重复注册静默
        std::vector<error_code_t> codes;
        codes.reserve(MEASURE_ITERATIONS);
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            codes.emplace_back(error_level_t::error, system_domain_t::database,
                               subsystem_id_t{1}, module_id_t{1},
                               error_number_t{static_cast<uint16_t>(1000 + i)});
        }
        auto handler = [](const error_context_t&) noexcept {};
        stopwatch_t sw;
        sw.start();
        for (size_t i = 0; i < static_cast<size_t>(MEASURE_ITERATIONS); ++i) {
            router.register_handler_by_code(codes[i], handler);
        }
        const auto ns = sw.elapsed_ns();
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    // ============================================================
    // 去重采样器基准函数
    // ============================================================

    /**
     * @brief 测量全部放行（无去重无采样）的判定性能
     * @param sampler 已配置为 dedup=0/rate=1.0 的采样器
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_dedup_all_forward(error_dedup_sampler_t& sampler,
                                              const error_code_t& code) noexcept {
        const error_context_t ctx(code, "去重全部放行基准");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)sampler.should_be_forwarded(ctx);
        }
        sampler.reset_stats();
        stopwatch_t sw;
        sw.start();
        std::size_t forwarded = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            if (sampler.should_be_forwarded(ctx)) { ++forwarded; }
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[dedup_all_forward] forwarded=%zu\n", forwarded);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量纯去重（去重窗口 1s，同 code 只放行首次）的判定性能
     * @param sampler 已配置为 dedup=1000ms/rate=1.0 的采样器
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_dedup_window_only(error_dedup_sampler_t& sampler,
                                              const error_code_t& code) noexcept {
        const error_context_t ctx(code, "去重窗口基准");
        sampler.clear_dedup_cache();
        sampler.reset_stats();
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)sampler.should_be_forwarded(ctx);
        }
        sampler.clear_dedup_cache();
        sampler.reset_stats();
        stopwatch_t sw;
        sw.start();
        std::size_t forwarded = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            if (sampler.should_be_forwarded(ctx)) { ++forwarded; }
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[dedup_window] forwarded=%zu\n", forwarded);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量纯采样（rate=0.1，放行 10%）的判定性能
     * @param sampler 已配置为 dedup=0/rate=0.1 的采样器
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_dedup_sample_only(error_dedup_sampler_t& sampler,
                                              const error_code_t& code) noexcept {
        const error_context_t ctx(code, "采样基准");
        sampler.reset_stats();
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)sampler.should_be_forwarded(ctx);
        }
        sampler.reset_stats();
        stopwatch_t sw;
        sw.start();
        std::size_t forwarded = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            if (sampler.should_be_forwarded(ctx)) { ++forwarded; }
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[dedup_sample] forwarded=%zu\n", forwarded);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量去重 + 采样叠加（dedup=1000ms, rate=0.1）的判定性能
     * @param sampler 已配置 dedup+rate 的采样器
     * @param code 基准错误码
     * @return double ns/op
     */
    inline double benchmark_dedup_combined(error_dedup_sampler_t& sampler,
                                           const error_code_t& code) noexcept {
        const error_context_t ctx(code, "去重+采样叠加基准");
        sampler.clear_dedup_cache();
        sampler.reset_stats();
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)sampler.should_be_forwarded(ctx);
        }
        sampler.clear_dedup_cache();
        sampler.reset_stats();
        stopwatch_t sw;
        sw.start();
        std::size_t forwarded = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            if (sampler.should_be_forwarded(ctx)) { ++forwarded; }
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[dedup_combined] forwarded=%zu\n", forwarded);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量多错误码去重（N 个不同 code 轮询，模拟真实场景）的判定性能
     * @param sampler 采样器
     * @param codes 不同 identity 的错误码集合
     * @return double ns/op
     */
    inline double benchmark_dedup_multi_codes(error_dedup_sampler_t& sampler,
                                              const std::vector<error_code_t>& codes) noexcept {
        std::vector<error_context_t> ctxs;
        ctxs.reserve(codes.size());
        for (const auto& c : codes) {
            ctxs.emplace_back(c, "多码去重基准");
        }
        sampler.clear_dedup_cache();
        sampler.reset_stats();
        const size_t n = ctxs.size();
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)sampler.should_be_forwarded(ctxs[static_cast<size_t>(i) % n]);
        }
        sampler.clear_dedup_cache();
        sampler.reset_stats();
        stopwatch_t sw;
        sw.start();
        std::size_t forwarded = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            if (sampler.should_be_forwarded(ctxs[static_cast<size_t>(i) % n])) { ++forwarded; }
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[dedup_multi] forwarded=%zu\n", forwarded);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    // ============================================================
    // i18n 翻译模块基准函数
    // ============================================================

    /**
     * @brief 测量单条消息注册性能
     * @return double ns/op
     */
    inline double benchmark_i18n_register_single() noexcept {
        auto& i18n = i18n_t::instance();
        i18n.clear_all();
        // 用不同 number 构造不同 code 避免覆盖
        std::vector<error_code_t> codes;
        codes.reserve(MEASURE_ITERATIONS);
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            codes.emplace_back(error_level_t::error, system_domain_t::database,
                               subsystem_id_t{1}, module_id_t{1},
                               error_number_t{static_cast<uint16_t>(2000 + i)});
        }
        stopwatch_t sw;
        sw.start();
        for (size_t i = 0; i < static_cast<size_t>(MEASURE_ITERATIONS); ++i) {
            i18n.register_message(locale_t::zh_CN, codes[i], "注册基准消息");
        }
        const auto ns = sw.elapsed_ns();
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量批量注册性能（register_messages，带 reserve 预分配）
     * @return double ns/op
     */
    inline double benchmark_i18n_register_batch() noexcept {
        auto& i18n = i18n_t::instance();
        i18n.clear_all();
        constexpr int BATCH = 1024;
        std::vector<std::pair<error_code_t, std::string_view>> entries;
        entries.reserve(BATCH);
        for (int i = 0; i < BATCH; ++i) {
            entries.emplace_back(
                error_code_t(error_level_t::error, system_domain_t::database,
                             subsystem_id_t{1}, module_id_t{1},
                             error_number_t{static_cast<uint16_t>(3000 + i)}),
                "批量注册消息");
        }
        // 预热
        i18n.clear_all();
        (void)i18n.register_messages(locale_t::zh_CN, entries);
        i18n.clear_all();
        stopwatch_t sw;
        sw.start();
        (void)i18n.register_messages(locale_t::zh_CN, entries);
        const auto ns = sw.elapsed_ns();
        return static_cast<double>(ns) / static_cast<double>(BATCH);
    }

    /**
     * @brief 测量按指定 locale 查询消息性能（命中主 locale，无回退）
     * @param code 已注册消息的错误码
     * @return double ns/op
     */
    inline double benchmark_i18n_get_message_hit(const error_code_t& code) noexcept {
        auto& i18n = i18n_t::instance();
        const error_context_t ctx(code, "查询命中基准");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)i18n.get_message(locale_t::zh_CN, code);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            checksum += i18n.get_message(locale_t::zh_CN, code).size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[i18n_get_hit] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量查询消息性能（未命中主 locale，回退到默认 locale）
     * @param code 已在默认 locale 注册消息的错误码
     * @return double ns/op
     */
    inline double benchmark_i18n_get_message_fallback(const error_code_t& code) noexcept {
        auto& i18n = i18n_t::instance();
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)i18n.get_message(locale_t::en_US, code);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            checksum += i18n.get_message(locale_t::en_US, code).size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[i18n_get_fallback] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量按当前输出 locale 查询消息性能（含 resolve_output_locale）
     * @param code 已注册消息的错误码
     * @return double ns/op
     */
    inline double benchmark_i18n_get_message_by_output_locale(const error_code_t& code) noexcept {
        auto& i18n = i18n_t::instance();
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)i18n.get_message(code);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            checksum += i18n.get_message(code).size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[i18n_get_output] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量查询完全未命中（返回空串）的性能
     * @param code 未注册消息的错误码
     * @return double ns/op
     */
    inline double benchmark_i18n_get_message_miss(const error_code_t& code) noexcept {
        auto& i18n = i18n_t::instance();
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)i18n.get_message(locale_t::zh_CN, code);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            checksum += i18n.get_message(locale_t::zh_CN, code).size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[i18n_get_miss] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量子系统/模块目录注册性能
     * @return double ns/op
     */
    inline double benchmark_catalog_register() noexcept {
        auto& catalog = subsystem_module_catalog_t::instance();
        catalog.clear();
        stopwatch_t sw;
        sw.start();
        std::size_t registered = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            catalog.register_subsystem_module(
                locale_t::zh_CN,
                static_cast<uint16_t>(1 + (i / 100)),
                static_cast<uint16_t>(1 + (i % 100)),
                "子系统", "模块");
            ++registered;
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[catalog_register] registered=%zu\n", registered);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量子系统/模块目录查询性能（命中主 locale）
     * @param subsystem_id 已注册的子系统 ID
     * @param module_id 已注册的模块 ID
     * @return double ns/op
     */
    inline double benchmark_catalog_get_hit(uint16_t subsystem_id, uint16_t module_id) noexcept {
        auto& catalog = subsystem_module_catalog_t::instance();
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)catalog.get_subsystem_module_info(locale_t::zh_CN, subsystem_id, module_id);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            auto info = catalog.get_subsystem_module_info(locale_t::zh_CN, subsystem_id, module_id);
            checksum += info.subsystem_name.size() + info.module_name.size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[catalog_get_hit] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量子系统/模块目录查询性能（未命中主 locale，回退默认）
     * @param subsystem_id 已注册的子系统 ID
     * @param module_id 已注册的模块 ID
     * @return double ns/op
     */
    inline double benchmark_catalog_get_fallback(uint16_t subsystem_id, uint16_t module_id) noexcept {
        auto& catalog = subsystem_module_catalog_t::instance();
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)catalog.get_subsystem_module_info(locale_t::ja_JP, subsystem_id, module_id);
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            auto info = catalog.get_subsystem_module_info(locale_t::ja_JP, subsystem_id, module_id);
            checksum += info.subsystem_name.size() + info.module_name.size();
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[catalog_get_fallback] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

    /**
     * @brief 测量 locale_t::to_string / from_string 工具函数性能
     * @return double ns/op（两次转换合计）
     */
    inline double benchmark_locale_conversion() noexcept {
        using error_system::i18n::from_string;
        using error_system::i18n::to_string;
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)from_string(to_string(locale_t::zh_CN));
        }
        stopwatch_t sw;
        sw.start();
        std::size_t checksum = 0;
        for (int i = 0; i < MEASURE_ITERATIONS; ++i) {
            checksum += static_cast<std::size_t>(from_string(to_string(locale_t::zh_CN)));
        }
        const auto ns = sw.elapsed_ns();
        std::fprintf(stderr, "[locale_conv] checksum=%zu\n", checksum);
        return static_cast<double>(ns) / static_cast<double>(MEASURE_ITERATIONS);
    }

}  // namespace perf
