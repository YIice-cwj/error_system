#include "error_system/mapping/grpc_status.h"

/**
 * @file grpc_status.cc
 * @brief gRPC 状态码 c_str 实现分离
 * @details 将 grpc_status_t::c_str() 的非 constexpr switch 实现从头文件分离，
 *          遵循规范第 5 条（头文件声明，源文件实现）。
 *          constexpr 方法（from_int / is_valid / to_int / value / 比较运算符）
 *          仍保留在头文件中以支持编译期求值。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::mapping {

    const char* grpc_status_t::c_str() const noexcept {
        switch (value_) {
            case value_t::ok:                  return "OK";
            case value_t::cancelled:           return "CANCELLED";
            case value_t::unknown:             return "UNKNOWN";
            case value_t::invalid_argument:    return "INVALID_ARGUMENT";
            case value_t::deadline_exceeded:   return "DEADLINE_EXCEEDED";
            case value_t::not_found:           return "NOT_FOUND";
            case value_t::already_exists:      return "ALREADY_EXISTS";
            case value_t::permission_denied:   return "PERMISSION_DENIED";
            case value_t::resource_exhausted:  return "RESOURCE_EXHAUSTED";
            case value_t::failed_precondition: return "FAILED_PRECONDITION";
            case value_t::aborted:             return "ABORTED";
            case value_t::out_of_range:        return "OUT_OF_RANGE";
            case value_t::unimplemented:       return "UNIMPLEMENTED";
            case value_t::internal:            return "INTERNAL";
            case value_t::unavailable:         return "UNAVAILABLE";
            case value_t::data_loss:           return "DATA_LOSS";
            case value_t::unauthenticated:     return "UNAUTHENTICATED";
            default:                           return "UNKNOWN";
        }
    }

}  // namespace error_system::mapping
