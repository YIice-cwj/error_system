#pragma once
#include "error_system/core/error_level.h"
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>

/**
 * @brief 错误配置类
 * @details 封装错误配置信息
 * @author yiice
 * @version 1.0.0
 * @date 2026-05-03
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    class error_config {
        private:
        static inline std::atomic<error_level_t> min_stacktrace_level_{error_level_t::error};
        static inline std::string default_language_{"zh_cn"};
        static inline std::shared_mutex language_mutex_;

        public:
        error_config() = delete;

        /**
         * @brief 设置堆栈等级
         * @param level 堆栈等级
         */
        static void set_stacktrace_level(error_level_t level) noexcept {
            min_stacktrace_level_.store(level, std::memory_order_relaxed);
        }

        /**
         * @brief 获取堆栈等级
         * @return error_level_t 堆栈等级
         */
        static error_level_t get_stacktrace_level() noexcept {
            return min_stacktrace_level_.load(std::memory_order_relaxed);
        }

        /**
         * @brief 设置默认语言
         * @param language 默认语言
         */
        static void set_default_language(const std::string& language) {
            std::unique_lock lock(language_mutex_);
            default_language_ = language;
        }

        /**
         * @brief 获取默认语言
         * @return std::string 默认语言
         */
        static std::string get_default_language() {
            std::shared_lock lock(language_mutex_);
            return default_language_;
        }
    };

}  // namespace error_system::core