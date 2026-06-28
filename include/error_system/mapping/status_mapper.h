#pragma once
#include <cstdint>

#include "error_system/core/error_code.h"
#include "error_system/core/error_level.h"
#include "error_system/domain/system_domain.h"
#include "error_system/mapping/grpc_status.h"
#include "error_system/mapping/http_status.h"

/**
 * @file status_mapper.h
 * @brief 错误码到 HTTP / gRPC 状态码的映射
 * @details 提供将 error_code_t 转换为标准 HTTP 状态码与 gRPC 状态码的能力，
 *          便于在 Web/RPC 边界统一翻译内部错误。所有方法均为 constexpr，
 *          可在编译期求值，零运行时开销。
 *
 *          状态码类型定义：
 *          - HTTP 状态码见 http_status.h（http_status_t）
 *          - gRPC 状态码见 grpc_status.h（grpc_status_t）
 * @author yiice
 * @version 1.1.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::mapping {

    using error_system::core::code_t;
    using error_system::core::error_code_t;
    using error_system::core::error_level_t;

    /**
     * @brief 错误码到 HTTP / gRPC 状态码的映射器
     * @details 纯函数工具类，不可实例化。映射策略：
     *          - 成功码 → HTTP 200 / gRPC OK
     *          - info/warn → HTTP 200 / gRPC OK（业务可继续）
     *          - error 级别按系统域细分：
     *            application → 400 / INVALID_ARGUMENT（业务参数错误）
     *            database     → 503 / DATA_LOSS（数据层不可用）
     *            middleware   → 503 / UNAVAILABLE（中间件不可用）
     *            system       → 500 / INTERNAL（系统内部错误）
     *            third_party  → 502 / UNAVAILABLE（上游故障）
     *            none         → 500 / INTERNAL
     *          - fatal 级别 → 500 / INTERNAL（除非 transient → 503 / UNAVAILABLE）
     *          - retryable/transient 优先映射为 503 / UNAVAILABLE，提示客户端重试
     */
    class status_mapper_t {
    private:
        /**
         * @brief HTTP 状态码映射内部实现
         * @details 根据错误等级和系统域将错误码映射为 HTTP 状态码：
         *          - retryable/transient → 503
         *          - info/warn → 200
         *          - error 级别按系统域细分（application→400, third_party→502,
         *            database/middleware→503, system/none→500）
         *          - fatal → 500
         * @param level 错误等级
         * @param domain 系统域
         * @param retryable 是否可重试
         * @param transient 是否为瞬时错误
         * @return http_status_t HTTP 状态码
         */
        static constexpr http_status_t to_http_status_impl_(error_level_t level,
                                                             domain::system_domain_t domain,
                                                             bool retryable, bool transient) noexcept {
            if (retryable || transient) {
                return http_status_t{http_status_t::service_unavailable};
            }
            switch (level) {
                case error_level_t::debug:
                case error_level_t::info:
                case error_level_t::warn:
                    return http_status_t{http_status_t::ok};
                case error_level_t::error:
                    switch (domain) {
                        case domain::system_domain_t::application:
                            return http_status_t{http_status_t::bad_request};
                        case domain::system_domain_t::third_party:
                            return http_status_t{http_status_t::bad_gateway};
                        case domain::system_domain_t::database:
                        case domain::system_domain_t::middleware:
                            return http_status_t{http_status_t::service_unavailable};
                        case domain::system_domain_t::system:
                        case domain::system_domain_t::none:
                        default:
                            return http_status_t{http_status_t::internal_server_error};
                    }
                case error_level_t::fatal:
                default:
                    return http_status_t{http_status_t::internal_server_error};
            }
        }

        /**
         * @brief gRPC 状态码映射内部实现
         * @details 根据错误等级和系统域将错误码映射为 gRPC 状态码：
         *          - retryable/transient → UNAVAILABLE
         *          - info/warn → OK
         *          - error 级别按系统域细分（application→INVALID_ARGUMENT,
         *            third_party/middleware→UNAVAILABLE, database→DATA_LOSS,
         *            system/none→INTERNAL）
         *          - fatal → INTERNAL
         * @param level 错误等级
         * @param domain 系统域
         * @param retryable 是否可重试
         * @param transient 是否为瞬时错误
         * @return grpc_status_t gRPC 状态码
         */
        static constexpr grpc_status_t to_grpc_status_impl_(error_level_t level,
                                                             domain::system_domain_t domain,
                                                             bool retryable, bool transient) noexcept {
            if (retryable || transient) {
                return grpc_status_t{grpc_status_t::unavailable};
            }
            switch (level) {
                case error_level_t::debug:
                case error_level_t::info:
                case error_level_t::warn:
                    return grpc_status_t{grpc_status_t::ok};
                case error_level_t::error:
                    switch (domain) {
                        case domain::system_domain_t::application:
                            return grpc_status_t{grpc_status_t::invalid_argument};
                        case domain::system_domain_t::third_party:
                            return grpc_status_t{grpc_status_t::unavailable};
                        case domain::system_domain_t::database:
                            return grpc_status_t{grpc_status_t::data_loss};
                        case domain::system_domain_t::middleware:
                            return grpc_status_t{grpc_status_t::unavailable};
                        case domain::system_domain_t::system:
                        case domain::system_domain_t::none:
                        default:
                            return grpc_status_t{grpc_status_t::internal};
                    }
                case error_level_t::fatal:
                default:
                    return grpc_status_t{grpc_status_t::internal};
            }
        }

    public:
        status_mapper_t() = delete;
        ~status_mapper_t() = delete;
        status_mapper_t(const status_mapper_t&) = delete;
        status_mapper_t& operator=(const status_mapper_t&) = delete;
        status_mapper_t(status_mapper_t&&) = delete;
        status_mapper_t& operator=(status_mapper_t&&) = delete;

        /**
         * @brief 将错误码映射为 HTTP 状态码
         * @param code 错误码
         * @return http_status_t HTTP 状态码
         */
        [[nodiscard]] static constexpr http_status_t to_http_status(error_code_t code) noexcept {
            if (code.is_success_code()) {
                return http_status_t{http_status_t::ok};
            }
            return to_http_status_impl_(code.get_level(), code.get_system(),
                                        code.is_retryable(), code.is_transient());
        }

        /**
         * @brief 将错误码映射为 gRPC 状态码
         * @param code 错误码
         * @return grpc_status_t gRPC 状态码
         */
        [[nodiscard]] static constexpr grpc_status_t to_grpc_status(error_code_t code) noexcept {
            if (code.is_success_code()) {
                return grpc_status_t{grpc_status_t::ok};
            }
            return to_grpc_status_impl_(code.get_level(), code.get_system(),
                                        code.is_retryable(), code.is_transient());
        }
    };

}  // namespace error_system::mapping
