#pragma once
#include <cstddef>
#include <cstdio>

#include <functional>
#include <memory>
#include <utility>

#include "error_system/core/error_context.h"
#include "error_system/utils/async_queue.h"

/**
 * @file async_notification_channel.h
 * @brief 异步通知通道
 * @details 将错误上下文异步入队并由后台线程处理，封装 async_queue_t。
 *          通过构造时注入通知回调，将出队上下文转发给实际通知者（plugin_registry_t），
 *          打破与具体注册表实现的循环依赖，遵循单一职责原则（SRP）。
 *          自动管理后台线程生命周期，支持背压控制（队列满时丢弃新通知）。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::plugin {

    /**
     * @brief 异步通知通道
     * @details 单一职责：负责错误上下文的异步入队与转发，不感知具体插件注册表实现。
     *          通知回调在构造时注入，由后台工作线程在出队时调用。
     *          使用示例：
     * @code
     *   async_notification_channel_t channel(
     *       [](const core::error_context_t& context) { handle(context); });
     *   channel.enqueue_notification(context);
     * @endcode
     */
    class async_notification_channel_t {
    public:
        using context_ptr_t = std::shared_ptr<core::error_context_t>;
        using notify_callback_t = std::function<void(const core::error_context_t&)>;

    private:
        /**
         * @brief 出队上下文处理器
         * @details 持有通知回调，将出队的 shared_ptr<error_context_t> 解引用后转发给回调。
         *          通过回调注入避免与 plugin_registry_t 产生循环依赖。
         */
        struct context_processor_t {
            notify_callback_t callback;

            void operator()(context_ptr_t& context) const noexcept {
                if (callback && context) {
                    callback(*context);
                }
            }
        };

        utils::async_queue_t<context_ptr_t, context_processor_t> async_queue_;

    public:
        /**
         * @brief 构造函数
         * @details 注入通知回调，回调由后台工作线程对每个出队上下文调用一次。
         * @param callback 通知回调，不可为空（为空时出队上下文将被丢弃）
         */
        explicit async_notification_channel_t(notify_callback_t callback) noexcept
            : async_queue_(context_processor_t{std::move(callback)}) {}

        /**
         * @brief 析构函数，自动停止后台工作线程
         */
        ~async_notification_channel_t() noexcept = default;

        async_notification_channel_t(const async_notification_channel_t&) = delete;

        async_notification_channel_t& operator=(const async_notification_channel_t&) = delete;

        async_notification_channel_t(async_notification_channel_t&&) = delete;

        async_notification_channel_t& operator=(async_notification_channel_t&&) = delete;

        /**
         * @brief 异步入队错误通知
         * @details 将错误上下文副本推入后台队列，由工作线程异步处理。
         *          首次调用时自动启动后台工作线程。内存分配失败时记录日志并丢弃。
         * @param context 错误上下文
         */
        void enqueue_notification(const core::error_context_t& context) noexcept {
            try {
                auto copy = std::make_shared<core::error_context_t>(context);
                async_queue_.enqueue(std::move(copy));
            } catch (...) {
                std::fprintf(stderr,
                             "[async_notification_channel] enqueue_notification failed to allocate memory\n");
            }
        }

        /**
         * @brief 获取待处理通知数量
         * @return size_t 队列大小
         */
        [[nodiscard]] size_t pending_notifications() const noexcept {
            return async_queue_.size();
        }

        /**
         * @brief 设置队列最大容量
         * @details 当队列达到最大容量时，新通知将被丢弃（默认 0 = 无限制）
         * @param max_size 队列最大容量
         */
        void set_max_queue_size(size_t max_size) noexcept {
            async_queue_.set_max_size(max_size);
        }

        /**
         * @brief 获取队列最大容量
         * @return size_t 队列最大容量，0 表示无限制
         */
        [[nodiscard]] size_t get_max_queue_size() const noexcept {
            return async_queue_.max_size();
        }
    };

}  // namespace error_system::plugin
