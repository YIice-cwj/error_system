#pragma once
#include <array>
#include <memory>

/**
 * @file object_pool_t.h
 * @brief 对象池
 * @details
 * @author yiice
 * @version 1.0.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::memory {

    /**
     * @brief 对象池
     * @tparam T 对象类型
     * @tparam Capacity 对象池容量
     * @details 预先分配固定大小的内存，每次使用时分配，用完归还到对象池
     */
    template <typename T, size_t Capacity>
    class object_pool_t {
        public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;

        private:
        std::unique_ptr<std::array<value_type, Capacity>> objects_{};
        std::unique_ptr<std::array<size_type, Capacity>> next_indices_{};
        size_type next_index_{0};
        size_type count_{0};

        private:
        /**
         * @brief 检查对象指针是否有效
         * @param object 对象指针
         * @return bool 对象指针是否有效
         */
        bool __is_valid(pointer object) const noexcept;

        public:
        /**
         * @brief 构造函数
         */
        object_pool_t() noexcept;

        /**
         * @brief 析构函数
         */
        ~object_pool_t();

        /**
         * @brief 从对象池获取对象
         * @return pointer 对象指针
         */
        pointer acquire() noexcept;

        /**
         * @brief 归还对象到对象池
         * @param object 对象指针
         * @return bool 归还成功返回 true，指针无效返回 false
         */
        bool release(pointer& object) noexcept;

        /**
         * @brief 获取对象池容量
         * @return size_type 对象池容量
         */
        size_type capacity() const noexcept;

        /**
         * @brief 获取对象池当前对象数量
         * @return size_type 对象池当前对象数量
         */
        size_type size() const noexcept;

        /**
         * @brief 检查对象池是否为空
         * @return bool 对象池是否为空
         */
        bool empty() const noexcept;

        /**
         * @brief 检查对象池是否已满
         * @return bool 对象池是否已满
         */
        bool full() const noexcept;

        public:
        /**
         * @brief 获取单例对象池
         * @return object_pool_t<value_type, Capacity>& 单例对象池引用
         */
        static object_pool_t<value_type, Capacity>& instance() noexcept {
            static object_pool_t<value_type, Capacity> instance;
            return instance;
        }

        /**
         * @brief 获取线程局部对象池
         * @return object_pool_t<value_type, Capacity>& 线程局部对象池引用
         */
        static object_pool_t<value_type, Capacity>& instance_thread_local() noexcept {
            static thread_local object_pool_t<value_type, Capacity> instance_threads;
            return instance_threads;
        }
    };

    /**
     * @brief 检查对象指针是否有效
     * @param object 对象指针
     * @return bool 对象指针是否有效
     */
    template <typename T, size_t Capacity>
    bool object_pool_t<T, Capacity>::__is_valid(pointer object) const noexcept {
        return object >= objects_->data() && object < objects_->data() + Capacity;
    }

    /**
     * @brief 构造函数
     */
    template <typename T, size_t Capacity>
    object_pool_t<T, Capacity>::object_pool_t() noexcept
        : objects_(std::make_unique<std::array<value_type, Capacity>>()),
          next_indices_(std::make_unique<std::array<size_type, Capacity>>()),
          next_index_(0),
          count_(0) {
        for (size_type i = 0; i < Capacity; ++i) {
            (*next_indices_)[i] = i;
        }
    }

    /**
     * @brief 析构函数
     */
    template <typename T, size_t Capacity>
    object_pool_t<T, Capacity>::~object_pool_t() {}

    /**
     * @brief 从对象池获取对象
     * @return pointer 对象指针
     */
    template <typename T, size_t Capacity>
    typename object_pool_t<T, Capacity>::pointer object_pool_t<T, Capacity>::acquire() noexcept {
        if (full()) {
            return nullptr;
        }
        size_type free_index = (*next_indices_)[next_index_++];
        ++count_;
        return &(*objects_)[free_index];
    }

    /**
     * @brief 归还对象到对象池
     * @param object 对象指针
     */
    template <typename T, size_t Capacity>
    bool object_pool_t<T, Capacity>::release(pointer& object) noexcept {
        if (__is_valid(object)) {
            size_type index = object - objects_->data();
            (*next_indices_)[--next_index_] = index;
            --count_;
            object = nullptr;
            return true;
        }
        return false;
    }

    /**
     * @brief 获取对象池容量
     * @return size_type 对象池容量
     */
    template <typename T, size_t Capacity>
    typename object_pool_t<T, Capacity>::size_type object_pool_t<T, Capacity>::capacity() const noexcept {
        return Capacity;
    }

    /**
     * @brief 获取对象池当前对象数量
     * @return size_type 对象池当前对象数量
     */
    template <typename T, size_t Capacity>
    typename object_pool_t<T, Capacity>::size_type object_pool_t<T, Capacity>::size() const noexcept {
        return count_;
    }

    /**
     * @brief 检查对象池是否为空
     * @return bool 对象池是否为空
     */
    template <typename T, size_t Capacity>
    bool object_pool_t<T, Capacity>::empty() const noexcept {
        return count_ == 0;
    }

    /**
     * @brief 检查对象池是否已满
     * @return bool 对象池是否已满
     */
    template <typename T, size_t Capacity>
    bool object_pool_t<T, Capacity>::full() const noexcept {
        return count_ >= Capacity;
    }

}  // namespace error_system::memory
