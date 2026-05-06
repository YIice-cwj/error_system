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

    class error_config_t {
        private:
        static inline std::atomic<i18n::language_t> default_language_{i18n::language_t::zh_cn};
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        static inline std::atomic<error_level_t> min_stacktrace_level_{error_level_t::error};
        static inline std::atomic<bool> enable_stacktrace_{true};
#endif

#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
        static inline std::atomic<bool> enable_validation_{true};
#endif
        public:
        error_config_t() = delete;

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
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
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
#else
        /**
         * @brief 获取堆栈等级
         * @return error_level_t 堆栈等级
         */
        static error_level_t get_stacktrace_level() noexcept { return error_level_t::warn; }

        /**
         * @brief 设置堆栈等级
         * @param level 堆栈等级
         */
        [[deprecated("警告：堆栈追踪功能已在 CMake 编译期被关闭。此调用无效，相关代码已从二进制文件中抹除。")]]
        static void set_stacktrace_level(error_level_t /*level*/) noexcept {}

        /**
         * @brief 🌟 编译期警告：若 CMake 关闭了堆栈功能，调用此函数将触发编译器警告
         */
        [[deprecated("警告：堆栈追踪功能已在 CMake 编译期被关闭。此调用无效，相关代码已从二进制文件中抹除。")]]
        static void set_enable_stacktrace(bool /*enable*/) noexcept {}

        /**
         * @brief 🌟 编译期常量：返回 false，允许编译器进行死代码消除 (Dead Code Elimination)
         */
        static constexpr bool is_stacktrace_enabled() noexcept { return false; }
#endif

#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
        /**
         * @brief 检查全局堆栈追踪功能是否开启
         * @return bool 是否开启
         */
        static bool is_stacktrace_enabled() noexcept { return enable_stacktrace_.load(std::memory_order_relaxed); }

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
        static bool is_validation_enabled() noexcept { return enable_validation_.load(std::memory_order_relaxed); }
    };
#else
        /**
         * @brief 检查全局堆栈追踪功能是否开启
         * @return bool 是否开启
         */
        static bool is_stacktrace_enabled() noexcept { return false; }

        /**
         * @brief 全局开启/关闭错误码验证功能
         * @param enable 是否开启
         */
        [[deprecated("警告：错误码验证功能已在 CMake 编译期被关闭。此调用无效，相关代码已从二进制文件中抹除。")]]
        static void set_enable_validation(bool /*enable*/) noexcept {}

        /**
         * @brief 检查错误码验证功能是否开启
         * @return bool 是否开启
         */
        static bool is_validation_enabled() noexcept { return false; }
    };
#endif

}  // namespace error_system::core