#pragma once
#include <cstddef>
#include <cstdio>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

/**
 * @file async_queue.h
 * @brief 异步工作队列模版类
 * @details 单生产者-单消费者异步队列，自动管理后台工作线程生命周期。
 *          首次 enqueue 时自动启动工作线程，析构时自动停止。
 *          支持背压控制，队列满时拒绝新任务。
 *          处理器异常不会导致工作线程退出。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-14
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils {

    /**
     * @brief 异步工作队列
     * @details 将任务推入队列后由后台线程异步处理，调用方无需等待。
     *          T 必须可移动构造。Processor 必须可调用 void(T&) 且 noexcept。
     * @tparam T 队列元素类型（必须可移动）
     * @tparam Processor 处理器类型（void(T&) noexcept 可调用对象）
     */
    template <typename T, typename Processor>
    class async_queue_t {
    public:
        using value_type_t = T;
        using pointer_t = value_type_t*;
        using const_pointer_t = const value_type_t*;
        using reference_t = value_type_t&;
        using const_reference_t = const value_type_t&;
        using processor_t = Processor;

    private:
        processor_t processor_;
        std::queue<T> queue_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::thread worker_;
        std::atomic<bool> running_{false};
        size_t max_size_{0};

    private:
        /**
         * @brief 停止后台工作线程
         * @details 先在锁内设置退出标志，再释放锁后唤醒线程并 join，
         *          避免 worker 在持锁状态下被唤醒后又阻塞获取锁造成死锁。
         *          必须在设置 running_ = false 之后再 notify，否则 worker 可能
         *          因 running_ 仍为 true 而回到 wait 状态，造成 join 死锁。
         */
        void stop_() noexcept;

        /**
         * @brief 后台工作线程主循环
         * @details 阻塞等待队列有数据或退出信号，取出任务后调用处理器。
         *          处理器异常被捕获并忽略，不影响工作线程继续运行。
         */
        void worker_loop_() noexcept;

    public:
        /**
         * @brief 构造函数
         * @param processor 任务处理器，对每个出队元素调用 processor(item)
         */
        explicit async_queue_t(processor_t processor) noexcept;

        /**
         * @brief 析构函数，自动停止工作线程
         */
        ~async_queue_t() noexcept;

        async_queue_t(const async_queue_t&) = delete;

        async_queue_t& operator=(const async_queue_t&) = delete;

        async_queue_t(async_queue_t&&) = delete;

        async_queue_t& operator=(async_queue_t&&) = delete;

        /**
         * @brief 将任务推入队列
         * @details 首次调用时在锁内启动工作线程，避免「CAS 成功但线程创建失败」窗口期
         *          内其他线程看到 running_=true 却无 worker 处理任务（启动竞态）。
         *          若队列已满则拒绝。
         * @param item 要处理的任务（移动语义）
         * @return bool 是否成功入队
         */
        bool enqueue(value_type_t item) noexcept;

        /**
         * @brief 设置最大队列容量（背压控制）
         * @param size 最大容量，0 表示无限制
         */
        void set_max_size(size_t size) noexcept;

        /**
         * @brief 获取最大队列容量
         * @return size_t 最大容量，0 表示无限制
         */
        [[nodiscard]] size_t max_size() const noexcept;

        /**
         * @brief 获取当前队列中待处理任务数
         * @return size_t 队列大小
         */
        [[nodiscard]] size_t size() const noexcept;

        /**
         * @brief 判断队列是否为空
         * @return bool 是否为空
         */
        [[nodiscard]] bool empty() const noexcept;
    };

}  // namespace error_system::utils

#include "details/async_queue.inl"
