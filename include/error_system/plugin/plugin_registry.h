#pragma once
#include <cstdio>

#include <atomic>
#include <memory>
#include <new>
#include <shared_mutex>
#include <string_view>
#include <vector>

#include "error_system/core/i_error_notifier.h"
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
 *
 * @section init_timing 初始化时序与最佳实践
 * - 插件注册表必须在构造任何 error_context_t 之前完成初始化，否则错误通知会被静默丢弃。
 * - 调用 plugin_registry_t::instance() 会自动完成初始化，并自注册为默认通知器。
 * - 若需要自定义通知器，应在调用 instance() 之前通过
 *   error_context_initializer_t::set_error_notifier() 设置；否则 instance() 会覆盖为默认值。
 * - 在 async_queue / sync_deferred 模式下，后台线程/线程本地缓冲的生命周期由注册表管理，
 *   无需手动创建线程。
 * - 线程安全：register_plugin / unregister_plugin / notify_error 均线程安全；
 *   flush_deferred_notifications / pending_deferred_notifications / clear_deferred_notifications
 *   仅操作当前线程的缓冲，必须由同一线程调用。
 *
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
     *          实现 core::i_error_notifier_t 接口，通过 instance() 自注册到
     *          error_context_initializer_t，使 core 层经由抽象接口完成通知，
     *          解耦 core→plugin 反向依赖（依赖倒置原则）。
     */
    class plugin_registry_t : public core::i_error_notifier_t {
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

        /**
         * @brief 单例是否已完成初始化
         * @details 用于检测在 instance() 之前误用非静态成员函数的时序错误。
         *          由 instance() 在 std::call_once 中设置为 true。
         */
        static std::atomic<bool> initialized_;

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
        void update_snapshot_(Modifier&& modifier) noexcept;

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
         *          实现 core::i_error_notifier_t 接口。
         * @param context 错误上下文
         */
        void notify_error(const core::error_context_t& context) noexcept override;

        /**
         * @brief 异步入队错误通知
         * @details 将错误上下文副本推入后台队列，由工作线程异步处理。
         *          在 async_queue 模式下由 error_context_t 构造时调用。
         *          首次调用时自动启动后台工作线程。
         *          实现 core::i_error_notifier_t 接口。
         * @param context 错误上下文
         */
        void enqueue_notification(const core::error_context_t& context) noexcept override;

        /**
         * @brief 累积延迟通知到线程本地缓冲（sync_deferred 模式）
         * @details 通知不会立即触发，而是累积到当前线程的本地缓冲中，
         *          直至调用 flush_deferred_notifications() 时统一批量通知。
         *          适用于请求处理等批处理场景：在请求处理期间累积错误，
         *          请求结束时一次性 flush，避免每个错误都阻塞在插件回调上。
         *          缓冲满时（默认 1024）丢弃新通知并设置溢出标志。
         *          实现 core::i_error_notifier_t 接口。
         * @param context 错误上下文
         */
        void enqueue_deferred_notification(const core::error_context_t& context) noexcept override;

        /**
         * @brief 触发当前线程累积的所有延迟通知
         * @details 在 sync_deferred 模式下，将线程本地缓冲中的所有错误上下文
         *          依次通知给已注册插件（按 min_level 过滤），通知完成后清空缓冲。
         *          通知期间产生的新错误不会进入缓冲（避免无限递归）。
         * @note 必须由累积通知的同一线程调用，否则无法 flush 该线程的缓冲。
         */
        void flush_deferred_notifications() noexcept;

        /**
         * @brief 获取当前线程待 flush 的延迟通知数量
         * @return size_t 待通知数量
         */
        [[nodiscard]] size_t pending_deferred_notifications() const noexcept;

        /**
         * @brief 清空当前线程的延迟通知缓冲（不触发通知）
         * @details 用于异常恢复场景：请求处理中途异常退出时丢弃累积的通知。
         * @return size_t 被丢弃的通知数量
         */
        size_t clear_deferred_notifications() noexcept;

        /**
         * @brief 设置延迟通知缓冲最大容量
         * @details 缓冲满时新通知将被丢弃，overflow_dropped 标志置位。
         *          默认容量 1024。仅影响后续 enqueue，不截断已有缓冲。
         * @param max_size 最大容量，0 表示无限制
         */
        void set_deferred_buffer_size(size_t max_size) noexcept;

        /**
         * @brief 获取延迟通知缓冲最大容量
         * @return size_t 最大容量，0 表示无限制
         */
        [[nodiscard]] size_t get_deferred_buffer_size() const noexcept;

        /**
         * @brief 查询延迟通知缓冲是否发生过溢出丢弃
         * @details 溢出标志在 flush/clear 时重置。
         * @return bool 是否发生过丢弃
         */
        [[nodiscard]] bool deferred_buffer_overflowed() const noexcept;

        /**
         * @brief 获取已注册插件数量
         * @return size_t 插件数量
         */
        [[nodiscard]] size_t size() const noexcept;

        /**
         * @brief 判断是否有已注册的插件
         * @return bool 是否为空
         */
        [[nodiscard]] bool empty() const noexcept;

        /**
         * @brief 清空所有已注册插件
         */
        void clear() noexcept;

        /**
         * @brief 获取异步队列中待处理通知数量
         * @return size_t 队列大小
         */
        [[nodiscard]] size_t pending_notifications() const noexcept;

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
        [[nodiscard]] size_t get_max_queue_size() const noexcept;

        /**
         * @brief 获取单例实例
         * @details 首次调用时完成单例构造，并尝试自注册为默认错误通知器。
         *          若调用前已通过 error_context_initializer_t::set_error_notifier()
         *          设置自定义通知器，则保留该通知器并输出提示。
         * @return plugin_registry_t& 单例引用
         */
        static plugin_registry_t& instance() noexcept;

        /**
         * @brief 判断插件注册表单例是否已完成初始化
         * @details 用于 main 函数或测试在构造 error_context_t 前确认插件系统已就绪。
         *          返回 true 不保证已有插件注册，只说明 instance() 已被调用过。
         * @return bool true=已初始化，false=未初始化
         */
        [[nodiscard]] static bool is_initialized() noexcept;
    };

}  // namespace error_system::plugin

#include "error_system/plugin/details/plugin_registry.inl"
