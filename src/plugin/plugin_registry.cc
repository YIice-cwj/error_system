#include "error_system/plugin/plugin_registry.h"
#include <algorithm>

namespace error_system::plugin {

    void plugin_registry_t::register_plugin(i_error_plugin_t* plugin) noexcept {
        if (!plugin) {
            return;
        }

        __update_snapshot([plugin](std::vector<i_error_plugin_t*>& snapshot) {
            auto it = std::find_if(snapshot.begin(), snapshot.end(), [plugin](const i_error_plugin_t* registered) {
                return registered->name() == plugin->name();
            });
            if (it != snapshot.end()) {
                *it = plugin;
            } else {
                snapshot.push_back(plugin);
            }
        });
    }

    void plugin_registry_t::unregister_plugin(std::string_view name) noexcept {
        __update_snapshot([name](std::vector<i_error_plugin_t*>& snapshot) {
            snapshot.erase(std::remove_if(snapshot.begin(), snapshot.end(),
                                          [name](const i_error_plugin_t* plugin) { return plugin->name() == name; }),
                           snapshot.end());
        });
    }

    void plugin_registry_t::notify_error(const core::error_context_t& context) noexcept {
        auto snapshot = std::atomic_load(&plugins_snapshot_);
        for (auto* plugin : *snapshot) {
            if (context.get_code().get_level() >= plugin->min_level()) {
                try {
                    plugin->on_error(context);
                } catch (...) {}
            }
        }
    }

    void plugin_registry_t::enqueue_notification(const core::error_context_t& context) noexcept {
        auto copy = std::make_shared<core::error_context_t>(context);
        async_queue_.enqueue(std::move(copy));
    }

    size_t plugin_registry_t::size() const noexcept {
        auto snapshot = std::atomic_load(&plugins_snapshot_);
        return snapshot->size();
    }

    bool plugin_registry_t::empty() const noexcept {
        auto snapshot = std::atomic_load(&plugins_snapshot_);
        return snapshot->empty();
    }

    void plugin_registry_t::clear() noexcept {
        __update_snapshot([](std::vector<i_error_plugin_t*>& snapshot) { snapshot.clear(); });
    }

    size_t plugin_registry_t::pending_notifications() const noexcept {
        return async_queue_.size();
    }

    plugin_registry_t& plugin_registry_t::instance() noexcept {
        static plugin_registry_t instance;
        return instance;
    }

}  // namespace error_system::plugin