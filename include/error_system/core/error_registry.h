#pragma once
#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "error_system/core/error_code.h"
#include "error_system/core/error_level.h"
#include "error_system/core/error_metadata.h"
// IWYU pragma: begin_exports
#include "error_system/core/duplicate_policy.h"
#include "error_system/core/error_builder.h"
// IWYU pragma: end_exports

/**
 * @file error_registry.h
 * @brief 错误码注册器
 * @details 定义错误码注册器，用于注册和查找错误码。重复处理策略委托给 duplicate_policy_handler_t，
 *          遵循单一职责原则。子系统/模块名称映射请直接使用 i18n::subsystem_module_catalog_t。
 * @author yiice
 * @version 3.0.0
 * @date 2026-06-29
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误码注册器
     * @details 用于注册和查找错误码。错误码元数据定义见 error_metadata.h。
     * @author yiice
     * @version 3.0.1
     * @date 2026-07-01
     */
    class error_registry_t {
    private:
        /**
         * @brief 主索引，根据错误码快速查找元数据
         */
        std::unordered_map<code_t, error_metadata_t> primary_index_;

        /**
         * @brief 名称索引，根据错误码名称快速查找 identity code
         */
        std::unordered_map<std::string, code_t> name_index_;

        /**
         * @brief 模块索引，根据模块 ID快速查找错误码
         */
        std::unordered_map<module_group_id_t, std::vector<code_t>> module_index_;

        /**
         * @brief 子系统索引，根据子系统 ID 快速查找其下所有模块组
         * @details key = subsystem_id，value = 该子系统下的所有 module_group_id（去重）
         */
        std::unordered_map<uint16_t, std::unordered_set<module_group_id_t>> subsystem_index_;

        /**
         * @brief 索引互斥锁，保护索引的并发访问
         */
        mutable std::shared_mutex index_mutex_;

        /**
         * @brief 重复处理策略处理器（自包含，自带互斥锁）
         */
        duplicate_policy_handler_t duplicate_handler_{};

        /**
         * @brief 单例初始化一次性标志（规范 22）
         */
        static std::once_flag once_flag_;

        /**
         * @brief 注册表变更纪元（epoch）
         * @details 任何注册/注销变更均 fetch_add+1（release 序），用于驱动线程本地
         *          元数据缓存失效检测。读取使用 acquire 序，与 release 配对，
         *          保证缓存重建时能看到所有写操作结果。
         */
        static std::atomic<uint64_t> epoch_counter_;

        /**
         * @brief 注册表变更后调用，bump 纪元使所有线程本地缓存失效
         */
        void bump_epoch_() noexcept {
            epoch_counter_.fetch_add(1, std::memory_order_release);
        }

        error_registry_t() = default;

        ~error_registry_t() noexcept = default;

        /**
         * @brief 为批量注册提前预留索引容量
         * @param additional_entries 新增条目数量
         */
        void reserve_for_registration_(size_t additional_entries) noexcept;

        /**
         * @brief 按模块组预分配 module_index_ 桶容量
         * @details 统计每个模块组的错误码数量，对 module_index_ 中对应桶 reserve，
         *          避免 push_back 时多次重分配。分配失败时静默继续（push_back 仍可工作）。
         * @param codes 待注册的错误码列表
         */
        void preallocate_module_buckets_(const std::vector<error_code_t>& codes) noexcept;

        /**
         * @brief 注册单个错误码到所有索引（已持有锁）
         * @details 处理重复策略、移除旧条目、插入新条目到 primary_index_ / name_index_ /
         *          module_index_ / subsystem_index_。分配失败时记录日志并返回 false。
         * @param code 错误码
         * @param name 错误码名称
         * @param description 错误码描述
         * @return bool true=注册成功，false=跳过（重复策略拒绝或分配失败）
         */
        bool register_single_entry_(error_code_t code, std::string_view name,
                                    std::string_view description) noexcept;

        /**
         * @brief 从模块索引中移除指定错误码
         * @param module_group_id 模块组 ID
         * @param identity_code 错误码 identity
         */
        void erase_from_module_index_(module_group_id_t module_group_id, code_t identity_code) noexcept;

        /**
         * @brief 从子系统索引中移除空的模块组条目
         * @param subsystem_id 子系统 ID
         * @param module_group_id 模块组 ID
         */
        void erase_from_subsystem_index_(uint16_t subsystem_id, module_group_id_t module_group_id) noexcept;

    public:
        error_registry_t(const error_registry_t&) = delete;

        error_registry_t& operator=(const error_registry_t&) = delete;

        error_registry_t(error_registry_t&&) = delete;

        error_registry_t& operator=(error_registry_t&&) = delete;

        /**
         * @brief 注册错误码
         * @param code 错误码
         * @param name 错误码宏名称
         * @param description 错误码中文描述
         */
        void register_error(const error_code_t code,
                            const std::string_view name,
                            const std::string_view description) noexcept;

        /**
         * @brief 批量注册错误码
         * @param codes 错误码数组
         * @param names 错误码宏名称数组
         * @param descriptions 错误码中文描述数组
         * @return size_t 实际注册成功的错误码数量
         * @note 如果数组长度不一致，返回0且不执行任何注册
         */
        [[nodiscard]] size_t register_errors(const std::vector<error_code_t>& codes,
                               const std::vector<std::string_view>& names,
                               const std::vector<std::string_view>& descriptions) noexcept;

        /**
         * @brief 注销错误码
         * @details 按错误码值注销，若不存在则静默忽略（无错误返回）
         * @param code 错误码
         */
        void unregister_error(const error_code_t code) noexcept;

        /**
         * @brief 注销错误码
         * @details 按名称注销，若不存在则静默忽略
         * @param name 错误码宏名称
         */
        void unregister_error(const std::string_view name) noexcept;

        /**
         * @brief 注销模块组的所有错误码
         * @details 同步清除 code_index_/name_index_/module_index_ 中该模块的所有条目，
         *          已注册的错误码数量不影响性能（O(模块内错误数)）
         * @param module_group_id 模块组 ID
         */
        void unregister_module(const module_group_id_t module_group_id) noexcept;

        /**
         * @brief 注销所有错误码
         */
        void unregister_all() noexcept;

        /**
         * @brief 检查错误码是否已注册
         * @param code 错误码
         * @return true 错误码已注册
         * @return false 错误码未注册
         */
        [[nodiscard]] bool is_registered(const error_code_t code) const noexcept;

        /**
         * @brief 通过 64位错误码 获取详情（值副本，线程安全）
         * @details 返回值副本而非指针，避免锁释放后被另一线程注销导致 use-after-free
         * @param code 错误码
         * @return std::optional<error_metadata_t> 错误码元数据副本，若未注册则返回 nullopt
         */
        [[nodiscard]] std::optional<error_metadata_t> get_info(const error_code_t code) const noexcept;

        /**
         * @brief 通过模块 ID 获取所有错误码（值副本，线程安全）
         * @details 返回值副本而非引用，避免锁释放后被另一线程注销导致悬垂引用
         * @param module_group_id 模块组 ID
         * @return std::vector<error_metadata_t> 模块下所有错误码的元数据副本
         */
        std::vector<error_metadata_t>
        get_errors_by_module(const module_group_id_t module_group_id) const noexcept;

        /**
         * @brief 通过子系统 ID 获取该子系统下所有错误码（值副本，线程安全）
         * @details 返回值副本而非引用，避免锁释放后被另一线程注销导致悬垂引用
         * @param subsystem_id 子系统 ID
         * @return std::vector<error_metadata_t> 子系统下所有错误码的元数据副本
         */
        std::vector<error_metadata_t>
        get_errors_by_subsystem(uint16_t subsystem_id) const noexcept;

        /**
         * @brief 通过错误码名称查找错误码
         * @param name 错误码名称
         * @return std::optional<error_code_t> 错误码，若未注册则返回空可选
         */
        [[nodiscard]] std::optional<error_code_t> find_by_name(const std::string_view name) const noexcept;

        /**
         * @brief 设置重复处理策略
         * @param policy 重复处理策略
         * @note 转发至 duplicate_handler_，保留接口以最小化调用方改动
         */
        void set_duplicate_policy(duplicate_policy_t policy) noexcept {
            duplicate_handler_.set_policy(policy);
        }

        /**
         * @brief 获取当前重复处理策略
         * @return duplicate_policy_t 当前重复处理策略
         * @note 转发至 duplicate_handler_，保留接口以最小化调用方改动
         */
        duplicate_policy_t get_duplicate_policy() const noexcept {
            return duplicate_handler_.get_policy();
        }

        /**
         * @brief 设置重复注册警告回调
         * @param callback 回调函数，签名为 void(code_t, const error_metadata_t&)
         * @note 传入 nullptr 可清除回调；转发至 duplicate_handler_
         */
        void set_duplicate_warn_callback(duplicate_warn_callback_t callback) noexcept {
            duplicate_handler_.set_warn_callback(std::move(callback));
        }

        /**
         * @brief 获取当前重复注册警告回调
         * @return const duplicate_warn_callback_t& 当前回调
         * @note 转发至 duplicate_handler_，保留接口以最小化调用方改动
         */
        const duplicate_warn_callback_t& get_duplicate_warn_callback() const noexcept {
            return duplicate_handler_.get_warn_callback();
        }

        /**
         * @brief 获取错误码注册器实例
         * @return error_registry_t& 错误码注册器实例
         */
        static error_registry_t& instance() noexcept;

        /**
         * @brief 获取当前注册表纪元（用于缓存失效检测）
         * @details 任何 register/unregister 调用都会使纪元自增。线程本地缓存
         *          在纪元不一致时整体失效重建。读取使用 acquire 序，与
         *          bump_epoch_ 的 release 序配对。
         * @return uint64_t 当前纪元值
         */
        [[nodiscard]] uint64_t get_epoch() const noexcept {
            return epoch_counter_.load(std::memory_order_acquire);
        }

        /**
         * @brief 线程本地缓存的元数据查询（热路径优化）
         * @details 使用 thread_local 环形缓存（容量 16），缓存命中时零锁开销。
         *          注册表任何变更（register/unregister）会 bump 纪元，
         *          缓存检测到纪元变化时整体失效重建。
         *          缓存同时记录"未注册"结果（nullopt），避免对未注册码重复查询。
         *          适用于错误码注册后基本不变的运行时热路径场景。
         * @note 缓存为 thread_local，每个线程独立，无需加锁。
         * @param code 错误码
         * @return std::optional<error_metadata_t> 元数据副本，未注册返回 nullopt
         */
        std::optional<error_metadata_t> get_info_cached(const error_code_t code) const noexcept;

        /**
         * @brief 清除当前线程的元数据缓存
         * @details 仅用于测试与显式刷新场景。正常运行时无需调用，
         *          纪元机制会自动处理缓存失效。
         */
        void invalidate_metadata_cache() const noexcept;
    };

    /**
     * @brief 错误码自动注册辅助类
     * @details 配合 DEFINE_ERROR_CODE 宏使用，利用静态初始化在 main 函数前注册错误码。
     *          子系统/模块名称需通过 i18n::subsystem_module_catalog_t::instance() 单独注册。
     */
    struct error_registrar_t {
        /**
         * @brief 构造函数（自动注册错误码）
         * @param code 错误码
         * @param name 错误码宏名称
         * @param description 错误码中文描述
         * @param subsystem_name 子系统名称（已废弃，子统/模块名称请通过 i18n::subsystem_module_catalog_t 注册）
         * @param module_name 模块名称（已废弃，子统/模块名称请通过 i18n::subsystem_module_catalog_t 注册）
         */
        error_registrar_t(const error_code_t code,
                          const char* name,
                          const char* description,
                          [[maybe_unused]] const char* subsystem_name = "未知子系统",
                          [[maybe_unused]] const char* module_name = "未知模块") noexcept {
            error_registry_t::instance().register_error(code, name, description);
        }

        error_registrar_t(const error_registrar_t&) = delete;
        error_registrar_t& operator=(const error_registrar_t&) = delete;
        error_registrar_t(error_registrar_t&&) = delete;
        error_registrar_t& operator=(error_registrar_t&&) = delete;
    };

}  // namespace error_system::core

