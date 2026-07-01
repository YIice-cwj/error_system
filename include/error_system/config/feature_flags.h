#pragma once
#include <atomic>
#include <cstdint>

#include "error_system/core/error_level.h"

/**
 * @file feature_flags.h
 * @brief 特性开关配置类
 * @details 从 error_config_t 拆分而来，单一职责：管理错误系统的编译期特性开关与
 *          运行时布尔标志位（堆栈追踪、验证、源位置、文件名缩写、通知模式）。
 *          文本/i18n 输出开关已统一迁移到 i18n_config_t::set_enable_i18n。
 *          编译期常量通过 if constexpr 消除运行时开销，由编译器死代码消除未启用分支。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::config {

    /**
     * @brief 特性开关配置类
     * @details 封装错误系统的所有特性开关，所有特性开关编译期决议，分支由编译器死代码消除。
     *          类仅包含静态成员，禁止实例化。
     */
    class feature_flags_t {
    public:
        /**
         * @brief 插件通知模式
         * @details sync：同步通知（默认），error_context_t 构造时立即调用所有插件；
         *          async_queue：异步模式，通知推入内部队列，由工作线程消费；
         *          sync_deferred：同步延迟模式，通知累积到线程本地缓冲，
         *                         由调用方显式调用 flush_deferred_notifications() 触发批量通知
         */
        enum class notify_mode_t : uint8_t {
            sync = 0,
            async_queue = 1,
            sync_deferred = 2,
        };

        static constexpr bool STACKTRACE_ENABLED =
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            true;
#else
            false;
#endif
        static constexpr bool VALIDATION_ENABLED =
#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
            true;
#else
            false;
#endif
        static constexpr bool LOCATION_ENABLED =
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
            true;
#else
            false;
#endif

    private:
        /**
         * @brief 是否启用堆栈追踪标志位存储
         * @details 使用 std::atomic<bool> 保证无锁并发读写
         * @return std::atomic<bool>& 是否启用堆栈追踪标志位引用
         */
        static std::atomic<bool>& get_flag_stacktrace_() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 是否启用错误码验证标志位存储
         * @details 使用 std::atomic<bool> 保证无锁并发读写
         * @return std::atomic<bool>& 是否启用错误码验证标志位引用
         */
        static std::atomic<bool>& get_flag_validation_() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 是否启用错误源位置(文件/行号)标志位存储
         * @details 使用 std::atomic<bool> 保证无锁并发读写
         * @return std::atomic<bool>& 是否启用错误源位置(文件/行号)标志位引用
         */
        static std::atomic<bool>& get_flag_source_location_() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 是否启用缩短源文件名标志位存储
         * @details 使用 std::atomic<bool> 保证无锁并发读写
         * @return std::atomic<bool>& 是否启用缩短源文件名标志位引用
         */
        static std::atomic<bool>& get_flag_short_filename_() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 通知模式存储
         */
        static std::atomic<notify_mode_t>& get_notify_mode_() noexcept {
            static std::atomic<notify_mode_t> mode{notify_mode_t::sync};
            return mode;
        }

    public:
        feature_flags_t() = delete;
        ~feature_flags_t() = delete;
        feature_flags_t(const feature_flags_t&) = delete;
        feature_flags_t& operator=(const feature_flags_t&) = delete;
        feature_flags_t(feature_flags_t&&) = delete;
        feature_flags_t& operator=(feature_flags_t&&) = delete;

        /**
         * @brief 全局开启/关闭堆栈追踪功能
         * @details 通过 std::atomic<bool> 无锁设置。若编译期未启用堆栈追踪，此调用无实际操作。
         * @param enable 是否开启
         */
        static void set_enable_stacktrace(bool enable) noexcept {
            if constexpr (STACKTRACE_ENABLED) {
                get_flag_stacktrace_().store(enable);
            }
        }

        /**
         * @brief 检查全局堆栈追踪功能是否开启
         * @details 若编译期未启用堆栈追踪，始终返回 false，
         *          允许编译器进行死代码消除 (Dead Code Elimination)。
         * @return bool 是否开启
         */
        [[nodiscard]] static bool is_stacktrace_enabled() noexcept {
            if constexpr (STACKTRACE_ENABLED) {
                return get_flag_stacktrace_().load();
            } else {
                return false;
            }
        }

        /**
         * @brief 全局开启/关闭错误码验证功能
         * @details 若编译期未启用验证，此调用无实际操作。
         * @param enable 是否开启
         */
        static void set_enable_validation(bool enable) noexcept {
            if constexpr (VALIDATION_ENABLED) {
                get_flag_validation_().store(enable);
            }
        }

        /**
         * @brief 检查错误码验证功能是否开启
         * @details 若编译期未启用验证，始终返回 false，
         *          允许编译器进行死代码消除 (Dead Code Elimination)。
         * @return bool 是否开启
         */
        [[nodiscard]] static bool is_validation_enabled() noexcept {
            if constexpr (VALIDATION_ENABLED) {
                return get_flag_validation_().load();
            } else {
                return false;
            }
        }

        /**
         * @brief 全局开启/关闭错误位置功能
         * @details 若编译期未启用位置追踪，此调用无实际操作。
         * @param enable 是否开启
         */
        static void set_enable_source_location(bool enable) noexcept {
            if constexpr (LOCATION_ENABLED) {
                get_flag_source_location_().store(enable);
            }
        }

        /**
         * @brief 检查错误位置功能是否开启
         * @details 若编译期未启用位置追踪，始终返回 false，
         *          允许编译器进行死代码消除 (Dead Code Elimination)。
         * @return bool 是否开启
         */
        [[nodiscard]] static bool is_source_location_enabled() noexcept {
            if constexpr (LOCATION_ENABLED) {
                return get_flag_source_location_().load();
            } else {
                return false;
            }
        }

        /**
         * @brief 设置是否开启文件名缩写
         * @details 若编译期未启用位置追踪，此调用无实际操作。
         * @param enable 是否开启
         */
        static void set_enable_short_filename(bool enable) noexcept {
            if constexpr (LOCATION_ENABLED) {
                get_flag_short_filename_().store(enable);
            }
        }

        /**
         * @brief 检查文件名缩写功能是否开启
         * @details 若编译期未启用位置追踪，始终返回 false，
         *          允许编译器进行死代码消除 (Dead Code Elimination)。
         * @return bool 是否开启
         */
        [[nodiscard]] static bool is_short_filename_enabled() noexcept {
            if constexpr (LOCATION_ENABLED) {
                return get_flag_short_filename_().load();
            } else {
                return false;
            }
        }

        /**
         * @brief 设置插件通知模式
         * @param mode 通知模式
         */
        static void set_notify_mode(notify_mode_t mode) noexcept {
            get_notify_mode_().store(mode);
        }

        /**
         * @brief 获取插件通知模式
         * @return notify_mode_t 当前通知模式
         */
        [[nodiscard]] static notify_mode_t get_notify_mode() noexcept {
            return get_notify_mode_().load();
        }
    };

}  // namespace error_system::config
