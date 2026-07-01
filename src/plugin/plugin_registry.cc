#include "error_system/plugin/plugin_registry.h"

/**
 * @file plugin_registry.cc
 * @brief 插件注册表单例实现，支持 sync/async_queue/sync_deferred 三种通知模式
 * @details 提供插件注册、注销、错误事件分发能力。snapshot 基于原子读写实现无锁通知路径；
 *          async_queue 通过通知通道异步排队；sync_deferred 通过线程本地缓冲累积后显式 flush。
 *          本类实现 core::i_error_notifier_t 接口，并在 instance() 中自注册为默认通知器，
 *          使 core 层经由抽象接口完成通知，解耦 core→plugin 反向依赖。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */

#include <cstdio>

#include <algorithm>
#include <mutex>
#include <vector>

#include "error_system/core/error_context_initializer.h"

namespace error_system::plugin {

    std::once_flag plugin_registry_t::once_flag_;
    std::atomic<bool> plugin_registry_t::initialized_{false};

    namespace {
        constexpr size_t DEFAULT_DEFERRED_BUFFER_SIZE = 1024;  ///< 默认线程本地延迟缓冲容量

        /**
         * @brief 线程本地延迟通知缓冲
         * @details sync_deferred 模式下累积错误上下文，由调用方显式 flush。
         *          thread_local 避免跨线程同步开销，每个请求处理线程独立缓冲。
         *          缓冲满时丢弃新通知并设置 overflow_dropped 标志。
         */
        struct deferred_buffer_t {
            std::vector<core::error_context_t> buffer;
            size_t max_size{DEFAULT_DEFERRED_BUFFER_SIZE};
            bool overflow_dropped{false};
            bool flushing{false};
        };

        thread_local deferred_buffer_t tls_deferred_;
    }  // namespace

    /**
     * @brief 构造函数
     * @details 初始化通知通道，将通知回调绑定到本对象的 notify_error 方法
     */
    plugin_registry_t::plugin_registry_t() noexcept
        : notification_channel_([this](const core::error_context_t& context) {
            notify_error(context);
        }) {}

    /**
     * @brief 注册插件（转移所有权）
     * @details 将插件包装为 shared_ptr 并加入 snapshot，同名插件替换旧条目。
     *          所有权由 owned 列表持有，保证 snapshot 中的裸指针/弱引用有效。
     * @param plugin 待注册的插件 unique_ptr，空指针将被忽略
     */
    void plugin_registry_t::register_plugin(std::unique_ptr<i_error_plugin_t> plugin) noexcept {
        if (!plugin) {
            return;
        }

        auto shared_plugin = std::shared_ptr<i_error_plugin_t>(std::move(plugin));
        update_snapshot_([&shared_plugin](plugin_list_t& snapshot,
                                          std::vector<shared_plugin_ptr_t>& owned) {
            auto name = shared_plugin->name();
            auto it = std::find_if(snapshot.begin(), snapshot.end(),
                [&name](const shared_plugin_ptr_t& plugin) { return plugin->name() == name; });
            if (it != snapshot.end()) {
                auto* old_ptr = it->get();
                owned.erase(std::remove_if(owned.begin(), owned.end(),
                                [old_ptr](const shared_plugin_ptr_t& plugin) { return plugin.get() == old_ptr; }),
                            owned.end());
                *it = shared_plugin;
            } else {
                snapshot.push_back(shared_plugin);
            }
            owned.push_back(std::move(shared_plugin));
        });
    }

    void plugin_registry_t::register_plugin_ref(i_error_plugin_t& plugin) noexcept {
        auto non_owning = std::shared_ptr<i_error_plugin_t>(&plugin, [](i_error_plugin_t*){});
        update_snapshot_([&non_owning](plugin_list_t& snapshot,
                                       std::vector<shared_plugin_ptr_t>&) {
            auto name = non_owning->name();
            auto it = std::find_if(snapshot.begin(), snapshot.end(),
                [&name](const shared_plugin_ptr_t& plugin) { return plugin->name() == name; });
            if (it != snapshot.end()) {
                *it = non_owning;
            } else {
                snapshot.push_back(non_owning);
            }
        });
    }

    void plugin_registry_t::unregister_plugin(std::string_view name) noexcept {
        update_snapshot_([name](plugin_list_t& snapshot, std::vector<shared_plugin_ptr_t>& owned) {
            auto it = std::find_if(snapshot.begin(), snapshot.end(),
                [name](const shared_plugin_ptr_t& plugin) { return plugin->name() == name; });
            if (it == snapshot.end()) {
                return;
            }
            auto* old_ptr = it->get();
            snapshot.erase(it);
            owned.erase(std::remove_if(owned.begin(), owned.end(),
                            [old_ptr](const shared_plugin_ptr_t& plugin) { return plugin.get() == old_ptr; }),
                        owned.end());
        });
    }

    void plugin_registry_t::notify_error(const core::error_context_t& context) noexcept {
        auto snapshot = std::atomic_load(&plugins_snapshot_);
        for (const auto& plugin : *snapshot) {
            if (context.get_code().get_level() >= plugin->min_level()) {
                try {
                    plugin->on_error(context);
                } catch (const std::exception& e) {
                    auto name = plugin->name();
                    std::fprintf(stderr,
                                 "[plugin_registry] plugin '%.*s' on_error threw exception: %s\n",
                                 static_cast<int>(name.size()), name.data(), e.what());
                }
            }
        }
    }

