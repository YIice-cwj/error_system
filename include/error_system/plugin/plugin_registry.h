#pragma once
#include "error_system/plugin/i_error_plugin.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <vector>

/**
 * @file plugin_registry.h
 * @brief 插件注册表
 * @details 单例模式，管理所有已注册的错误系统插件，
 *          在错误事件发生时依次通知所有插件。
 *          支持同步（默认）和异步队列两种通知模式。
 * @author yiice
 * @version 2.0.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::plugin {

    /**
     * @brief 插件注册表
     * @details 单例，不持有插件所有权，调用方负责插件对象的生命周期。
     *          异步模式下自动启动后台工作线程处理通知队列。
     */
    class plugin_registry_t {
        private:
        std::vector<i_error_plugin_t*> plugins_;
        mutable std::shared_mutex plugins_mutex_;

        /**
         * @brief 异步通知队列（仅在 async_queue 模式下使用）
         * @details 存储 error_context_t 副本，由后台工作线程消费
         */
        std::queue<std::shared_ptr<core::error_context_t>> async_queue_;
        mutable std::mutex queue_mutex_;
        std::condition_variable queue_cv_;
        std::thread async_worker_;
        std::atomic<bool> worker_running_{false};

        private:
        plugin_registry_t() = default;

        ~plugin_registry_t() {
            stop_async_worker();
        }

        plugin_registry_t(const plugin_registry_t&) = delete;

        plugin_registry_t& operator=(const plugin_registry_t&) = delete;

        plugin_registry_t(plugin_registry_t&&) = delete;

        plugin_registry_t& operator=(plugin_registry_t&&) = delete;

        /**
         * @brief 启动异步工作线程
         * @details 首次调用 enqueue_notification 时自动启动，
         *          线程循环从队列取出通知并调用 notify_error
         */
        void start_async_worker() noexcept;

        /**
         * @brief 停止异步工作线程
         * @details 析构时自动调用，等待队列中所有通知处理完毕
         */
        void stop_async_worker() noexcept;

        /**
         * @brief 异步工作线程主循环
         * @details 阻塞等待队列有数据，取出后调用 notify_error 处理
         */
        void async_worker_loop() noexcept;

        public:
        /**
         * @brief 注册插件
         * @details 若已存在同名插件，将替换旧插件
         *          不持有所有权，调用方需保证插件在整个使用期间有效
         * @param plugin 插件指针，不能为 nullptr
         */
        void register_plugin(i_error_plugin_t* plugin) noexcept;

        /**
         * @brief 注销插件
         * @details 按名称查找并移除，未找到则无操作
         * @param name 插件名称
         */
        void unregister_plugin(std::string_view name) noexcept;

        /**
         * @brief 同步通知所有插件发生了错误事件
         * @details 依次调用每个插件的 on_error()，异常不会向外传播。
         *          在 sync 模式下由 error_context_t 构造时直接调用。
         * @param context 错误上下文
         */
        void notify_error(const core::error_context_t& context) noexcept;

        /**
         * @brief 异步入队错误通知
         * @details 将错误上下文副本推入后台队列，由工作线程异步处理。
         *          在 async_queue 模式下由 error_context_t 构造时调用。
         *          首次调用时自动启动后台工作线程。
         * @param context 错误上下文
         */
        void enqueue_notification(const core::error_context_t& context) noexcept;

        /**
         * @brief 获取已注册插件数量
         * @return size_t 插件数量
         */
        size_t size() const noexcept;

        /**
         * @brief 判断是否有已注册的插件
         * @return bool 是否为空
         */
        bool empty() const noexcept;

        /**
         * @brief 清空所有已注册插件
         */
        void clear() noexcept;

        /**
         * @brief 获取异步队列中待处理通知数量
         * @return size_t 队列大小
         */
        size_t pending_notifications() const noexcept;

        public:
        /**
         * @brief 获取单例实例
         * @return plugin_registry_t& 单例引用
         */
        static plugin_registry_t& instance() noexcept;
    };

}  // namespace error_system::plugin