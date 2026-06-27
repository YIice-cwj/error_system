#pragma once
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

/**
 * @file subsystem_module_registry.h
 * @brief 子系统/模块名称注册器
 * @details 维护 (子系统ID, 模块ID) → 名称映射，从 error_registry_t 中拆分以遵循单一职责原则。
 *          自包含（自带互斥锁），可被 error_registry_t 组合使用。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 子系统/模块名称映射
     * @details 通过 (subsys_id << 16 | module_id) 作为 key 存储名称，避免每个错误码重复存储
     */
    struct subsystem_module_info_t {
        std::string subsystem_name{"未知子系统"};
        std::string module_name{"未知模块"};
    };

    /**
     * @brief 子系统/模块名称注册器
     * @details 维护 (subsys_id, module_id) → (subsystem_name, module_name) 的映射表，
     *          用于 to_string() 等可读输出场景。线程安全（自带读写锁）。
     */
    class subsystem_module_registry_t {
    private:
        /**
         * @brief 子系统/模块名称映射表
         * @details key = (subsys_id << 16) | module_id，避免每个错误码重复存储名称
         */
        std::unordered_map<uint32_t, subsystem_module_info_t> subsystem_module_index_;

        /**
         * @brief 映射表互斥锁，保护并发访问
         */
        mutable std::shared_mutex mutex_;

    public:
        subsystem_module_registry_t() = default;

        ~subsystem_module_registry_t() noexcept = default;

        subsystem_module_registry_t(const subsystem_module_registry_t&) = delete;

        subsystem_module_registry_t& operator=(const subsystem_module_registry_t&) = delete;

        subsystem_module_registry_t(subsystem_module_registry_t&&) = delete;

        subsystem_module_registry_t& operator=(subsystem_module_registry_t&&) = delete;

        /**
         * @brief 注册子系统/模块名称
         * @param subsys_id 子系统 ID
         * @param module_id 模块 ID
         * @param subsystem_name 子系统名称
         * @param module_name 模块名称
         * @note 若 (subsys_id, module_id) 已存在则静默跳过（保留首次注册）
         */
        void register_subsystem_module(uint16_t subsys_id,
                                       uint16_t module_id,
                                       const std::string_view subsystem_name,
                                       const std::string_view module_name) noexcept;

        /**
         * @brief 查询子系统/模块名称（值副本，线程安全）
         * @details 返回值副本而非引用，在锁内完成拷贝，避免锁释放后另一线程调用 clear() 导致悬垂引用
         * @param subsys_id 子系统 ID
         * @param module_id 模块 ID
         * @return subsystem_module_info_t 子系统/模块名称信息副本（未注册时返回默认值）
         */
        subsystem_module_info_t get_subsystem_module_info(uint16_t subsys_id, uint16_t module_id) const noexcept;

        /**
         * @brief 清空所有子系统/模块名称映射
         */
        void clear() noexcept;
    };

}  // namespace error_system::core
