#pragma once

namespace error_system::utils {

template <typename T, typename Processor>
void async_queue_t<T, Processor>::stop_() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_.load()) {
            return;
        }
        running_.store(false);
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

template <typename T, typename Processor>
void async_queue_t<T, Processor>::worker_loop_() noexcept {
    while (true) {
        value_type_t item;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return !queue_.empty() || !running_.load();
            });
            if (queue_.empty() && !running_.load()) {
                return;
            }
            item = std::move(queue_.front());
            queue_.pop();
        }
        try {
            processor_(item);
        } catch (const std::exception&) {
            std::fprintf(stderr, "[async_queue] processor exception caught and ignored\n");
        }
    }
}

template <typename T, typename Processor>
async_queue_t<T, Processor>::async_queue_t(processor_t processor) noexcept
    : processor_(std::move(processor)) {}

template <typename T, typename Processor>
async_queue_t<T, Processor>::~async_queue_t() noexcept {
    stop_();
}

template <typename T, typename Processor>
bool async_queue_t<T, Processor>::enqueue(value_type_t item) noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_.load()) {
            try {
                worker_ = std::thread(&async_queue_t::worker_loop_, this);
                running_.store(true);
            } catch (const std::system_error&) {
                std::fprintf(stderr, "[async_queue] enqueue: failed to create worker thread\n");
                return false;  // 线程启动失败，拒绝入队
            }
        }
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            return false;
        }
        try {
            queue_.push(std::move(item));
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[async_queue] enqueue: std::bad_alloc\n");
            return false;
        }
    }
    cv_.notify_one();
    return true;
}

template <typename T, typename Processor>
void async_queue_t<T, Processor>::set_max_size(size_t size) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    max_size_ = size;
}

template <typename T, typename Processor>
size_t async_queue_t<T, Processor>::max_size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return max_size_;
}

template <typename T, typename Processor>
size_t async_queue_t<T, Processor>::size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

template <typename T, typename Processor>
bool async_queue_t<T, Processor>::empty() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

}  // namespace error_system::utils