/**
 * @brief 定义并自动注册错误码的宏
 * @param NAME 错误码名称（宏名）
 * @param LEVEL 错误等级 (error_level_t)
 * @param SYSTEM 系统域 (system_domain_t)
 * @param SUBSYS 子系统 ID
 * @param MODULE 模块 ID
 * @param NUMBER 错误编号
 * @param DESC 错误描述字符串
 * @param SUBSYS_NAME 子系统名称（已废弃，保留参数以向后兼容；请通过 i18n::subsystem_module_catalog_t 注册）
 * @param MODULE_NAME 模块名称（已废弃，保留参数以向后兼容；请通过 i18n::subsystem_module_catalog_t 注册）
 * @details 该宏会：
 *          1. 创建一个 constexpr error_code_t 常量（编译期可用，无初始化顺序问题）
 *          2. 在动态初始化阶段自动将错误码注册到 error_registry_t
 * @note 子系统/模块名称不再由本宏自动注册，需调用方通过
 *       i18n::subsystem_module_catalog_t::instance().register_subsystem_module(...) 单独注册
 * @note 必须在全局命名空间使用
 * @note 静态初始化顺序安全性：
 *       - error_registry_t 单例使用 std::call_once + 函数局部静态，跨 TU 安全；
 *       - registrar 使用 C++17 inline 变量，全程序仅初始化一次；
 *       - constexpr error_code_t 常量为编译期决议，无 SIOF 风险；
 *       - 但请勿在其它 TU 的静态初始化代码中查询注册表（如
 *         static auto meta = registry.get_info(MY_CODE)），因为跨 TU 动态
 *         初始化顺序未指定。运行时查询（如 error_context_t 构造）不受影响，
 *         因其发生在 main() 之后所有静态初始化完成时。
 * @example
 * DEFINE_ERROR_CODE(
 *     ERR_DB_CONNECTION_TIMEOUT,
 *     error_system::core::error_level_t::error,
 *     error_system::domain::system_domain_t::database,
 *     1, 1, 0x0001,
 *     "数据库连接超时",
 *     "数据库服务",
 *     "连接管理")
 */
#define DEFINE_ERROR_CODE(NAME, LEVEL, SYSTEM, SUBSYS, MODULE, NUMBER, DESC, SUBSYS_NAME, MODULE_NAME)                  \
    constexpr ::error_system::core::error_code_t NAME(LEVEL, SYSTEM, SUBSYS, MODULE, NUMBER);                           \
    inline const ::error_system::core::error_registrar_t NAME##_registrar_(NAME, #NAME, DESC, SUBSYS_NAME, MODULE_NAME);
