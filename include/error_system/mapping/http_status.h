#pragma once
#include <cstdint>
#include <string_view>

/**
 * @file http_status.h
 * @brief HTTP 状态码定义与转换
 * @details 包装常用 HTTP 状态码，提供类型安全的转换接口。
 *          仅包含错误系统映射所需的状态码子集，非完整 HTTP 标准覆盖。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::mapping {

    /**
     * @brief HTTP 状态码
     * @details 包装常用 HTTP 状态码，提供 to_string / from_int / is_valid 转换。
     *          枚举值嵌在类内，http_status_t::ok 等用法保持兼容。
     *
     * @example
     * @code
     * http_status_t s = http_status_t::service_unavailable;
     * s.to_int();       // → 503
     * s.to_string();    // → "Service Unavailable"
     * http_status_t::from_int(503); // → http_status_t::service_unavailable
     * @endcode
     */
    class http_status_t {
    public:
        /**
         * @brief HTTP 状态码枚举值
         * @details 数值与 HTTP 标准一致（RFC 7231）
         */
        enum value_t : uint16_t {
            ok = 200,

            bad_request = 400,
            unauthorized = 401,
            forbidden = 403,
            not_found = 404,
            method_not_allowed = 405,
            request_timeout = 408,
            conflict = 409,
            gone = 410,
            payload_too_large = 413,
            uri_too_long = 414,
            too_many_requests = 429,

            internal_server_error = 500,
            not_implemented = 501,
            bad_gateway = 502,
            service_unavailable = 503,
            gateway_timeout = 504,
        };

        /**
         * @brief 默认构造，值为 ok
         */
        constexpr http_status_t() noexcept = default;

        /**
         * @brief 从枚举值构造
         * @param value HTTP 状态码枚举值
         */
        constexpr http_status_t(value_t value) noexcept : value_(value) {}

        /**
         * @brief 从整数构造（用于接收外部 HTTP 响应码）
         * @param code HTTP 状态码整数值
         */
        constexpr explicit http_status_t(uint16_t code) noexcept
            : value_(is_valid(code) ? static_cast<value_t>(code) : internal_server_error) {}

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
         * @brief 转换为可读字符串（用于日志/序列化）
         * @return const char* 状态码描述（如 "OK"、"Service Unavailable"）
         * @note 实现位于 http_status.cc（非 constexpr，分离以遵循规范第 5 条）
         */
        [[nodiscard]] const char* to_string() const noexcept;

        /**
         * @brief 从整数构造 HTTP 状态码
         * @param value 整数值
         * @return http_status_t 状态码（不在已知列表中时返回 internal_server_error）
         */
        [[nodiscard]] static constexpr http_status_t from_int(int value) noexcept {
            switch (value) {
                case 200: return http_status_t{ok};
                case 400: return http_status_t{bad_request};
                case 401: return http_status_t{unauthorized};
                case 403: return http_status_t{forbidden};
                case 404: return http_status_t{not_found};
                case 405: return http_status_t{method_not_allowed};
                case 408: return http_status_t{request_timeout};
                case 409: return http_status_t{conflict};
                case 410: return http_status_t{gone};
                case 413: return http_status_t{payload_too_large};
                case 414: return http_status_t{uri_too_long};
                case 429: return http_status_t{too_many_requests};
                case 500: return http_status_t{internal_server_error};
                case 501: return http_status_t{not_implemented};
                case 502: return http_status_t{bad_gateway};
                case 503: return http_status_t{service_unavailable};
                case 504: return http_status_t{gateway_timeout};
                default:  return http_status_t{internal_server_error};
            }
        }

        /**
         * @brief 检查整数值是否为已知 HTTP 状态码
         * @param value 整数值
         * @return bool 已知返回 true
         */
        [[nodiscard]] static constexpr bool is_valid(int value) noexcept {
            switch (value) {
                case 200: case 400: case 401: case 403: case 404:
                case 405: case 408: case 409: case 410: case 413:
                case 414: case 429: case 500: case 501: case 502:
                case 503: case 504:
                    return true;
                default:
                    return false;
            }
        }

        /**
         * @brief 是否为成功类状态码（2xx）
         */
        [[nodiscard]] constexpr bool is_success() const noexcept {
            return value_ == ok;
        }

        /**
         * @brief 是否为客户端错误（4xx）
         */
        [[nodiscard]] constexpr bool is_client_error() const noexcept {
            return to_int() >= 400 && to_int() < 500;
        }

        /**
         * @brief 是否为服务器错误（5xx）
         */
        [[nodiscard]] constexpr bool is_server_error() const noexcept {
            return to_int() >= 500;
        }

        /**
         * @brief 相等比较
         */
        constexpr bool operator==(http_status_t other) const noexcept { return value_ == other.value_; }

        /**
         * @brief 不等比较
         */
        constexpr bool operator!=(http_status_t other) const noexcept { return value_ != other.value_; }

    private:
        value_t value_{ok};
    };

}  // namespace error_system::mapping
