#pragma once
#include <functional>
// IWYU pragma: begin_exports
#include <mutex>
// IWYU pragma: end_exports
#include <shared_mutex>
#include <unordered_map>

#include "error_system/core/error_code.h"
#include "error_system/plugin/i_error_plugin.h"

/**
 * @file error_router_plugin.h
 * @brief 错误路由插件
 * @details 错误路由插件负责将错误事件路由到对应的插件进行处理，
 *          例如：日志记录、统计分析、告警通知等
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::plugin {

    /**
     * @brief 处理函数类型
     * @param error_context 错误上下文
     */
    using error_handler_t = std::function<void(const core::error_context_t&)>;

    /**
     * @brief 错误路由插件
     * @details 错误路由插件负责将错误事件路由到对应的处理函数
     */
    class error_router_plugin_t : public i_error_plugin_t {
    private:
        std::unordered_map<core::code_t, error_handler_t> specific_handlers_{};

        std::unordered_map<core::module_group_id_t, error_handler_t> module_group_handlers_{};

        std::unordered_map<domain::system_domain_t, error_handler_t> domain_handlers_{};

        std::string name_{"global_error_router"};

        mutable std::shared_mutex mutex_;

    private:
        error_router_plugin_t() = default;

        error_router_plugin_t(const error_router_plugin_t&) = delete;

        error_router_plugin_t(error_router_plugin_t&&) = delete;

        error_router_plugin_t& operator=(const error_router_plugin_t&) = delete;

        error_router_plugin_t& operator=(error_router_plugin_t&&) = delete;

    public:
        /**
         * @brief 获取插件名称
         * @details 用于标识插件，注册时若名称重复则替换旧插件
         * @return std::string_view 插件名称
         */
        std::string_view name() const noexcept override;

        /**
         * @brief 错误事件回调
         * @details 当一个 error_context_t 被创建时触发，实现此方法进行日志/统计等处理
         * @param context 错误上下文（只读）
         */
        void on_error(const core::error_context_t& context) noexcept override;

        /**
         * @brief 按错误码注册处理函数
         * @param code 错误码
         * @param handler 处理函数
         */
        void register_handler_by_code(const core::error_code_t& code, error_handler_t handler) noexcept;

        /**
         * @brief 按模块组 ID 注册处理函数
         * @param module_group_id 模块组 ID
         * @param handler 处理函数
         */
        void register_handler_by_module_group_id(core::module_group_id_t module_group_id,
                                                 error_handler_t handler) noexcept;

        /**
         * @brief 按系统域注册处理函数
         * @param domain 系统域
         * @param handler 处理函数
         */
        void register_handler_by_domain(domain::system_domain_t domain, error_handler_t handler) noexcept;

        /**
         * @brief 移除按错误码注册的处理函数
         * @param code 错误码
         */
        void unregister_handler_by_code(const core::error_code_t& code) noexcept;

        /**
         * @brief 移除按模块组 ID 注册的处理函数
         * @param module_group_id 模块组 ID
         */
        void unregister_handler_by_module_group_id(core::module_group_id_t module_group_id) noexcept;

        /**
         * @brief 移除按系统域注册的处理函数
         * @param domain 系统域
         */
        void unregister_handler_by_domain(domain::system_domain_t domain) noexcept;

    public:
        /**
         * @brief 获取路由插件单例
         * @return error_router_plugin_t& 插件的全局单例引用
         */
        static error_router_plugin_t& instance() noexcept {
            static error_router_plugin_t instance;
            return instance;
        }
    };
}  // namespace error_system::plugin