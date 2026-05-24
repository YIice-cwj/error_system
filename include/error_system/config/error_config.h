#pragma once
#include "error_system/core/error_level.h"
#include <atomic>
// IWYU pragma: begin_exports
#include <functional>
#include <mutex>
#include <string>
// IWYU pragma: end_exports
#include <shared_mutex>

/**
 * @file error_config.h
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

    using translator_func_t = std::function<std::string(uint16_t subsys_id, uint16_t module_id)>;

    /**
     * @brief 错误配置类
     * @details 封装错误配置信息
     */
    class error_config_t {
        private:
        /**
         * @brief 内部共享锁
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::shared_mutex& 共享锁引用
         */
        static std::shared_mutex& __get_mutex() noexcept {
            static std::shared_mutex mutex;
            return mutex;
        }

        /**
         * @brief 自定义格式化函数存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return formatter_callback_t& 自定义格式化函数引用
         */
        static formatter_callback_t& __get_custom_formatter() noexcept {
            static formatter_callback_t formatter{nullptr};
            return formatter;
        }

        /**
         * @brief 自定义翻译函数存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return translator_func_t& 自定义翻译函数引用
         */
        static translator_func_t& __get_custom_translator() noexcept {
            static translator_func_t translator{nullptr};
            return translator;
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        /**
         * @brief 触发堆栈追踪的最小错误等级存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::atomic<core::error_level_t>& 触发堆栈追踪的最小错误等级引用
         */
        static std::atomic<core::error_level_t>& __get_min_stacktrace_level() noexcept {
            static std::atomic<core::error_level_t> level{core::error_level_t::error};
            return level;
        }

        /**
         * @brief 是否启用堆栈追踪标志位存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::atomic<bool>& 是否启用堆栈追踪标志位引用
         */
        static std::atomic<bool>& __get_enable_stacktrace() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }
#endif

#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
        /**
         * @brief 是否启用错误码验证标志位存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::atomic<bool>& 是否启用错误码验证标志位引用
         */
        static std::atomic<bool>& __get_enable_validation() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }
#endif

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        /**
         * @brief 是否启用错误源位置(文件/行号)标志位存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::atomic<bool>& 是否启用错误源位置(文件/行号)标志位引用
         */
        static std::atomic<bool>& __get_enable_source_location() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 是否启用缩短源文件名标志位存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::atomic<bool>& 是否启用缩短源文件名标志位引用
         */
        static std::atomic<bool>& __get_enable_short_filename() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }
#endif

        public:
        error_config_t() = delete;

        /**
         * @brief 设置自定义格式化函数
         * @param formatter 自定义格式化函数
         */
        static void set_custom_formatter(formatter_callback_t formatter) noexcept {
            std::unique_lock<std::shared_mutex> lock(__get_mutex());
            __get_custom_formatter() = std::move(formatter);
        }

        /**
         * @brief 获取自定义格式化函数
         * @details 保护全局配置项并发访问的互斥锁
         * @return formatter_callback_t 自定义格式化函数
         */
        static formatter_callback_t get_custom_formatter() noexcept {
            std::shared_lock<std::shared_mutex> lock(__get_mutex());
            return __get_custom_formatter();
        }

        /**
         * @brief 设置自定义翻译函数
         * @details 保护全局配置项并发访问的互斥锁
         * @param translator 自定义翻译函数
         */
        static void set_translator(translator_func_t translator) noexcept {
            std::unique_lock<std::shared_mutex> lock(__get_mutex());
            __get_custom_translator() = std::move(translator);
        }

        /**
         * @brief 获取自定义翻译函数
         * @return translator_func_t 自定义翻译函数
         */
        static translator_func_t get_translator() noexcept {
            std::shared_lock<std::shared_mutex> lock(__get_mutex());
            return __get_custom_translator();
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        /**
         * @brief 获取堆栈等级
         * @details 保护全局配置项并发访问的互斥锁
         * @return error_level_t 堆栈等级
         */
        static core::error_level_t get_stacktrace_level() noexcept {
            return __get_min_stacktrace_level().load(std::memory_order_relaxed);
        }

        /**
         * @brief 设置堆栈等级
         * @details 保护全局配置项并发访问的互斥锁
         * @param level 堆栈等级
         */
        static void set_stacktrace_level(core::error_level_t level) noexcept {
            __get_min_stacktrace_level().store(level, std::memory_order_relaxed);
        }

        /**
         * @brief 全局开启/关闭堆栈追踪功能
         * @details 保护全局配置项并发访问的互斥锁
         * @param enable 是否开启
         */
        static void set_enable_stacktrace(bool enable) noexcept {
            __get_enable_stacktrace().store(enable, std::memory_order_relaxed);
        }

        /**
         * @brief 检查全局堆栈追踪功能是否开启
         * @details 保护全局配置项并发访问的互斥锁
         * @return bool 是否开启
         */
        static bool is_stacktrace_enabled() noexcept {
            return __get_enable_stacktrace().load(std::memory_order_relaxed);
        }
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
            __get_enable_validation().store(enable, std::memory_order_relaxed);
        }

        /**
         * @brief 检查错误码验证功能是否开启
         * @return bool 是否开启
         */
        static bool is_validation_enabled() noexcept {
            return __get_enable_validation().load(std::memory_order_relaxed);
        }
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
            __get_enable_source_location().store(enable, std::memory_order_relaxed);
        }

        /**
         * @brief 检查错误位置功能是否开启
         * @return bool 是否开启
         */
        static bool is_source_location_enabled() noexcept {
            return __get_enable_source_location().load(std::memory_order_relaxed);
        }

        /**
         * @brief 设置是否开启文件名缩写
         * @param enable 是否开启
         */
        static void set_enable_short_filename(bool enable) noexcept {
            __get_enable_short_filename().store(enable, std::memory_order_relaxed);
        }

        /**
         * @brief 检查文件名缩写功能是否开启
         * @return bool 是否开启
         */
        static bool is_short_filename_enabled() noexcept {
            return __get_enable_short_filename().load(std::memory_order_relaxed);
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