#pragma once
#include "error_system/core/error_code.h"
#include "error_system/core/error_level.h"
// IWYU pragma: begin_exports
#include "error_system/core/error_builder.h"
#include <mutex>
// IWYU pragma: end_exports
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
     * @brief 错误码元数据信息 (数据负载)
     */
    struct error_metadata_t {
        std::string name;         // 宏名称 (例: "ERR_TCP_TIMEOUT")
        std::string description;  // 中文描述 (例: "TCP 连接超时")
        uint16_t module_id;       // 所属模块 ID
        uint16_t error_number;    // 局部错误编号
        error_level_t level;      // 错误等级
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
         * @brief 模块索引，根据模块 ID快速查找错误码
         */
        std::unordered_map<module_group_id_t, std::vector<code_t>> module_index_;

        /**
         * @brief 索引互斥锁，保护索引的并发访问
         */
        mutable std::shared_mutex index_mutex_;

        private:
        error_registry_t() = default;

        ~error_registry_t() = default;

        public:
        error_registry_t(const error_registry_t&) = delete;

        error_registry_t& operator=(const error_registry_t&) = delete;

        error_registry_t(error_registry_t&&) = delete;

        error_registry_t& operator=(error_registry_t&&) = delete;

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
         */
        void register_errors(const std::vector<error_code_t>& codes,
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
         * @return std::optional<error_metadata_t> 错误码元数据，若未注册则返回空可选
         */
        std::optional<error_metadata_t> get_info(const error_code_t code) const noexcept;

        /**
         * @brief 通过模块 ID 获取所有错误码
         * @param module_group_id 模块组 ID
         * @return std::vector<error_metadata_t> 模块下所有错误码的元数据
         */
        std::vector<error_metadata_t> get_errors_by_module(const module_group_id_t module_group_id) const noexcept;

        /**
         * @brief 通过系统、子系统、模块获取所有错误码
         * @tparam SystemEnum 系统枚举类型
         * @tparam SubSystemEnum 子系统枚举类型
         * @tparam ModuleEnum 模块枚举类型
         * @param system 系统枚举值
         * @param subsystem 子系统枚举值
         * @param module 模块枚举值
         * @return std::vector<error_metadata_t> 模块下所有错误码的元数据
         */
        template <typename SystemEnum, typename SubSystemEnum, typename ModuleEnum>
        std::vector<error_metadata_t> get_errors_by_module(const SystemEnum system,
                                                           const SubSystemEnum subsystem,
                                                           const ModuleEnum module) const noexcept {
            return get_errors_by_module(error_builder_t::make_error_code(error_level_t::info, system, subsystem, module, 0).get_module_group_id());
        }

        public:
        /**
         * @brief 获取错误码注册器实例
         * @return error_registry_t& 错误码注册器实例
         */
        static error_registry_t& instance() noexcept {
            static error_registry_t instance_;
            return instance_;
        }
    };

}  // namespace error_system::core
