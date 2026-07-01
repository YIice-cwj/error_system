#pragma once

/**
 * @file i_error_notifier.h
 * @brief 错误通知器抽象接口
 * @details 解耦 core 层对 plugin 层的反向依赖。core 层通过此接口通知错误事件，
 *          plugin 层提供具体实现（如 plugin_registry_t）。遵循依赖倒置原则，
 *          core 层不再直接依赖 plugin_registry_t，而是依赖此抽象接口。
 *          接口覆盖三种通知路径：同步通知、异步入队、延迟累积，与
 *          plugin_registry_t 的 notify_error / enqueue_notification /
 *          enqueue_deferred_notification 一一对应，确保 core 层全部通知调用
 *          均经由抽象接口完成。本头文件仅前向声明 error_context_t（不包含
 *          error_context.h），避免与 error_context.h 形成循环依赖。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-29
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    struct error_context_t;

    /**
     * @brief 错误通知器接口
     * @details 定义错误事件通知的抽象接口，由 plugin 层实现。core 层通过
     *          error_context_initializer_t::set_error_notifier() 注入具体实现，
     *          避免直接依赖 plugin 层（依赖倒置原则）。
     *          实现类必须保证所有方法 noexcept 安全，内部异常不应向外传播。
     * @see error_context_initializer_t::set_error_notifier
     * @see plugin_registry_t
     */
    class i_error_notifier_t {
    public:
        /**
         * @brief 虚析构函数
         * @details 确保通过基类指针正确释放派生类对象（规范 6：接口必须包含虚析构函数）
         */
        virtual ~i_error_notifier_t() noexcept = default;

        /**
         * @brief 同步通知错误事件
         * @details 立即向所有已注册插件分发错误上下文，调用方阻塞直至所有插件处理完成。
         *          实现需保证 noexcept，插件回调抛出的异常应在实现内部捕获并记录，
         *          不向外传播。在 sync 通知模式下由 error_context_t 构造时调用。
         * @param context 错误上下文
         */
        virtual void notify_error(const error_context_t& context) noexcept = 0;

        /**
         * @brief 异步入队错误通知
         * @details 将错误上下文副本推入后台队列，由工作线程异步处理。
         *          调用方不阻塞，通知延迟取决于队列处理速度。
         *          在 async_queue 通知模式下由 error_context_t 构造时调用。
         * @param context 错误上下文
         */
        virtual void enqueue_notification(const error_context_t& context) noexcept = 0;

        /**
         * @brief 累积延迟通知到线程本地缓冲
         * @details 通知不会立即触发，而是累积到当前线程的本地缓冲中，
         *          直至显式 flush 时统一批量通知。适用于请求处理等批处理场景。
         *          在 sync_deferred 通知模式下由 error_context_t 构造时调用。
         * @param context 错误上下文
         */
        virtual void enqueue_deferred_notification(const error_context_t& context) noexcept = 0;
    };

}  // namespace error_system::core
