#pragma once
#include <atomic>
#include <cstdio>
#include <memory>
#include <new>
#include <shared_mutex>
#include <string_view>
#include <vector>

#include "error_system/plugin/async_notification_channel.h"
#include "error_system/plugin/i_error_plugin.h"

/**
 * @file plugin_registry.h
 * @brief 插件注册表
 * @details 单例模式，管理所有已注册的错误系统插件，
 *          在错误事件发生时依次通知所有插件。
 *          支持同步（默认）和异步队列两种通知模式。
 *          使用 RCU（Read-Copy-Update）快照机制，notify_error 热路径零拷贝无锁读取。
 *          异步通知通道由 async_notification_channel_t 封装，自动管理后台线程生命周期。
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::plugin {

    /**
     * @brief 插件注册表
     * @details 单例，通过 register_plugin(unique_ptr) 接管插件所有权，
     *          register_plugin_ref() 提供非持有引用注册（用于单例等场景）。
     *          异步模式下自动启动后台工作线程处理通知队列，
     *          队列处理回调转发至本类的 notify_error。
     */
    class plugin_registry_t {
    public:
        using plugin_pointer_t = i_error_plugin_t*;
        using unique_plugin_ptr_t = std::unique_ptr<i_error_plugin_t>;
        using shared_plugin_ptr_t = std::shared_ptr<i_error_plugin_t>;
        using plugin_list_t = std::vector<shared_plugin_ptr_t>;

    private:
        /**
         * @brief RCU 插件快照
         * @details shared_ptr 保证旧快照在所有读者完成后安全释放。
         *          register/unregister 时拷贝-修改-原子交换，
         *          notify_error 直接 atomic_load 读取，无锁零拷贝。
         *          快照内存储 shared_ptr<i_error_plugin_t>，使读者持有插件共享所有权，
         *          避免 unregister 销毁插件后调用 on_error 触发 use-after-free。
         */
        std::shared_ptr<const plugin_list_t> plugins_snapshot_{
            std::make_shared<const plugin_list_t>()};
        mutable std::shared_mutex plugins_mutex_;

        /**
         * @brief 持有所有权的插件列表
         * @details 存储通过 register_plugin(unique_ptr) 注册的插件（转为 shared_ptr），
         *          注册表析构时自动释放。非持有引用（register_plugin_ref）不在此列表。
         *          与快照在同一写锁保护下修改，避免数据竞争。
         */
        std::vector<shared_plugin_ptr_t> owned_plugins_{};

        /**
         * @brief 异步通知通道（仅在 async_queue 模式下使用）
         * @details 封装 async_queue_t，自动管理后台线程生命周期。
         *          出队上下文通过注入的回调转发至 notify_error。
         */
        async_notification_channel_t notification_channel_;

        /**
         * @brief 单例初始化一次性标志（规范 22）
         */
        static std::once_flag once_flag_;

        plugin_registry_t() noexcept;

        ~plugin_registry_t() noexcept = default;

        plugin_registry_t(const plugin_registry_t&) = delete;

        plugin_registry_t& operator=(const plugin_registry_t&) = delete;

        plugin_registry_t(plugin_registry_t&&) = delete;

        plugin_registry_t& operator=(plugin_registry_t&&) = delete;

        /**
         * @brief 原子更新插件快照
         * @details 在写锁保护下，基于当前快照创建新副本、修改后原子替换。
         *          旧快照由 shared_ptr 自动管理生命周期。
         *          modifier 同时操作快照副本与 owned_plugins_，保证二者在
         *          同一写锁保护下保持一致。make_shared 与 modifier 中的
         *          push_back 可能抛出 std::bad_alloc，捕获后记录日志并返回，
         *          保持旧快照不变。
         * @param modifier 修改新副本与 owned_plugins_ 的回调函数
         */
        template <typename Modifier>
        void update_snapshot_(Modifier&& modifier) noexcept {
            try {
                std::unique_lock<std::shared_mutex> lock(plugins_mutex_);
                auto old_snapshot = std::atomic_load(&plugins_snapshot_);
                auto new_snapshot_ptr = std::make_shared<plugin_list_t>(*old_snapshot);
                modifier(*new_snapshot_ptr, owned_plugins_);
                std::atomic_store(&plugins_snapshot_,
                                  std::static_pointer_cast<const plugin_list_t>(new_snapshot_ptr));
            } catch (const std::bad_alloc&) {
                std::fprintf(stderr, "[plugin_registry] update_snapshot_: std::bad_alloc\n");
            }
        }

    public:
        /**
         * @brief 注册插件（转移所有权）
         * @details 注册表接管插件所有权，插件生命周期由注册表管理。
         *          若已存在同名插件，旧插件将被替换并自动释放。
         * @param plugin 插件 unique_ptr，不能为 nullptr
         */
        void register_plugin(unique_plugin_ptr_t plugin) noexcept;

        /**
         * @brief 注册插件（非持有引用）
         * @details 注册表不持有所有权，调用方负责插件生命周期。
         *          适用于单例、栈对象等场景。
         * @param plugin 插件引用
         */
        void register_plugin_ref(i_error_plugin_t& plugin) noexcept;

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
         *          同时作为异步通道出队上下文的处理回调。
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

        /**
         * @brief 设置异步通知队列最大容量
         * @details 当队列达到最大容量时，新通知将被丢弃（默认 0 = 无限制）
         * @param max_size 队列最大容量
         */
        void set_max_queue_size(size_t max_size) noexcept;

        /**
         * @brief 获取异步通知队列最大容量
         * @return size_t 队列最大容量，0 表示无限制
         */
        size_t get_max_queue_size() const noexcept;

        /**
         * @brief 获取单例实例
         * @return plugin_registry_t& 单例引用
         */
        static plugin_registry_t& instance() noexcept;
    };

}  // namespace error_system::plugin
