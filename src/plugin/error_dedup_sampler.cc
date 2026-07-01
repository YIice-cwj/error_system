#include "error_system/plugin/error_dedup_sampler.h"

/**
 * @file error_dedup_sampler.cc
 * @brief 错误去重与采样器实现
 * @details 提供基于时间窗口的错误去重和基于速率的错误采样能力，
 *          用于高并发场景下抑制重复错误洪水、降低下游通知压力。
 * @author yiice
 * @version 1.1.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */

#include <chrono>
#include <limits>
#include <mutex>
#include <new>
#include <unordered_map>

namespace error_system::plugin {

    namespace {
        constexpr size_t DEDUP_CLEANUP_THRESHOLD = 4096;  ///< 去重表大小超过该阈值时触发过期清理
    }  // namespace

    /**
     * @brief 清理去重表中过期的表项
     * @details 在持锁状态下调用，遍历删除超过 dedup_window_ms_ 未命中的项
     * @param now 当前时间点
     */
    void error_dedup_sampler_t::cleanup_expired_(time_point_t now) noexcept {
        for (auto it = dedup_map_.begin(); it != dedup_map_.end();) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second.last_forwarded).count();
            if (static_cast<uint64_t>(elapsed) >= dedup_window_ms_) {
                it = dedup_map_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief 设置去重时间窗口
     * @details 同一 identity code 在窗口内只放行首次，其余抑制。
     *          设为 0 关闭去重（默认）。
     * @param milliseconds 时间窗口（毫秒）
     */
    void error_dedup_sampler_t::set_dedup_window_ms(uint64_t milliseconds) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        dedup_window_ms_ = milliseconds;
        if (milliseconds == 0) {
            dedup_map_.clear();
        }
    }

    /**
     * @brief 设置采样放行率
     * @details rate=1.0 全部放行，rate=0.1 放行 10%（每 10 个放行 1 个）。
     *          rate<=0 视为 0（全部抑制，慎用），rate>=1 视为 1（全部放行）。
     *          采用确定性计数器：第 (counter % interval == 0) 个放行。
     * @param rate 采样率 [0.0, 1.0]
     */
    void error_dedup_sampler_t::set_sample_rate(double rate) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (rate <= 0.0) {
            sample_interval_ = std::numeric_limits<uint64_t>::max();
        } else if (rate >= 1.0) {
            sample_interval_ = 0;
        } else {
            sample_interval_ = static_cast<uint64_t>(1.0 / rate);
            if (sample_interval_ < 1) {
                sample_interval_ = 1;
            }
        }
    }

    /**
     * @brief 采样判定
     * @details 在持锁状态下更新 sampled_count_ 并返回是否应被采样抑制。
     *          采样为原子操作，但计数更新需要锁保护。
     * @return bool true=被采样抑制，false=通过采样
     */
    bool error_dedup_sampler_t::is_sampling_suppressed_() noexcept {
        constexpr uint64_t SUPPRESS_ALL = std::numeric_limits<uint64_t>::max();
        if (sample_interval_ == 0) {
            return false;
        }
        if (sample_interval_ == SUPPRESS_ALL) {
            ++sampled_count_;
            return true;
        }
        const uint64_t seq = sample_counter_.fetch_add(1);
        if (seq % sample_interval_ == 0) {
            return false;
        }
        ++sampled_count_;
        return true;
    }

    /**
     * @brief 去重判定
     * @details 在持锁状态下检查 identity code 是否在时间窗口内已放行。
     *          若未命中或已过期则更新去重表；返回 true 表示被去重抑制。
     * @param identity 错误码 identity
     * @param now 当前时间点
     * @return bool true=被去重抑制，false=通过去重
     */
    bool error_dedup_sampler_t::is_dedup_suppressed_(core::code_t identity, time_point_t now) noexcept {
        if (dedup_window_ms_ == 0) {
            return false;
        }
        auto it = dedup_map_.find(identity);
        if (it != dedup_map_.end()) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second.last_forwarded).count();
            if (static_cast<uint64_t>(elapsed) < dedup_window_ms_) {
                ++it->second.suppressed_count;
                ++deduped_count_;
                return true;
            }
        }
        try {
            dedup_map_[identity].last_forwarded = now;
        } catch (const std::bad_alloc&) {
            ++forwarded_count_;
            return false;
        }
        if (dedup_map_.size() > DEDUP_CLEANUP_THRESHOLD) {
            cleanup_expired_(now);
        }
        return false;
    }

    /**
     * @brief 判定错误上下文是否应放行
     * @details 先过采样（廉价，原子操作），再过去重（需锁）。
     *          两关都通过才返回 true。任何一关失败都更新对应统计。
     * @param context 错误上下文
     * @return bool true=放行，false=抑制
     */
    bool error_dedup_sampler_t::should_be_forwarded(const core::error_context_t& context) noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (is_sampling_suppressed_()) {
                return false;
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const core::code_t identity = context.get_code().get_identity_code();
        const auto now = clock_t::now();
        if (is_dedup_suppressed_(identity, now)) {
            return false;
        }

        ++forwarded_count_;
        return true;
    }

    /**
     * @brief 获取被去重抑制的数量
     * @return uint64_t 抑制数
     */
    uint64_t error_dedup_sampler_t::deduped_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return deduped_count_;
    }

    /**
     * @brief 获取被采样抑制的数量
     * @return uint64_t 抑制数
     */
    uint64_t error_dedup_sampler_t::sampled_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return sampled_count_;
    }

    /**
     * @brief 获取放行数量
     * @return uint64_t 放行数
     */
    uint64_t error_dedup_sampler_t::forwarded_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return forwarded_count_;
    }

    /**
     * @brief 重置所有统计计数与采样计数器
     * @details 将 deduped/sampled/forwarded 计数归零，并重置采样计数器，
     *          使后续采样从 seq=0 开始（保证测试可重复）。
     *          注意：并发调用时重置后的计数可能与正在进行的 should_be_forwarded 产生竞争，
     *          仅建议在无并发或测试场景下使用。
     */
    void error_dedup_sampler_t::reset_stats() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        deduped_count_ = 0;
        sampled_count_ = 0;
        forwarded_count_ = 0;
        sample_counter_.store(0);
    }

    /**
     * @brief 清除去重表
     */
    void error_dedup_sampler_t::clear_dedup_cache() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        dedup_map_.clear();
    }

    /**
     * @brief 获取去重表大小
     * @return size_t 表项数
     */
    size_t error_dedup_sampler_t::dedup_cache_size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return dedup_map_.size();
    }

}  // namespace error_system::plugin
