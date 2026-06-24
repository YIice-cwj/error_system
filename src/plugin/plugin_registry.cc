#include "error_system/plugin/plugin_registry.h"

#include <cstdio>

#include <algorithm>

namespace error_system::plugin {

    void plugin_registry_t::register_plugin(std::unique_ptr<i_error_plugin_t> plugin) noexcept {
        if (!plugin) {
            return;
        }

        i_error_plugin_t* raw_ptr = plugin.get();
        update_snapshot_([raw_ptr](std::vector<i_error_plugin_t*>& snapshot) {
            auto it = std::find_if(snapshot.begin(), snapshot.end(), [raw_ptr](const i_error_plugin_t* registered) {
                return registered->name() == raw_ptr->name();
            });
            if (it != snapshot.end()) {
                *it = raw_ptr;
            } else {
                snapshot.push_back(raw_ptr);
            }
        });

        auto it = std::find_if(owned_plugins_.begin(), owned_plugins_.end(),
                               [&raw_ptr](const std::unique_ptr<i_error_plugin_t>& owned) {
                                   return owned->name() == raw_ptr->name();
                               });
        if (it != owned_plugins_.end()) {
            *it = std::move(plugin);
        } else {
            owned_plugins_.push_back(std::move(plugin));
        }
    }

    void plugin_registry_t::register_plugin_ref(i_error_plugin_t& plugin) noexcept {
        i_error_plugin_t* raw_ptr = &plugin;
        update_snapshot_([raw_ptr](std::vector<i_error_plugin_t*>& snapshot) {
            auto it = std::find_if(snapshot.begin(), snapshot.end(), [raw_ptr](const i_error_plugin_t* registered) {
                return registered->name() == raw_ptr->name();
            });
            if (it != snapshot.end()) {
                *it = raw_ptr;
            } else {
                snapshot.push_back(raw_ptr);
            }
        });
    }

    void plugin_registry_t::unregister_plugin(std::string_view name) noexcept {
        update_snapshot_([name](std::vector<i_error_plugin_t*>& snapshot) {
            snapshot.erase(std::remove_if(snapshot.begin(), snapshot.end(),
                                          [name](const i_error_plugin_t* plugin) { return plugin->name() == name; }),
                           snapshot.end());
        });
        owned_plugins_.erase(
            std::remove_if(owned_plugins_.begin(), owned_plugins_.end(),
                           [name](const std::unique_ptr<i_error_plugin_t>& owned) { return owned->name() == name; }),
            owned_plugins_.end());
    }

    void plugin_registry_t::notify_error(const core::error_context_t& context) noexcept {
        auto snapshot = std::atomic_load(&plugins_snapshot_);
        for (auto* plugin : *snapshot) {
            if (context.get_code().get_level() >= plugin->min_level()) {
                try {
                    plugin->on_error(context);
                } catch (...) {
                    fprintf(stderr, "[plugin_registry] plugin '%s' on_error threw exception\n",
                            plugin->name().data());
                }
            }
        }
    }

    void plugin_registry_t::enqueue_notification(const core::error_context_t& context) noexcept {
        try {
            auto copy = std::make_shared<core::error_context_t>(context);
            async_queue_.enqueue(std::move(copy));
        } catch (...) {
            fprintf(stderr, "[plugin_registry] enqueue_notification failed to allocate memory\n");
        }
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
        update_snapshot_([](std::vector<i_error_plugin_t*>& snapshot) { snapshot.clear(); });
        owned_plugins_.clear();
    }

    size_t plugin_registry_t::pending_notifications() const noexcept {
        return async_queue_.size();
    }

    void plugin_registry_t::set_max_queue_size(size_t max_size) noexcept {
        async_queue_.set_max_size(max_size);
    }

    size_t plugin_registry_t::get_max_queue_size() const noexcept {
        return async_queue_.max_size();
    }

    plugin_registry_t& plugin_registry_t::instance() noexcept {
        static plugin_registry_t instance;
        return instance;
    }

}  // namespace error_system::plugin