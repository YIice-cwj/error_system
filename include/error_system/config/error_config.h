#pragma once
#include "error_system/core/error_level.h"
#include <atomic>
// IWYU pragma: begin_exports
#include <mutex>
// IWYU pragma: end_exports
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
    struct error_context_t;
}

namespace error_system::config {

    using formatter_callback_t = std::function<std::string(const core::error_context_t&)>;

    class error_config_t {
        private:
        static inline formatter_callback_t custom_formatter_{nullptr};

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        static inline std::atomic<core::error_level_t> min_stacktrace_level_{core::error_level_t::error};
        static inline std::atomic<bool> enable_stacktrace_{true};
#endif

#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
        static inline std::atomic<bool> enable_validation_{true};
#endif

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        static inline std::atomic<bool> enable_source_location_{true};
        static inline std::atomic<bool> enable_short_filename_{true};
#endif

        static inline std::shared_mutex config_mutex_{};

        public:
        error_config_t() = delete;

        /**
         * @brief 设置自定义格式化函数
         * @param formatter 自定义格式化函数
         */
        static void set_custom_formatter(formatter_callback_t formatter) noexcept {
            std::unique_lock<std::shared_mutex> lock(config_mutex_);
            custom_formatter_ = formatter;
        }

        /**
         * @brief 获取自定义格式化函数
         * @return formatter_callback_t 自定义格式化函数
         */
        static formatter_callback_t get_custom_formatter() noexcept {
            std::shared_lock<std::shared_mutex> lock(config_mutex_);
            return custom_formatter_;
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        /**
         * @brief 获取堆栈等级
         * @return error_level_t 堆栈等级
         */
        static core::error_level_t get_stacktrace_level() noexcept {
            return min_stacktrace_level_.load(std::memory_order_relaxed);
        }

        /**
         * @brief 设置堆栈等级
         * @param level 堆栈等级
         */
        static void set_stacktrace_level(core::error_level_t level) noexcept {
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
        static bool is_stacktrace_enabled() noexcept { return enable_stacktrace_.load(std::memory_order_relaxed); }
#else
        /**
         * @brief 获取堆栈等级
         * @return error_level_t 堆栈等级
         */
        static constexpr core::error_level_t get_stacktrace_level() noexcept { return core::error_level_t::warn; }

        /**
         * @brief 设置堆栈等级
         * @param level 堆栈等级
         */
        [[deprecated("警告：堆栈追踪功能已在 CMake 编译期被关闭。此调用无效，相关代码已从二进制文件中抹除。")]]
        static void set_stacktrace_level(core::error_level_t /*level*/) noexcept {}

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
#else
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
        static constexpr bool is_validation_enabled() noexcept { return false; }
#endif

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        /**
         * @brief 全局开启/关闭错误位置功能
         * @param enable 是否开启
         */
        static void set_enable_source_location(bool enable) noexcept {
            enable_source_location_.store(enable, std::memory_order_relaxed);
        }

        /**
         * @brief 检查错误位置功能是否开启
         * @return bool 是否开启
         */
        static bool is_source_location_enabled() noexcept {
            return enable_source_location_.load(std::memory_order_relaxed);
        }

        /**
         * @brief 设置是否开启文件名缩写
         * @param enable 是否开启
         */
        static void set_enable_short_filename(bool enable) noexcept {
            enable_short_filename_.store(enable, std::memory_order_relaxed);
        }

        /**
         * @brief 检查文件名缩写功能是否开启
         * @return bool 是否开启
         */
        static bool is_short_filename_enabled() noexcept {
            return enable_short_filename_.load(std::memory_order_relaxed);
        }
#else
        /**
         * @brief 全局开启/关闭错误位置功能
         * @param enable 是否开启
         */
        [[deprecated("警告：位置追踪已在 CMake 中被关闭。此调用无效。")]]
        static void set_enable_source_location(bool /*enable*/) noexcept {}

        /**
         * @brief 检查错误位置功能是否开启
         * @return bool 是否开启
         */
        static constexpr bool is_source_location_enabled() noexcept { return false; }

        /**
         * @brief 设置是否开启文件名缩写
         * @param enable 是否开启
         */
        [[deprecated("警告：位置追踪已在 CMake 中被关闭。此调用无效。")]]
        static void set_enable_short_filename(bool /*enable*/) noexcept {}

        /**
         * @brief 检查文件名缩写功能是否开启
         * @return bool 是否开启
         */
        static constexpr bool is_short_filename_enabled() noexcept { return false; }
#endif
    };

}  // namespace error_system::config