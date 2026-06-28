#include "error_system/mapping/http_status.h"

/**
 * @file http_status.cc
 * @brief HTTP 状态码 to_string 实现分离
 * @details 将 http_status_t::to_string() 的非 constexpr switch 实现从头文件分离，
 *          遵循规范第 5 条（头文件声明，源文件实现）。
 *          constexpr 方法（from_int / is_valid / to_int / value / 比较运算符）
 *          仍保留在头文件中以支持编译期求值。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::mapping {

    const char* http_status_t::to_string() const noexcept {
        switch (value_) {
            case ok:                     return "OK";
            case bad_request:            return "Bad Request";
            case unauthorized:           return "Unauthorized";
            case forbidden:              return "Forbidden";
            case not_found:              return "Not Found";
            case method_not_allowed:     return "Method Not Allowed";
            case request_timeout:        return "Request Timeout";
            case conflict:               return "Conflict";
            case gone:                   return "Gone";
            case payload_too_large:      return "Payload Too Large";
            case uri_too_long:           return "URI Too Long";
            case too_many_requests:      return "Too Many Requests";
            case internal_server_error:  return "Internal Server Error";
            case not_implemented:        return "Not Implemented";
            case bad_gateway:            return "Bad Gateway";
            case service_unavailable:    return "Service Unavailable";
            case gateway_timeout:        return "Gateway Timeout";
            default:                     return "Unknown";
        }
    }

}  // namespace error_system::mapping
