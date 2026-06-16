#include "error_system/plugin/error_router_plugin.h"
#include <cstdio>

namespace error_system::plugin {

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

            if (auto it = specific_handlers_.find(context.get_code().get_code()); it != specific_handlers_.end()) {
                handler = it->second;
            } else if (auto it = module_group_handlers_.find(context.get_code().get_module_group_id());
                       it != module_group_handlers_.end()) {
                handler = it->second;
            } else if (auto it = domain_handlers_.find(context.get_code().get_system());
                       it != domain_handlers_.end()) {
                handler = it->second;
            }
        }

        if (handler) {
            try {
                handler(context);
            } catch (...) {
                std::fprintf(stderr, "[error_router_plugin] handler exception caught and ignored\n");
            }
        }
    }

    /**
     * @brief 按错误码注册处理函数
     * @param code 错误码
     * @param handler 处理函数
     */
    void error_router_plugin_t::register_handler_by_code(core::code_t code, error_handler_t handler) noexcept {
        if (!handler) {
            return;
        }
        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            specific_handlers_[code] = std::move(handler);
        } catch (...) {
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
        } catch (...) {
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
        } catch (...) {
            std::fprintf(stderr, "[error_router_plugin] register_handler_by_domain: std::bad_alloc\n");
        }
    }

    /**
     * @brief 移除按错误码注册的处理函数
     * @param code 错误码
     */
    void error_router_plugin_t::unregister_handler_by_code(core::code_t code) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        specific_handlers_.erase(code);
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
}  // namespace error_system::plugin