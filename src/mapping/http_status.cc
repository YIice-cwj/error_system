#include "error_system/mapping/http_status.h"

/**
 * @file http_status.cc
 * @brief HTTP 状态码 c_str 实现分离
 * @details 将 http_status_t::c_str() 的非 constexpr switch 实现从头文件分离，
 *          遵循规范第 5 条（头文件声明，源文件实现）。
 *          constexpr 方法（from_int / is_valid / to_int / value / 比较运算符）
 *          仍保留在头文件中以支持编译期求值。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::mapping {

    const char* http_status_t::c_str() const noexcept {
        switch (value_) {
            case value_t::ok:                     return "OK";
            case value_t::bad_request:            return "Bad Request";
            case value_t::unauthorized:           return "Unauthorized";
            case value_t::forbidden:              return "Forbidden";
            case value_t::not_found:              return "Not Found";
            case value_t::method_not_allowed:     return "Method Not Allowed";
            case value_t::request_timeout:        return "Request Timeout";
            case value_t::conflict:               return "Conflict";
            case value_t::gone:                   return "Gone";
            case value_t::payload_too_large:      return "Payload Too Large";
            case value_t::uri_too_long:           return "URI Too Long";
            case value_t::too_many_requests:      return "Too Many Requests";
            case value_t::internal_server_error:  return "Internal Server Error";
            case value_t::not_implemented:        return "Not Implemented";
            case value_t::bad_gateway:            return "Bad Gateway";
            case value_t::service_unavailable:    return "Service Unavailable";
            case value_t::gateway_timeout:        return "Gateway Timeout";
            default:                              return "Unknown";
        }
    }

}  // namespace error_system::mapping
