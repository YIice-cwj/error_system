#include "error_system/plugin/plugin_registry.h"
#include <iostream>

namespace error_system::plugin {

    /**
     * @brief 注册插件
     * @details 若已存在同名插件，替换旧插件
     */
    void plugin_registry_t::register_plugin(i_error_plugin_t* plugin) noexcept {
        if (!plugin) {
            return;
        }

        std::unique_lock<std::shared_mutex> lock(plugins_mutex_);

        auto it = std::find_if(plugins_.begin(), plugins_.end(), [&](const i_error_plugin_t* registered_plugin) {
            return registered_plugin->name() == plugin->name();
        });

        if (it != plugins_.end()) {
            *it = plugin;
        } else {
            plugins_.push_back(plugin);
        }
    }

    /**
     * @brief 注销插件
     */
    void plugin_registry_t::unregister_plugin(std::string_view name) noexcept {
        std::unique_lock<std::shared_mutex> lock(plugins_mutex_);
        plugins_.erase(std::remove_if(plugins_.begin(),
                                      plugins_.end(),
                                      [&](const i_error_plugin_t* registered_plugin) {
                                          return registered_plugin->name() == name;
                                      }),
                       plugins_.end());
    }

    /**
     * @brief 通知所有插件发生了错误事件
     */
    void plugin_registry_t::notify_error(const core::error_context_t& context) noexcept {
        std::shared_lock<std::shared_mutex> lock(plugins_mutex_);
        for (auto* registered_plugin : plugins_) {
            try {
                registered_plugin->on_error(context);
            } catch (const std::exception& e) {
                std::cerr << "[Plugin Error] Plugin '" << registered_plugin->name()
                          << "' threw an exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[Plugin Error] Plugin '" << registered_plugin->name() << "' threw an unknown exception."
                          << std::endl;
            }
        }
    }

    /**
     * @brief 获取已注册插件数量
     * @return size_t 已注册插件数量
     */
    size_t plugin_registry_t::size() const noexcept {
        std::shared_lock<std::shared_mutex> lock(plugins_mutex_);
        return plugins_.size();
    }

    /**
     * @brief 判断是否有已注册的插件
     * @return bool 是否有已注册的插件
     */
    bool plugin_registry_t::empty() const noexcept {
        std::shared_lock<std::shared_mutex> lock(plugins_mutex_);
        return plugins_.empty();
    }

    /**
     * @brief 清空所有已注册插件
     */
    void plugin_registry_t::clear() noexcept {
        std::unique_lock<std::shared_mutex> lock(plugins_mutex_);
        plugins_.clear();
    }

    /**
     * @brief 获取单例实例
     * @return plugin_registry_t& 单例引用
     */
    plugin_registry_t& plugin_registry_t::instance() noexcept {
        static plugin_registry_t instance;
        return instance;
    }

}  // namespace error_system::plugin
