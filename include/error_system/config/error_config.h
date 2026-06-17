#pragma once
#include "error_system/core/error_level.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

/**
 * @file error_config.h
 * @brief 错误配置类
 * @details 封装错误配置信息，通过 if constexpr + 编译期常量消除运行时开销
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-03
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {
    struct error_context_t;
}

namespace error_system::config {

    using formatter_callback_t = std::function<std::string(const core::error_context_t&)>;

    /**
     * @brief 错误配置类
     * @details 封装错误配置信息，所有特性开关编译期决议，分支由编译器死代码消除
     */
    class error_config_t {
    public:
        /**
         * @brief 插件通知模式
         * @details sync：同步通知（默认），error_context_t 构造时立即调用所有插件；
         *          async_queue：异步模式，通知推入内部队列，由工作线程消费
         */
        enum class notify_mode_t : uint8_t {
            sync = 0,
            async_queue = 1,
        };

        // ─── 编译期特性开关（每个宏仅判断一次，消除重复 #ifdef） ───
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
         * @brief 格式化函数专用共享锁
         * @details 保护自定义格式化函数的互斥锁
         * @return std::shared_mutex& 共享锁引用
         */
        static std::shared_mutex& __get_formatter_mutex() noexcept {
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
        static std::atomic<bool>& __get_flag_stacktrace() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 是否启用错误码验证标志位存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::atomic<bool>& 是否启用错误码验证标志位引用
         */
        static std::atomic<bool>& __get_flag_validation() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 是否启用错误源位置(文件/行号)标志位存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::atomic<bool>& 是否启用错误源位置(文件/行号)标志位引用
         */
        static std::atomic<bool>& __get_flag_source_location() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 是否启用缩短源文件名标志位存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::atomic<bool>& 是否启用缩短源文件名标志位引用
         */
        static std::atomic<bool>& __get_flag_short_filename() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 是否启用文本输出模式（子系统和模块名称）
         */
        static std::atomic<bool>& __get_flag_text_output() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 通知模式存储
         */
        static std::atomic<notify_mode_t>& __get_notify_mode() noexcept {
            static std::atomic<notify_mode_t> mode{notify_mode_t::sync};
            return mode;
        }

        /**
         * @brief per-code 堆栈追踪等级映射表
         */
        static std::unordered_map<uint64_t, core::error_level_t>& __get_per_code_stacktrace_map() noexcept {
            static std::unordered_map<uint64_t, core::error_level_t> map;
            return map;
        }

        /**
         * @brief per-code 配置专用互斥锁
         */
        static std::shared_mutex& __get_per_code_mutex() noexcept {
            static std::shared_mutex mutex;
            return mutex;
        }

    public:
        error_config_t() = delete;

        /**
         * @brief 设置自定义格式化函数
         * @param formatter 自定义格式化函数
         */
        static void set_custom_formatter(formatter_callback_t formatter) noexcept {
            std::unique_lock<std::shared_mutex> lock(__get_formatter_mutex());
            __get_custom_formatter() = std::move(formatter);
        }

        /**
         * @brief 获取自定义格式化函数副本
         * @details 返回格式化函数的线程安全拷贝，调用方无需持有锁即可安全调用
         * @return formatter_callback_t 格式化函数副本
         */
        static formatter_callback_t get_custom_formatter() noexcept {
            std::shared_lock<std::shared_mutex> lock(__get_formatter_mutex());
            return __get_custom_formatter();
        }

        /**
         * @brief 获取堆栈等级
         * @details 保护全局配置项并发访问的互斥锁。
         *          若编译期未启用堆栈追踪，始终返回 warn。
         * @return error_level_t 堆栈等级
         */
        static core::error_level_t get_stacktrace_level() noexcept {
            if constexpr (STACKTRACE_ENABLED) {
                return __get_min_stacktrace_level().load();
            } else {
                return core::error_level_t::warn;
            }
        }

        /**
         * @brief 设置堆栈等级
         * @details 保护全局配置项并发访问的互斥锁。
         *          若编译期未启用堆栈追踪，此调用无实际操作。
         * @param level 堆栈等级
         */
        static void set_stacktrace_level(core::error_level_t level) noexcept {
            if constexpr (STACKTRACE_ENABLED) {
                __get_min_stacktrace_level().store(level);
            }
        }

        /**
         * @brief 全局开启/关闭堆栈追踪功能
         * @details 保护全局配置项并发访问的互斥锁。
         *          若编译期未启用堆栈追踪，此调用无实际操作。
         * @param enable 是否开启
         */
        static void set_enable_stacktrace(bool enable) noexcept {
            if constexpr (STACKTRACE_ENABLED) {
                __get_flag_stacktrace().store(enable);
            }
        }

        /**
         * @brief 检查全局堆栈追踪功能是否开启
         * @details 保护全局配置项并发访问的互斥锁。
         *          若编译期未启用堆栈追踪，始终返回 false，
         *          允许编译器进行死代码消除 (Dead Code Elimination)。
         * @return bool 是否开启
         */
        static bool is_stacktrace_enabled() noexcept {
            if constexpr (STACKTRACE_ENABLED) {
                return __get_flag_stacktrace().load();
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
                __get_flag_validation().store(enable);
            }
        }

        /**
         * @brief 检查错误码验证功能是否开启
         * @details 若编译期未启用验证，始终返回 false，
         *          允许编译器进行死代码消除 (Dead Code Elimination)。
         * @return bool 是否开启
         */
        static bool is_validation_enabled() noexcept {
            if constexpr (VALIDATION_ENABLED) {
                return __get_flag_validation().load();
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
                __get_flag_source_location().store(enable);
            }
        }

        /**
         * @brief 检查错误位置功能是否开启
         * @details 若编译期未启用位置追踪，始终返回 false，
         *          允许编译器进行死代码消除 (Dead Code Elimination)。
         * @return bool 是否开启
         */
        static bool is_source_location_enabled() noexcept {
            if constexpr (LOCATION_ENABLED) {
                return __get_flag_source_location().load();
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
                __get_flag_short_filename().store(enable);
            }
        }

        /**
         * @brief 检查文件名缩写功能是否开启
         * @details 若编译期未启用位置追踪，始终返回 false，
         *          允许编译器进行死代码消除 (Dead Code Elimination)。
         * @return bool 是否开启
         */
        static bool is_short_filename_enabled() noexcept {
            if constexpr (LOCATION_ENABLED) {
                return __get_flag_short_filename().load();
            } else {
                return false;
            }
        }

        /**
         * @brief 设置文本输出模式
         * @details true 时输出子系统和模块名称，false 时输出原始 ID 数字
         * @param enable 是否开启文本输出
         */
        static void set_enable_text_output(bool enable) noexcept {
            __get_flag_text_output().store(enable);
        }

        /**
         * @brief 检查文本输出模式是否开启
         * @return bool 是否开启文本输出
         */
        static bool is_text_output_enabled() noexcept {
            return __get_flag_text_output().load();
        }

        /**
         * @brief 设置插件通知模式
         * @param mode 通知模式
         */
        static void set_notify_mode(notify_mode_t mode) noexcept {
            __get_notify_mode().store(mode);
        }

        /**
         * @brief 获取插件通知模式
         * @return notify_mode_t 当前通知模式
         */
        static notify_mode_t get_notify_mode() noexcept {
            return __get_notify_mode().load();
        }

        /**
         * @brief 设置指定错误码的堆栈追踪等级（覆盖全局配置）
         * @details 若设置了 per-code 配置，finalize_runtime_features 优先使用此值。
         *          不影响全局 is_stacktrace_enabled 判断。
         *          若编译期未启用堆栈追踪，此调用无实际操作。
         * @param identity_code 错误码的 identity_code（通过 get_identity_code() 获取）
         * @param level 堆栈追踪触发等级
         */
        static void set_per_code_stacktrace_level(uint64_t identity_code, core::error_level_t level) noexcept {
            if constexpr (STACKTRACE_ENABLED) {
                std::unique_lock<std::shared_mutex> lock(__get_per_code_mutex());
                __get_per_code_stacktrace_map()[identity_code] = level;
            }
        }

        /**
         * @brief 获取指定错误码的堆栈追踪等级
         * @details 若编译期未启用堆栈追踪，始终返回 std::nullopt。
         * @param identity_code 错误码的 identity_code
         * @return std::optional<core::error_level_t> 若已设置则返回覆盖值，否则返回空
         */
        static std::optional<core::error_level_t> get_per_code_stacktrace_level(uint64_t identity_code) noexcept {
            if constexpr (STACKTRACE_ENABLED) {
                std::shared_lock<std::shared_mutex> lock(__get_per_code_mutex());
                const auto& map = __get_per_code_stacktrace_map();
                auto it = map.find(identity_code);
                if (it != map.end()) {
                    return it->second;
                }
            }
            return std::nullopt;
        }

        /**
         * @brief 删除指定错误码的堆栈追踪等级覆盖配置
         * @details 若编译期未启用堆栈追踪，此调用无实际操作。
         * @param identity_code 错误码的 identity_code
         */
        static void remove_per_code_stacktrace_level(uint64_t identity_code) noexcept {
            if constexpr (STACKTRACE_ENABLED) {
                std::unique_lock<std::shared_mutex> lock(__get_per_code_mutex());
                __get_per_code_stacktrace_map().erase(identity_code);
            }
        }
    };

}  // namespace error_system::config