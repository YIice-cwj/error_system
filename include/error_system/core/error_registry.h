#pragma once
#include "error_system/core/error_code.h"
#include "error_system/core/error_level.h"
// IWYU pragma: begin_exports
#include "error_system/core/error_builder.h"
#include <iosfwd>
#include <mutex>
// IWYU pragma: end_exports
#include <functional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * @file error_registry.h
 * @brief 错误码注册器
 * @details 定义错误码注册器，用于注册和查找错误码
 * @author yiice
 * @version 1.0.0
 * @date 2026-04-27
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
     * @brief 错误码元数据信息 (数据负载)
     */
    struct error_metadata_t {
        std::string name;
        std::string description;
        uint16_t module_id;
        uint16_t error_number;
        error_level_t level;
    };

    /**
     * @brief 重复处理策略
     * @details 定义当错误码已存在时的处理行为
     */
    enum class duplicate_policy_t : uint8_t {
        skip,       // 静默跳过（默认）
        overwrite,  // 覆盖已有定义
        warn,       // 跳过但记录警告
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
         * @brief 子系统/模块名称映射表
         * @details key = (subsys_id << 16) | module_id，避免每个错误码重复存储名称
         */
        std::unordered_map<uint32_t, subsystem_module_info_t> subsystem_module_index_;

        /**
         * @brief 索引互斥锁，保护索引的并发访问
         */
        mutable std::shared_mutex index_mutex_;

        /**
         * @brief 重复处理策略
         */
        duplicate_policy_t duplicate_policy_{duplicate_policy_t::skip};

        /**
         * @brief 重复注册警告回调函数
         * @details 当策略为 warn 时调用，参数为 (错误码原始值, 已存在的元数据)。
         *          默认输出到 std::cerr，可通过 set_duplicate_warn_callback 覆盖。
         */
        std::function<void(code_t, const error_metadata_t&)> duplicate_warn_callback_{nullptr};

        private:
        error_registry_t();

        ~error_registry_t() = default;

        public:
        error_registry_t(const error_registry_t&) = delete;

        error_registry_t& operator=(const error_registry_t&) = delete;

        error_registry_t(error_registry_t&&) = delete;

        error_registry_t& operator=(error_registry_t&&) = delete;

        private:
        /**
         * @brief 处理 skip 策略
         * @param raw_code 错误码原始值
         * @return bool 是否继续注册流程（skip 返回 false）
         */
        bool __handle_duplicate_skip(code_t raw_code) noexcept;

        /**
         * @brief 处理 overwrite 策略
         * @param raw_code 错误码原始值
         * @return bool 是否继续注册流程（overwrite 返回 true）
         */
        bool __handle_duplicate_overwrite(code_t raw_code) noexcept;

        /**
         * @brief 处理 warn 策略
         * @param raw_code 错误码原始值
         * @return bool 是否继续注册流程（warn 返回 false）
         */
        bool __handle_duplicate_warn(code_t raw_code) noexcept;

        /**
         * @brief 根据当前策略处理重复错误码
         * @param raw_code 错误码原始值
         * @return bool 是否继续注册流程
         */
        bool __apply_duplicate_policy(code_t raw_code) noexcept;

        /**
         * @brief 为批量注册提前预留索引容量
         * @param additional_entries 新增条目数量
         */
        void __reserve_for_registration(size_t additional_entries) noexcept;

        /**
         * @brief 从模块索引中移除指定错误码
         * @param module_group_id 模块组 ID
         * @param identity_code 错误码 identity
         */
        void __erase_from_module_index(module_group_id_t module_group_id, code_t identity_code) noexcept;

        public:
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
         * @param code 错误码
         */
        void unregister_error(const error_code_t code) noexcept;

        /**
         * @brief 注销错误码
         * @param name 错误码宏名称
         */
        void unregister_error(const std::string_view name) noexcept;

        /**
         * @brief 注销模块组的所有错误码
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
         * @brief 通过 64位错误码 获取详情
         * @param code 错误码
         * @return std::optional<std::reference_wrapper<const error_metadata_t>> 错误码元数据，若未注册则返回空可选
         */
        std::optional<std::reference_wrapper<const error_metadata_t>> get_info(const error_code_t code) const noexcept;

        /**
         * @brief 通过模块 ID 获取所有错误码
         * @param module_group_id 模块组 ID
         * @return std::vector<std::reference_wrapper<const error_metadata_t>> 模块下所有错误码的元数据
         */
        std::vector<std::reference_wrapper<const error_metadata_t>>
        get_errors_by_module(const module_group_id_t module_group_id) const noexcept;

        /**
         * @brief 注册子系统/模块名称
         * @param subsys_id 子系统 ID
         * @param module_id 模块 ID
         * @param subsystem_name 子系统名称
         * @param module_name 模块名称
         */
        void register_subsystem_module(uint16_t subsys_id,
                                       uint16_t module_id,
                                       const std::string_view subsystem_name,
                                       const std::string_view module_name) noexcept;

        /**
         * @brief 查询子系统/模块名称
         * @param subsys_id 子系统 ID
         * @param module_id 模块 ID
         * @return const subsystem_module_info_t& 子系统/模块名称信息
         */
        const subsystem_module_info_t& get_subsystem_module_info(uint16_t subsys_id, uint16_t module_id) const noexcept;

        public:
        /**
         * @brief 设置重复处理策略
         * @param policy 重复处理策略
         */
        void set_duplicate_policy(duplicate_policy_t policy) noexcept {
            std::unique_lock<std::shared_mutex> lock(index_mutex_);
            duplicate_policy_ = policy;
        }

        /**
         * @brief 获取当前重复处理策略
         * @return duplicate_policy_t 当前重复处理策略
         */
        duplicate_policy_t get_duplicate_policy() const noexcept {
            std::shared_lock<std::shared_mutex> lock(index_mutex_);
            return duplicate_policy_;
        }

        /**
         * @brief 设置重复注册警告回调
         * @param callback 回调函数，签名为 void(code_t, const error_metadata_t&)
         * @note 传入 nullptr 可清除回调
         */
        void set_duplicate_warn_callback(std::function<void(code_t, const error_metadata_t&)> callback) noexcept {
            std::unique_lock<std::shared_mutex> lock(index_mutex_);
            duplicate_warn_callback_ = std::move(callback);
        }

        /**
         * @brief 获取当前重复注册警告回调
         * @return const std::function<void(code_t, const error_metadata_t&)>& 当前回调
         */
        const std::function<void(code_t, const error_metadata_t&)>& get_duplicate_warn_callback() const noexcept {
            std::shared_lock<std::shared_mutex> lock(index_mutex_);
            return duplicate_warn_callback_;
        }

        /**
         * @brief 获取错误码注册器实例
         * @return error_registry_t& 错误码注册器实例
         */
        static error_registry_t& instance() noexcept {
            static error_registry_t instance_;
            return instance_;
        }
    };

    /**
     * @brief 错误码自动注册辅助类
     * @details 配合 DEFINE_ERROR_CODE 宏使用，利用静态初始化在 main 函数前注册错误码
     */
    struct error_registrar_t {
        error_registrar_t(const error_code_t code,
                          const char* name,
                          const char* desc,
                          const char* subsys_name = "未知子系统",
                          const char* module_name = "未知模块") noexcept {
            error_registry_t::instance().register_subsystem_module(
                code.get_subsys(), code.get_module(), subsys_name, module_name);
            error_registry_t::instance().register_error(code, name, desc);
        }
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
 * @details 该宏会：
 *          1. 创建一个 constexpr error_code_t 常量
 *          2. 在静态初始化阶段自动将错误码注册到 error_registry_t
 * @note 必须在全局命名空间使用
 * @example
 * DEFINE_ERROR_CODE(
 *     ERR_DB_CONNECTION_TIMEOUT,
 *     error_system::core::error_level_t::error,
 *     error_system::domain::system_domain_t::database,
 *     1, 1, 0x0001,
 *     "数据库连接超时")
 */
#define DEFINE_ERROR_CODE(NAME, LEVEL, SYSTEM, SUBSYS, MODULE, NUMBER, DESC)                                           \
    constexpr ::error_system::core::error_code_t NAME =                                                                \
        ::error_system::core::error_builder_t::make_error_code(LEVEL, SYSTEM, SUBSYS, MODULE, NUMBER);                 \
    inline const ::error_system::core::error_registrar_t NAME##_registrar_(NAME, #NAME, DESC);
