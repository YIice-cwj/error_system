#pragma once
#include <functional>
#include <shared_mutex>

#include "error_system/core/error_code.h"

/**
 * @file duplicate_policy.h
 * @brief 重复错误码处理策略
 * @details 定义重复错误码的处理策略枚举与策略处理器，封装 skip/overwrite/warn 三种行为，
 *          从 error_registry_t 中拆分以遵循单一职责原则
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误码元数据前置声明
     * @details 定义于 error_registry.h，此处仅需引用类型用于回调签名
     */
    struct error_metadata_t;

    /**
     * @brief 重复注册警告回调函数类型
     * @details 参数为 (错误码原始值, 已存在的元数据)
     */
    using duplicate_warn_callback_t = std::function<void(code_t, const error_metadata_t&)>;

    /**
     * @brief 重复处理策略
     * @details 定义当错误码已存在时的处理行为
     */
    enum class duplicate_policy_t : uint8_t {
        skip,       // 静默跳过（默认）
        overwrite,  // 覆盖已有定义
        warn,       // 跳过但记录警告
    };

    /**
     * @brief 重复错误码策略处理器
     * @details 封装重复错误码的处理策略与警告回调，自包含（自带互斥锁），
     *          可被 error_registry_t 组合使用。处理器仅负责决策（skip/overwrite/warn），
     *          不负责索引擦除——擦除由调用方（注册器）根据返回值执行。
     * @note apply_duplicate_policy_ 返回 true 表示继续注册（overwrite 场景，调用方需先擦除旧条目）；
     *       返回 false 表示中止注册（skip/warn 场景）。
     *       warn 场景下回调由处理器内部触发。
     */
    class duplicate_policy_handler_t {
    private:
        /**
         * @brief 重复处理策略
         */
        duplicate_policy_t policy_{duplicate_policy_t::skip};

        /**
         * @brief 重复注册警告回调函数
         * @details 当策略为 warn 时调用，参数为 (错误码原始值, 已存在的元数据)。
         *          默认为 nullptr（不触发回调）。
         */
        duplicate_warn_callback_t warn_callback_{nullptr};

        /**
         * @brief 策略与回调互斥锁，保护并发访问
         */
        mutable std::shared_mutex mutex_;

        /**
         * @brief 处理 skip 策略
         * @param raw_code 错误码原始值
         * @return bool 是否继续注册流程（skip 返回 false）
         */
        bool handle_duplicate_skip_(code_t raw_code) const noexcept;

        /**
         * @brief 处理 overwrite 策略
         * @param raw_code 错误码原始值
         * @return bool 是否继续注册流程（overwrite 返回 true，调用方负责擦除旧条目）
         */
        bool handle_duplicate_overwrite_(code_t raw_code) const noexcept;

        /**
         * @brief 处理 warn 策略（不持锁调用回调）
         * @details 在锁内拷贝 callback 与 existing，释放锁后再调用回调，
         *          避免在持锁状态下执行用户代码（规范 16「禁止持锁调用用户回调」）
         * @param raw_code 错误码原始值
         * @param existing 已存在的元数据
         * @return bool 是否继续注册流程（warn 返回 false）
         */
        bool handle_duplicate_warn_(code_t raw_code, const error_metadata_t& existing) const noexcept;

    public:
        duplicate_policy_handler_t() = default;

        ~duplicate_policy_handler_t() noexcept = default;

        duplicate_policy_handler_t(const duplicate_policy_handler_t&) = delete;

        duplicate_policy_handler_t& operator=(const duplicate_policy_handler_t&) = delete;

        duplicate_policy_handler_t(duplicate_policy_handler_t&&) = delete;

        duplicate_policy_handler_t& operator=(duplicate_policy_handler_t&&) = delete;

        /**
         * @brief 根据当前策略处理重复错误码
         * @param raw_code 错误码原始值
         * @param existing 已存在的元数据指针（无重复时为 nullptr）
         * @return bool 是否继续注册流程（true=继续，调用方需先擦除旧条目；false=中止）
         * @note 当 existing 为 nullptr 时，无论策略如何均返回 true（无重复可直接注册）。
         *       warn 策略下若 existing 非空，会触发警告回调（回调在释放锁后调用）。
         */
        bool apply_duplicate_policy(code_t raw_code, const error_metadata_t* existing) noexcept;

        /**
         * @brief 设置重复处理策略
         * @param policy 重复处理策略
         */
        void set_policy(duplicate_policy_t policy) noexcept {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            policy_ = policy;
        }

        /**
         * @brief 获取当前重复处理策略
         * @return duplicate_policy_t 当前重复处理策略
         */
        [[nodiscard]] duplicate_policy_t get_policy() const noexcept {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return policy_;
        }

        /**
         * @brief 设置重复注册警告回调
         * @param callback 回调函数，签名为 void(code_t, const error_metadata_t&)
         * @note 传入 nullptr 可清除回调
         */
        void set_warn_callback(duplicate_warn_callback_t callback) noexcept {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            warn_callback_ = std::move(callback);
        }

        /**
         * @brief 获取当前重复注册警告回调
         * @return const duplicate_warn_callback_t& 当前回调
         */
        [[nodiscard]] const duplicate_warn_callback_t& get_warn_callback() const noexcept {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return warn_callback_;
        }
    };

}  // namespace error_system::core
