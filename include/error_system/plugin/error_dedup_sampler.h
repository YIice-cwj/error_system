#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_map>

#include "error_system/core/error_code.h"
#include "error_system/core/error_context.h"

/**
 * @file error_dedup_sampler.h
 * @brief 错误去重与采样器
 * @details 提供基于时间窗口的错误去重和基于速率的错误采样能力，
 *          用于高并发场景下抑制重复错误洪水、降低下游通知压力。
 *          线程安全，可被多个生产者线程共享调用。
 * @note 本头文件仅含声明，实现见 error_dedup_sampler.cc
 * @author yiice
 * @version 1.1.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::plugin {

    /**
     * @brief 错误去重与采样器
     * @details 单一职责：对流入的错误上下文做去重与采样判定，不感知下游通知通道。
     *          使用方式：
     * @code
     *   error_dedup_sampler_t sampler;
     *   sampler.set_dedup_window_ms(1000);   // 同一 identity code 1s 内只放行一次
     *   sampler.set_sample_rate(0.1);        // 放行 10%（每 10 个放行 1 个）
     *   if (sampler.should_forward(ctx)) {
     *       channel.enqueue_notification(ctx);
     *   }
     * @endcode
     *          去重基于 error_code_t::get_identity_code()（忽略 sign/reserved 位），
     *          保证同业务错误的不同瞬态标记（retryable/transient）仍被去重。
     *          采样采用确定性计数器（每 N 个放行 1 个），避免随机数开销，
     *          适合高频错误热路径。
     */
    class error_dedup_sampler_t {
    public:
        using clock_t = std::chrono::steady_clock;
        using time_point_t = clock_t::time_point;

    private:
        /**
         * @brief 去重表项
         * @details 记录某 identity code 最近一次放行的时间戳与窗口内累计抑制数
         */
        struct dedup_entry_t {
            time_point_t last_forwarded{};
            uint64_t suppressed_count{0};
        };

        mutable std::mutex mutex_;
        std::unordered_map<core::code_t, dedup_entry_t> dedup_map_;

        /**
         * @brief 去重时间窗口（毫秒），0 表示去重关闭
         */
        uint64_t dedup_window_ms_{0};

        /**
         * @brief 采样放行间隔（每 N 个放行 1 个），0 表示采样关闭（全部放行）
         * @details 由 set_sample_rate 转换得来：rate=0.1 → interval=10
         */
        uint64_t sample_interval_{0};

        /**
         * @brief 采样计数器（原子，避免锁竞争）
         */
        std::atomic<uint64_t> sample_counter_{0};

        /**
         * @brief 统计：被去重抑制数
         */
        uint64_t deduped_count_{0};

        /**
         * @brief 统计：被采样抑制数
         */
        uint64_t sampled_count_{0};

        /**
         * @brief 统计：放行数
         */
        uint64_t forwarded_count_{0};

        /**
         * @brief 清理去重表中过期的表项
         * @details 在持锁状态下调用，遍历删除超过 dedup_window_ms_ 未命中的项
         * @param now 当前时间点
         */
        void cleanup_expired_(time_point_t now) noexcept;

    public:
        error_dedup_sampler_t() noexcept = default;
        ~error_dedup_sampler_t() noexcept = default;

        error_dedup_sampler_t(const error_dedup_sampler_t&) = delete;
        error_dedup_sampler_t& operator=(const error_dedup_sampler_t&) = delete;
        error_dedup_sampler_t(error_dedup_sampler_t&&) = delete;
        error_dedup_sampler_t& operator=(error_dedup_sampler_t&&) = delete;

        /**
         * @brief 设置去重时间窗口
         * @details 同一 identity code 在窗口内只放行首次，其余抑制。
         *          设为 0 关闭去重（默认）。
         * @param milliseconds 时间窗口（毫秒）
         */
        void set_dedup_window_ms(uint64_t milliseconds) noexcept;

        /**
         * @brief 设置采样放行率
         * @details rate=1.0 全部放行，rate=0.1 放行 10%（每 10 个放行 1 个）。
         *          rate<=0 视为 0（全部抑制，慎用），rate>=1 视为 1（全部放行）。
         *          采用确定性计数器：第 (counter % interval == 0) 个放行。
         * @param rate 采样率 [0.0, 1.0]
         */
        void set_sample_rate(double rate) noexcept;

        /**
         * @brief 判定错误上下文是否应放行
         * @details 先过采样（廉价，原子操作），再过去重（需锁）。
         *          两关都通过才返回 true。任何一关失败都更新对应统计。
         * @param context 错误上下文
         * @return bool true=放行，false=抑制
         */
        [[nodiscard]] bool should_forward(const core::error_context_t& context) noexcept;

        /**
         * @brief 获取被去重抑制的数量
         * @return uint64_t 抑制数
         */
        [[nodiscard]] uint64_t deduped_count() const noexcept;

        /**
         * @brief 获取被采样抑制的数量
         * @return uint64_t 抑制数
         */
        [[nodiscard]] uint64_t sampled_count() const noexcept;

        /**
         * @brief 获取放行数量
         * @return uint64_t 放行数
         */
        [[nodiscard]] uint64_t forwarded_count() const noexcept;

        /**
         * @brief 重置所有统计计数与采样计数器
         * @details 将 deduped/sampled/forwarded 计数归零，并重置采样计数器，
         *          使后续采样从 seq=0 开始（保证测试可重复）。
         *          注意：并发调用时重置后的计数可能与正在进行的 should_forward 产生竞争，
         *          仅建议在无并发或测试场景下使用。
         */
        void reset_stats() noexcept;

        /**
         * @brief 清除去重表
         */
        void clear_dedup_cache() noexcept;

        /**
         * @brief 获取去重表大小
         * @return size_t 表项数
         */
        [[nodiscard]] size_t dedup_cache_size() const noexcept;
    };

}  // namespace error_system::plugin
