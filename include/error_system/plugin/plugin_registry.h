#pragma once
#include "error_system/plugin/i_error_plugin.h"
#include "error_system/utils/async_queue.h"
#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <vector>

/**
 * @file plugin_registry.h
 * @brief 插件注册表
 * @details 单例模式，管理所有已注册的错误系统插件，
 *          在错误事件发生时依次通知所有插件。
 *          支持同步（默认）和异步队列两种通知模式。
 *          使用 RCU（Read-Copy-Update）快照机制，notify_error 热路径零拷贝无锁读取。
 *          异步队列由 async_queue_t 模版类实现，自动管理后台线程生命周期。
 * @author yiice
 * @version 2.2.0
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
        /**
         * @brief RCU 插件快照
         * @details shared_ptr 保证旧快照在所有读者完成后安全释放。
         *          register/unregister 时拷贝-修改-原子交换，
         *          notify_error 直接 atomic_load 读取，无锁零拷贝。
         */
        std::shared_ptr<const std::vector<i_error_plugin_t*>> plugins_snapshot_{
            std::make_shared<const std::vector<i_error_plugin_t*>>()};
        mutable std::shared_mutex plugins_mutex_;

        /**
         * @brief 异步队列处理器
         * @details 将 async_queue_t 出队的 shared_ptr<error_context_t> 转发给 notify_error
         *          通过 instance() 获取单例，无需存储 this 指针
         */
        struct __context_processor_t {
            void operator()(std::shared_ptr<core::error_context_t>& context) const noexcept {
                plugin_registry_t::instance().notify_error(*context);
            }
        };

        /**
         * @brief 异步通知队列（仅在 async_queue 模式下使用）
         * @details 基于 async_queue_t 模版类，自动管理后台线程生命周期。
         *          元素类型为 shared_ptr<error_context_t>，避免拷贝。
         */
        utils::async_queue_t<std::shared_ptr<core::error_context_t>, __context_processor_t> async_queue_{{__context_processor_t{}}};

        private:
        plugin_registry_t() = default;

        ~plugin_registry_t() = default;

        plugin_registry_t(const plugin_registry_t&) = delete;

        plugin_registry_t& operator=(const plugin_registry_t&) = delete;

        plugin_registry_t(plugin_registry_t&&) = delete;

        plugin_registry_t& operator=(plugin_registry_t&&) = delete;

        /**
         * @brief 原子更新插件快照
         * @details 在写锁保护下，基于当前快照创建新副本、修改后原子替换。
         *          旧快照由 shared_ptr 自动管理生命周期。
         * @param modifier 修改新副本的回调函数
         */
        template <typename Modifier>
        void __update_snapshot(Modifier&& modifier) noexcept {
            std::unique_lock<std::shared_mutex> lock(plugins_mutex_);
            auto old_snapshot = std::atomic_load(&plugins_snapshot_);
            auto new_snapshot_ptr = std::make_shared<std::vector<i_error_plugin_t*>>(*old_snapshot);
            modifier(*new_snapshot_ptr);
            std::atomic_store(&plugins_snapshot_,
                              std::static_pointer_cast<const std::vector<i_error_plugin_t*>>(new_snapshot_ptr));
        }

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
         * @details 通过 RCU 快照无锁读取插件列表，依次调用每个插件的 on_error()，
         *          异常不会向外传播。在 sync 模式下由 error_context_t 构造时直接调用。
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