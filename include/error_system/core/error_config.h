#pragma once
#include "error_system/core/error_level.h"
#include "error_system/i18n/language.h"
#include <atomic>
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
        static inline std::atomic<i18n::language_t> default_language_{i18n::language_t::zh_cn};
        #ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            static inline std::atomic<bool> enable_stacktrace_{true};
        #else
            static inline std::atomic<bool> enable_stacktrace_{false};
        #endif

        #ifdef ERROR_SYSTEM_ENABLE_VALIDATION
            static inline std::atomic<bool> enable_validation_{true};
        #else
            static inline std::atomic<bool> enable_validation_{false};
        #endif

        public:
        error_config() = delete;

        /**
         * @brief 设置默认语言
         * @param language 默认语言
         */
        static void set_default_language(const std::string& language) {
            default_language_.store(i18n::from_string(language), std::memory_order_relaxed);
        }

         /**
         * @brief 设置默认语言
         * @param language 默认语言
         */
        static void set_default_language(const i18n::language_t language) {
            default_language_.store(language, std::memory_order_relaxed);
        }

        /**
         * @brief 获取默认语言
         * @return std::string 默认语言
         */
        static std::string get_default_language() {
            return i18n::to_string(default_language_.load(std::memory_order_relaxed));
        }

        /**
         * @brief 获取堆栈等级
         * @return error_level_t 堆栈等级
         */
        static error_level_t get_stacktrace_level() noexcept {
            return min_stacktrace_level_.load(std::memory_order_relaxed);
        }

        /**
         * @brief 设置堆栈等级
         * @param level 堆栈等级
         */
        static void set_stacktrace_level(error_level_t level) noexcept {
            min_stacktrace_level_.store(level, std::memory_order_relaxed);
        }


        /**
         * @brief 全局开启/关闭堆栈追踪功能
         * @param enable 是否开启
         */
        static void set_enable_stacktrace(bool enable) noexcept {
            enable_stacktrace_.store(enable, std::memory_order_relaxed);
        }

        /**
         * @brief 检查全局堆栈追踪功能是否开启
         * @return bool 是否开启
         */
        static bool is_stacktrace_enabled() noexcept {
            return enable_stacktrace_.load(std::memory_order_relaxed);
        }

        /**
         * @brief 全局开启/关闭错误码验证功能
         * @param enable 是否开启
         */
        static void set_enable_validation(bool enable) noexcept {
            enable_validation_.store(enable, std::memory_order_relaxed);
        }

        /**
         * @brief 检查错误码验证功能是否开启
         * @return bool 是否开启
         */
        static bool is_validation_enabled() noexcept {
            return enable_validation_.load(std::memory_order_relaxed);
        }
    };

}  // namespace error_system::core