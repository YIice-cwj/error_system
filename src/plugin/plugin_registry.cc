#include "error_system/plugin/plugin_registry.h"

#include <cstdio>

#include <algorithm>
#include <mutex>

namespace error_system::plugin {

    std::once_flag plugin_registry_t::once_flag_;

    plugin_registry_t::plugin_registry_t() noexcept
        : notification_channel_([this](const core::error_context_t& context) {
            notify_error(context);
        }) {}

    void plugin_registry_t::register_plugin(std::unique_ptr<i_error_plugin_t> plugin) noexcept {
        if (!plugin) {
            return;
        }

        auto shared_plugin = std::shared_ptr<i_error_plugin_t>(std::move(plugin));
        update_snapshot_([&shared_plugin](plugin_list_t& snapshot,
                                          std::vector<shared_plugin_ptr_t>& owned) {
            auto name = shared_plugin->name();
            auto it = std::find_if(snapshot.begin(), snapshot.end(),
                [&name](const shared_plugin_ptr_t& p) { return p->name() == name; });
            if (it != snapshot.end()) {
                // 同名替换：从 owned 中移除旧插件，再替换快照项
                auto* old_ptr = it->get();
                owned.erase(std::remove_if(owned.begin(), owned.end(),
                                [old_ptr](const shared_plugin_ptr_t& p) { return p.get() == old_ptr; }),
                            owned.end());
                *it = shared_plugin;
            } else {
                snapshot.push_back(shared_plugin);
            }
            owned.push_back(std::move(shared_plugin));
        });
    }

    void plugin_registry_t::register_plugin_ref(i_error_plugin_t& plugin) noexcept {
        // 非持有引用：创建空删除器的 shared_ptr，不接管所有权
        auto non_owning = std::shared_ptr<i_error_plugin_t>(&plugin, [](i_error_plugin_t*){});
        update_snapshot_([&non_owning](plugin_list_t& snapshot,
                                       std::vector<shared_plugin_ptr_t>&) {
            auto name = non_owning->name();
            auto it = std::find_if(snapshot.begin(), snapshot.end(),
                [&name](const shared_plugin_ptr_t& p) { return p->name() == name; });
            if (it != snapshot.end()) {
                *it = non_owning;
            } else {
                snapshot.push_back(non_owning);
            }
            // 非持有引用不加入 owned_plugins_
        });
    }

    void plugin_registry_t::unregister_plugin(std::string_view name) noexcept {
        update_snapshot_([name](plugin_list_t& snapshot, std::vector<shared_plugin_ptr_t>& owned) {
            auto it = std::find_if(snapshot.begin(), snapshot.end(),
                [name](const shared_plugin_ptr_t& p) { return p->name() == name; });
            if (it == snapshot.end()) {
                return;
            }
            // 按指针匹配同步从 owned 中移除（非持有引用不在 owned 中，匹配不到即无操作）
            auto* old_ptr = it->get();
            snapshot.erase(it);
            owned.erase(std::remove_if(owned.begin(), owned.end(),
                            [old_ptr](const shared_plugin_ptr_t& p) { return p.get() == old_ptr; }),
                        owned.end());
        });
    }

    void plugin_registry_t::notify_error(const core::error_context_t& context) noexcept {
        auto snapshot = std::atomic_load(&plugins_snapshot_);
        for (const auto& plugin : *snapshot) {
            if (context.get_code().get_level() >= plugin->min_level()) {
                try {
                    plugin->on_error(context);
                } catch (...) {
                    auto name = plugin->name();
                    std::fprintf(stderr,
                                 "[plugin_registry] plugin '%.*s' on_error threw exception\n",
                                 static_cast<int>(name.size()), name.data());
                }
            }
        }
    }

    void plugin_registry_t::enqueue_notification(const core::error_context_t& context) noexcept {
        notification_channel_.enqueue_notification(context);
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
        update_snapshot_([](plugin_list_t& snapshot, std::vector<shared_plugin_ptr_t>& owned) {
            snapshot.clear();
            owned.clear();
        });
    }

    size_t plugin_registry_t::pending_notifications() const noexcept {
        return notification_channel_.pending_notifications();
    }

    void plugin_registry_t::set_max_queue_size(size_t max_size) noexcept {
        notification_channel_.set_max_queue_size(max_size);
    }

    size_t plugin_registry_t::get_max_queue_size() const noexcept {
        return notification_channel_.get_max_queue_size();
    }

    plugin_registry_t& plugin_registry_t::instance() noexcept {
        static plugin_registry_t* instance_ptr = nullptr;
        std::call_once(once_flag_, [] {
            static plugin_registry_t instance;
            instance_ptr = &instance;
        });
        return *instance_ptr;
    }

}  // namespace error_system::plugin
