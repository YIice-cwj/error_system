#pragma once
#include <atomic>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include "error_system/config/feature_flags.h"
#include "error_system/core/error_level.h"

/**
 * @file stacktrace_config.h
 * @brief 堆栈追踪配置类
 * @details 从 error_config_t 拆分而来，单一职责：管理堆栈追踪的全局阈值与 per-code 覆盖配置。
 *          通过 if constexpr + 编译期常量 STACKTRACE_ENABLED 消除运行时开销，
 *          编译器死代码消除未启用分支。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::config {

    /**
     * @brief 堆栈追踪配置类
     * @details 管理堆栈追踪的全局阈值与 per-code 覆盖配置。
     *          类仅包含静态成员，禁止实例化。
     */
    class stacktrace_config_t {
    private:
        /**
         * @brief 触发堆栈追踪的最小错误等级存储
         * @details 保护全局配置项并发访问的互斥锁
         * @return std::atomic<core::error_level_t>& 触发堆栈追踪的最小错误等级引用
         */
        static std::atomic<core::error_level_t>& get_min_stacktrace_level_() noexcept {
            static std::atomic<core::error_level_t> level{core::error_level_t::error};
            return level;
        }

        /**
         * @brief per-code 堆栈追踪等级映射表
         */
        static std::unordered_map<uint64_t, core::error_level_t>& get_per_code_stacktrace_map_() noexcept {
            static std::unordered_map<uint64_t, core::error_level_t> map;
            return map;
        }

        /**
         * @brief per-code 配置专用互斥锁
         */
        static std::shared_mutex& get_per_code_mutex_() noexcept {
            static std::shared_mutex mutex;
            return mutex;
        }

    public:
        stacktrace_config_t() = delete;
        ~stacktrace_config_t() = delete;
        stacktrace_config_t(const stacktrace_config_t&) = delete;
        stacktrace_config_t& operator=(const stacktrace_config_t&) = delete;
        stacktrace_config_t(stacktrace_config_t&&) = delete;
        stacktrace_config_t& operator=(stacktrace_config_t&&) = delete;

        /**
         * @brief 获取堆栈等级
         * @details 保护全局配置项并发访问的互斥锁。
         *          若编译期未启用堆栈追踪，始终返回 warn。
         * @return error_level_t 堆栈等级
         */
        static core::error_level_t get_stacktrace_level() noexcept {
            if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
                return get_min_stacktrace_level_().load();
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
            if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
                get_min_stacktrace_level_().store(level);
            }
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
            if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
                std::unique_lock<std::shared_mutex> lock(get_per_code_mutex_());
                get_per_code_stacktrace_map_()[identity_code] = level;
            }
        }

        /**
         * @brief 获取指定错误码的堆栈追踪等级
         * @details 若编译期未启用堆栈追踪，始终返回 std::nullopt。
         * @param identity_code 错误码的 identity_code
         * @return std::optional<core::error_level_t> 若已设置则返回覆盖值，否则返回空
         */
        static std::optional<core::error_level_t> get_per_code_stacktrace_level(uint64_t identity_code) noexcept {
            if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
                std::shared_lock<std::shared_mutex> lock(get_per_code_mutex_());
                const auto& map = get_per_code_stacktrace_map_();
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
            if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
                std::unique_lock<std::shared_mutex> lock(get_per_code_mutex_());
                get_per_code_stacktrace_map_().erase(identity_code);
            }
        }
    };

}  // namespace error_system::config
