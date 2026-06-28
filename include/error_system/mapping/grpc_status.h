#pragma once
#include <cstdint>
#include <string_view>

/**
 * @file grpc_status.h
 * @brief gRPC 状态码定义与转换
 * @details 与 grpc::StatusCode 数值一致，避免引入 gRPC 依赖。
 *          提供 to_string / from_int / is_valid 等转换函数。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::mapping {

    /**
     * @brief gRPC 状态码
     * @details 包装 gRPC 标准状态码，提供类型安全的转换接口。
     *          枚举值嵌在类内，grpc_status_t::ok 等用法保持兼容。
     *
     * @example
     * @code
     * grpc_status_t s = grpc_status_t::internal;
     * s.to_string();              // → "INTERNAL"
     * grpc_status_t::from_int(13); // → grpc_status_t::internal
     * grpc_status_t::is_valid(13); // → true
     * @endcode
     */
    class grpc_status_t {
    public:
        /**
         * @brief gRPC 状态码枚举值
         * @details 与 grpc::StatusCode 数值一致
         */
        enum value_t : uint16_t {
            ok = 0,                  // 成功
            cancelled = 1,           // 调用取消
            unknown = 2,             // 未知错误
            invalid_argument = 3,    // 客户端参数错误
            deadline_exceeded = 4,   // 超时
            not_found = 5,           // 资源未找到
            already_exists = 6,      // 资源已存在
            permission_denied = 7,   // 权限不足
            resource_exhausted = 8,  // 资源耗尽（限流/配额）
            failed_precondition = 9, // 前置条件不满足
            aborted = 10,            // 操作中止（并发冲突等）
            out_of_range = 11,       // 越界
            unimplemented = 12,      // 未实现
            internal = 13,           // 内部错误
            unavailable = 14,        // 服务不可用（可重试）
            data_loss = 15,          // 数据损坏
            unauthenticated = 16,    // 未认证
        };

        /**
         * @brief 默认构造，值为 ok
         */
        constexpr grpc_status_t() noexcept = default;

        /**
         * @brief 从枚举值构造
         * @param value gRPC 状态码枚举值
         */
        constexpr grpc_status_t(value_t value) noexcept : value_(value) {}

        /**
         * @brief 获取底层枚举值
         * @return value_t 枚举值
         */
        [[nodiscard]] constexpr value_t value() const noexcept { return value_; }

        /**
         * @brief 转换为整数
         * @return uint16_t 状态码数值
         */
        [[nodiscard]] constexpr uint16_t to_int() const noexcept {
            return static_cast<uint16_t>(value_);
        }

        /**
         * @brief 转换为字符串名称（用于日志/序列化）
         * @return const char* 状态码名称（如 "OK"、"INTERNAL"）
         * @note 实现位于 grpc_status.cc（非 constexpr，分离以遵循规范第 5 条）
         */
        [[nodiscard]] const char* to_string() const noexcept;

        /**
         * @brief 从整数构造 gRPC 状态码
         * @param value 整数值
         * @return grpc_status_t 状态码（无效值返回 unknown）
         */
        [[nodiscard]] static constexpr grpc_status_t from_int(int value) noexcept {
            if (value < 0 || value > 16) {
                return grpc_status_t{unknown};
            }
            return grpc_status_t{static_cast<value_t>(value)};
        }

        /**
         * @brief 检查整数值是否为合法 gRPC 状态码
         * @param value 整数值
         * @return bool 合法返回 true
         */
        [[nodiscard]] static constexpr bool is_valid(int value) noexcept {
            return value >= 0 && value <= 16;
        }

        /**
         * @brief 相等比较
         */
        constexpr bool operator==(grpc_status_t other) const noexcept { return value_ == other.value_; }

        /**
         * @brief 不等比较
         */
        constexpr bool operator!=(grpc_status_t other) const noexcept { return value_ != other.value_; }

    private:
        value_t value_{ok};
    };

}  // namespace error_system::mapping