    void plugin_registry_t::enqueue_notification(const core::error_context_t& context) noexcept {
        notification_channel_.enqueue_notification(context);
    }

    void plugin_registry_t::enqueue_deferred_notification(const core::error_context_t& context) noexcept {
        if (tls_deferred_.flushing) {
            return;
        }
        if (tls_deferred_.max_size > 0 && tls_deferred_.buffer.size() >= tls_deferred_.max_size) {
            tls_deferred_.overflow_dropped = true;
            return;
        }
        try {
            tls_deferred_.buffer.push_back(context);
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr,
                         "[plugin_registry] enqueue_deferred_notification: std::bad_alloc\n");
            tls_deferred_.overflow_dropped = true;
        }
    }

    namespace {

        /**
         * @brief 向所有插件分发单个延迟错误上下文
         * @details 遍历插件快照，按 min_level 过滤后调用 on_error。
         *          插件回调抛出的异常被捕获并记录日志，不影响其他插件。
         */
        void dispatch_to_plugins(const core::error_context_t& context,
                                 const std::vector<std::shared_ptr<i_error_plugin_t>>& plugins) noexcept {
            for (const auto& plugin : plugins) {
                if (context.get_code().get_level() < plugin->min_level()) {
                    continue;
                }
                try {
                    plugin->on_error(context);
                } catch (const std::exception& e) {
                    auto name = plugin->name();
                    std::fprintf(stderr,
                                 "[plugin_registry] deferred plugin '%.*s' on_error threw: %s\n",
                                 static_cast<int>(name.size()), name.data(), e.what());
                }
            }
        }

    }  // namespace

    void plugin_registry_t::flush_deferred_notifications() noexcept {
        if (tls_deferred_.buffer.empty()) {
            tls_deferred_.overflow_dropped = false;
            return;
        }
        tls_deferred_.flushing = true;
        auto snapshot = std::atomic_load(&plugins_snapshot_);
        for (const auto& deferred_context : tls_deferred_.buffer) {
            dispatch_to_plugins(deferred_context, *snapshot);
        }
        tls_deferred_.buffer.clear();
        tls_deferred_.overflow_dropped = false;
        tls_deferred_.flushing = false;
    }

    /**
     * @brief 获取当前线程待刷新的延迟通知数量
     * @return size_t 缓冲中的通知数量
     */
    size_t plugin_registry_t::pending_deferred_notifications() const noexcept {
        return tls_deferred_.buffer.size();
    }

    size_t plugin_registry_t::clear_deferred_notifications() noexcept {
        const size_t dropped = tls_deferred_.buffer.size();
        tls_deferred_.buffer.clear();
        tls_deferred_.overflow_dropped = false;
        return dropped;
    }

    /**
     * @brief 设置当前线程延迟缓冲最大容量
     * @param max_size 最大容量，0 表示无限制
     */
    void plugin_registry_t::set_deferred_buffer_size(size_t max_size) noexcept {
        tls_deferred_.max_size = max_size;
    }

    size_t plugin_registry_t::get_deferred_buffer_size() const noexcept {
        return tls_deferred_.max_size;
    }

    bool plugin_registry_t::deferred_buffer_overflowed() const noexcept {
        return tls_deferred_.overflow_dropped;
    }

    size_t plugin_registry_t::size() const noexcept {
        auto snapshot = std::atomic_load(&plugins_snapshot_);
        return snapshot->size();
    }

    bool plugin_registry_t::empty() const noexcept {
        auto snapshot = std::atomic_load(&plugins_snapshot_);
        return snapshot->empty();
    }

    /**
     * @brief 清空所有已注册插件
     */
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

    /**
     * @brief 获取单例实例
     * @details 使用 std::call_once + 函数局部静态保证线程安全的单例初始化。
     *          初始化完成后自注册为默认错误通知器（仅在未设置自定义 notifier 时）。
     *          若已存在自定义通知器，输出提示信息，避免用户误以为插件注册表已被使用。
     * @return 单例引用
     */
    plugin_registry_t& plugin_registry_t::instance() noexcept {
        static plugin_registry_t* instance_ptr = nullptr;
        std::call_once(once_flag_, [] {
            static plugin_registry_t instance;
            instance_ptr = &instance;
            initialized_.store(true, std::memory_order_release);
            auto* existing_notifier = core::error_context_initializer_t::get_error_notifier();
            if (existing_notifier == nullptr) {
                core::error_context_initializer_t::set_error_notifier(instance_ptr);
            } else if (existing_notifier != instance_ptr) {
                std::fprintf(stderr,
                             "[plugin_registry] instance: custom error notifier already set; "
                             "plugin_registry_t will not receive error notifications.\n");
            }
        });
        return *instance_ptr;
    }

    bool plugin_registry_t::is_initialized() noexcept {
        return initialized_.load(std::memory_order_acquire);
    }

}  // namespace error_system::plugin
