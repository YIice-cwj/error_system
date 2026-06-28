#pragma once
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace error_system::core {
    struct error_context_t;
}

/**
 * @file formatter_config.h
 * @brief 自定义格式化器配置类
 * @details 从 error_config_t 拆分而来，单一职责：管理错误上下文的自定义格式化函数。
 *          提供线程安全的 set/get 接口，通过 shared_mutex 实现读多写少场景下的并发访问。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::config {

    /**
     * @brief 自定义格式化回调函数类型
     * @details 接受 error_context_t 引用，返回格式化后的字符串
     */
    using formatter_callback_t = std::function<std::string(const core::error_context_t&)>;

    /**
     * @brief 自定义格式化器配置类
     * @details 管理错误上下文的自定义格式化函数，提供线程安全的 set/get 接口。
     *          类仅包含静态成员，禁止实例化。
     */
    class formatter_config_t {
    private:
        /**
         * @brief 格式化函数专用共享锁
         * @details 保护自定义格式化函数的互斥锁
         * @return std::shared_mutex& 共享锁引用
         */
        static std::shared_mutex& get_formatter_mutex_() noexcept {
            static std::shared_mutex mutex;
            return mutex;
        }

        /**
         * @brief 自定义格式化函数存储
         * @details 使用 shared_mutex 保护并发读写
         * @return formatter_callback_t& 自定义格式化函数引用
         */
        static formatter_callback_t& get_custom_formatter_() noexcept {
            static formatter_callback_t formatter{nullptr};
            return formatter;
        }

    public:
        formatter_config_t() = delete;
        ~formatter_config_t() = delete;
        formatter_config_t(const formatter_config_t&) = delete;
        formatter_config_t& operator=(const formatter_config_t&) = delete;
        formatter_config_t(formatter_config_t&&) = delete;
        formatter_config_t& operator=(formatter_config_t&&) = delete;

        /**
         * @brief 设置自定义格式化函数
         * @param formatter 自定义格式化函数
         */
        static void set_custom_formatter(formatter_callback_t formatter) noexcept {
            std::unique_lock<std::shared_mutex> lock(get_formatter_mutex_());
            get_custom_formatter_() = std::move(formatter);
        }

        /**
         * @brief 获取自定义格式化函数副本
         * @details 返回格式化函数的线程安全拷贝，调用方无需持有锁即可安全调用
         * @return formatter_callback_t 格式化函数副本
         */
        [[nodiscard]] static formatter_callback_t get_custom_formatter() noexcept {
            std::shared_lock<std::shared_mutex> lock(get_formatter_mutex_());
            return get_custom_formatter_();
        }
    };

}  // namespace error_system::config
