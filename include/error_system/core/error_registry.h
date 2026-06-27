#pragma once
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
// IWYU pragma: begin_exports
#include "error_system/core/duplicate_policy.h"
#include "error_system/core/error_builder.h"
#include "error_system/domain/subsystem_module_registry.h"
// IWYU pragma: end_exports

/**
 * @file error_registry.h
 * @brief 错误码注册器
 * @details 定义错误码注册器，用于注册和查找错误码。重复处理策略委托给 duplicate_policy_handler_t，
 *          子系统/模块名称映射委托给 subsystem_module_registry_t，遵循单一职责原则。
 * @author yiice
 * @version 2.3.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误码元数据信息 (数据负载)
     */
    struct error_metadata_t {
        std::string name;
        std::string description;
        uint16_t module_id{0};
        uint16_t error_number{0};
        error_level_t level{error_level_t::info};
    };

    /**
     * @brief 错误码注册器
     * @details 用于注册和查找错误码
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
         * @details key = subsys_id，value = 该子系统下的所有 module_group_id（去重）
         */
        std::unordered_map<uint16_t, std::unordered_set<module_group_id_t>> subsystem_index_;

        /**
         * @brief 索引互斥锁，保护索引的并发访问
         */
        mutable std::shared_mutex index_mutex_;

        /**
         * @brief 重复处理策略处理器（自包含，自带互斥锁）
         */
        duplicate_policy_handler_t duplicate_handler_;

        /**
         * @brief 子系统/模块名称注册器（自包含，自带互斥锁）
         */
        subsystem_module_registry_t subsystem_module_registry_;

        /**
         * @brief 单例初始化一次性标志（规范 22）
         */
        static std::once_flag once_flag_;

        error_registry_t() = default;

        ~error_registry_t() noexcept = default;

        /**
         * @brief 为批量注册提前预留索引容量
         * @param additional_entries 新增条目数量
         */
        void reserve_for_registration_(size_t additional_entries) noexcept;

        /**
         * @brief 从模块索引中移除指定错误码
         * @param module_group_id 模块组 ID
         * @param identity_code 错误码 identity
         */
        void erase_from_module_index_(module_group_id_t module_group_id, code_t identity_code) noexcept;

        /**
         * @brief 从子系统索引中移除空的模块组条目
         * @param subsys_id 子系统 ID
         * @param module_group_id 模块组 ID
         */
        void erase_from_subsystem_index_(uint16_t subsys_id, module_group_id_t module_group_id) noexcept;

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
        size_t register_errors(const std::vector<error_code_t>& codes,
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
        bool is_registered(const error_code_t code) const noexcept;

        /**
         * @brief 通过 64位错误码 获取详情（值副本，线程安全）
         * @details 返回值副本而非指针，避免锁释放后被另一线程注销导致 use-after-free
         * @param code 错误码
         * @return std::optional<error_metadata_t> 错误码元数据副本，若未注册则返回 nullopt
         */
        std::optional<error_metadata_t> get_info(const error_code_t code) const noexcept;

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
         * @param subsys_id 子系统 ID
         * @return std::vector<error_metadata_t> 子系统下所有错误码的元数据副本
         */
        std::vector<error_metadata_t>
        get_errors_by_subsystem(uint16_t subsys_id) const noexcept;

        /**
         * @brief 通过错误码名称查找错误码
         * @param name 错误码名称
         * @return std::optional<error_code_t> 错误码，若未注册则返回空可选
         */
        std::optional<error_code_t> find_by_name(const std::string_view name) const noexcept;

        /**
         * @brief 注册子系统/模块名称
         * @param subsys_id 子系统 ID
         * @param module_id 模块 ID
         * @param subsystem_name 子系统名称
         * @param module_name 模块名称
         * @note 转发至 subsystem_module_registry_，保留接口以最小化调用方改动
         */
        void register_subsystem_module(uint16_t subsys_id,
                                       uint16_t module_id,
                                       const std::string_view subsystem_name,
                                       const std::string_view module_name) noexcept {
            subsystem_module_registry_.register_subsystem_module(subsys_id, module_id, subsystem_name, module_name);
        }

        /**
         * @brief 查询子系统/模块名称（值副本，线程安全）
         * @param subsys_id 子系统 ID
         * @param module_id 模块 ID
         * @return subsystem_module_info_t 子系统/模块名称信息副本
         * @note 转发至 subsystem_module_registry_，保留接口以最小化调用方改动
         */
        subsystem_module_info_t get_subsystem_module_info(uint16_t subsys_id, uint16_t module_id) const noexcept {
            return subsystem_module_registry_.get_subsystem_module_info(subsys_id, module_id);
        }

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
    };

    /**
     * @brief 错误码自动注册辅助类
     * @details 配合 DEFINE_ERROR_CODE 宏使用，利用静态初始化在 main 函数前注册错误码
     *          同时自动注册对应的子系统/模块名称
     */
    struct error_registrar_t {
        /**
         * @brief 构造函数（自动注册错误码及子系统/模块名称）
         * @param code 错误码
         * @param name 错误码宏名称
         * @param desc 错误码中文描述
         * @param subsys_name 子系统名称
         * @param module_name 模块名称
         */
        error_registrar_t(const error_code_t code,
                          const char* name,
                          const char* desc,
                          const char* subsys_name = "未知子系统",
                          const char* module_name = "未知模块") noexcept {
            error_registry_t::instance().register_subsystem_module(
                code.get_subsys(), code.get_module(), subsys_name, module_name);
            error_registry_t::instance().register_error(code, name, desc);
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
 * @param SUBSYS_NAME 子系统名称（用于 to_string() 可读输出）
 * @param MODULE_NAME 模块名称（用于 to_string() 可读输出）
 * @details 该宏会：
 *          1. 创建一个 constexpr error_code_t 常量
 *          2. 在静态初始化阶段自动将错误码注册到 error_registry_t
 *          3. 同时注册子系统/模块名称到 error_registry_t 的映射表
 * @note 必须在全局命名空间使用
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
