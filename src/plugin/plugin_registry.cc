#include "error_system/plugin/plugin_registry.h"
#include "error_system/config/error_config.h"

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
     * @details 仅通知 min_level() <= context.get_code().get_level() 的插件
     */
    void plugin_registry_t::notify_error(const core::error_context_t& context) noexcept {
        std::vector<i_error_plugin_t*> snapshot;
        snapshot.reserve(16);
        {
            std::shared_lock<std::shared_mutex> lock(plugins_mutex_);
            snapshot = plugins_;
        }
        const auto ctx_level = context.get_code().get_level();
        for (auto* registered_plugin : snapshot) {
            if (ctx_level < registered_plugin->min_level()) {
                continue;
            }
            try {
                registered_plugin->on_error(context);
            } catch (...) {
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

    /**
     * @brief 异步入队错误通知
     * @details 将 error_context_t 复制到 shared_ptr 后推入队列，
     *          首次调用时自动启动后台工作线程。
     *          若配置了 max_queue_size 且队列已满，新通知将被丢弃。
     */
    void plugin_registry_t::enqueue_notification(const core::error_context_t& context) noexcept {
        if (!worker_running_.load(std::memory_order_acquire)) {
            start_async_worker();
        }
        auto copy = std::make_shared<core::error_context_t>(context);
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            const size_t max_size = config::error_config_t::get_max_queue_size();
            if (max_size > 0 && async_queue_.size() >= max_size) {
                return;  // 队列已满，丢弃通知
            }
            async_queue_.push(std::move(copy));
        }
        queue_cv_.notify_one();
    }

    /**
     * @brief 获取异步队列中待处理通知数量
     * @return size_t 队列大小
     */
    size_t plugin_registry_t::pending_notifications() const noexcept {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return async_queue_.size();
    }

    /**
     * @brief 启动异步工作线程
     * @details 使用原子变量保证仅启动一次
     */
    void plugin_registry_t::start_async_worker() noexcept {
        bool expected = false;
        if (!worker_running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;  // 已有线程在运行
        }
        async_worker_ = std::thread(&plugin_registry_t::async_worker_loop, this);
    }

    /**
     * @brief 停止异步工作线程
     * @details 设置停止标志，唤醒工作线程，等待处理完所有待处理通知后退出
     */
    void plugin_registry_t::stop_async_worker() noexcept {
        worker_running_.store(false, std::memory_order_release);
        queue_cv_.notify_one();
        if (async_worker_.joinable()) {
            async_worker_.join();
        }
    }

    /**
     * @brief 异步工作线程主循环
     * @details 阻塞等待队列有新通知，取出后同步调用 notify_error。
     *          当 worker_running_ 为 false 且队列为空时退出。
     */
    void plugin_registry_t::async_worker_loop() noexcept {
        while (true) {
            std::shared_ptr<core::error_context_t> ctx;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this]() {
                    return !async_queue_.empty() || !worker_running_.load(std::memory_order_acquire);
                });

                if (!worker_running_.load(std::memory_order_acquire) && async_queue_.empty()) {
                    return;
                }

                ctx = std::move(async_queue_.front());
                async_queue_.pop();
            }

            if (ctx) {
                notify_error(*ctx);
            }
        }
    }

}  // namespace error_system::plugin
