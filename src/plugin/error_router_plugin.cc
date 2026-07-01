#include "error_system/plugin/error_router_plugin.h"

/**
 * @file error_router_plugin.cc
 * @brief 错误路由插件实现，按码/域/模块组分发
 * @details 实现按错误码、模块组 ID、系统域三级匹配的处理函数分发，匹配优先级为 码 > 模块组 > 域。
 *          基于 std::call_once 实现线程安全的单例初始化。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */

#include <cstdio>
#include <mutex>

namespace error_system::plugin {

    std::once_flag error_router_plugin_t::once_flag_;

    /**
     * @brief 获取插件名称
     * @details 用于标识插件，注册时若名称重复则替换旧插件
     * @return std::string_view 插件名称
     */
    std::string_view error_router_plugin_t::name() const noexcept {
        return name_;
    }

    /**
     * @brief 错误事件回调
     * @details 当一个 error_context_t 被创建时触发，实现此方法进行日志/统计等处理
     * @param context 错误上下文（只读）
     */
    void error_router_plugin_t::on_error(const core::error_context_t& context) noexcept {
        error_handler_t handler;

        {
            std::shared_lock<std::shared_mutex> lock(mutex_);

            if (auto it_specific = specific_handlers_.find(context.get_code().get_code()); it_specific != specific_handlers_.end()) {
                handler = it_specific->second;
            } else if (auto it_module = module_group_handlers_.find(context.get_code().get_module_group_id());
                       it_module != module_group_handlers_.end()) {
                handler = it_module->second;
            } else if (auto it_domain = domain_handlers_.find(context.get_code().get_system());
                       it_domain != domain_handlers_.end()) {
                handler = it_domain->second;
            }
        }

        if (handler) {
            try {
                handler(context);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[error_router_plugin] handler exception caught and ignored: %s\n", e.what());
            }
        }
    }

    /**
     * @brief 按错误码注册处理函数
     * @param code 错误码
     * @param handler 处理函数
     */
    void error_router_plugin_t::register_handler_by_code(const core::error_code_t& code,
                                                       error_handler_t handler) noexcept {
        if (!handler) {
            return;
        }
        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            specific_handlers_[code.get_code()] = std::move(handler);
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_router_plugin] register_handler_by_code: std::bad_alloc\n");
        }
    }

    /**
     * @brief 按模块组 ID 注册处理函数
     * @param module_group_id 模块组 ID
     * @param handler 处理函数
     */
    void error_router_plugin_t::register_handler_by_module_group_id(core::module_group_id_t module_group_id,
                                                                    error_handler_t handler) noexcept {
        if (!handler) {
            return;
        }
        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            module_group_handlers_[module_group_id] = std::move(handler);
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_router_plugin] register_handler_by_module_group_id: std::bad_alloc\n");
        }
    }

    /**
     * @brief 按系统域注册处理函数
     * @param domain 系统域
     * @param handler 处理函数
     */
    void error_router_plugin_t::register_handler_by_domain(domain::system_domain_t domain,
                                                           error_handler_t handler) noexcept {
        if (!handler) {
            return;
        }
        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            domain_handlers_[domain] = std::move(handler);
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_router_plugin] register_handler_by_domain: std::bad_alloc\n");
        }
    }

    /**
     * @brief 移除按错误码注册的处理函数
     * @param code 错误码
     */
    void error_router_plugin_t::unregister_handler_by_code(const core::error_code_t& code) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        specific_handlers_.erase(code.get_code());
    }

    /**
     * @brief 移除按模块组 ID 注册的处理函数
     * @param module_group_id 模块组 ID
     */
    void
    error_router_plugin_t::unregister_handler_by_module_group_id(core::module_group_id_t module_group_id) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        module_group_handlers_.erase(module_group_id);
    }

    /**
     * @brief 移除按系统域注册的处理函数
     * @param domain 系统域
     */
    void error_router_plugin_t::unregister_handler_by_domain(domain::system_domain_t domain) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        domain_handlers_.erase(domain);
    }

    /**
     * @brief 获取单例实例
     * @details 使用 std::call_once + 函数局部静态保证线程安全的单例初始化
     * @return 单例引用
     */
    error_router_plugin_t& error_router_plugin_t::instance() noexcept {
        static error_router_plugin_t* instance_ptr = nullptr;
        std::call_once(once_flag_, [] {
            static error_router_plugin_t instance;
            instance_ptr = &instance;
        });
        return *instance_ptr;
    }
}  // namespace error_system::plugin