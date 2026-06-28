#include "error_system/mapping/grpc_status.h"

/**
 * @file grpc_status.cc
 * @brief gRPC 状态码 to_string 实现分离
 * @details 将 grpc_status_t::to_string() 的非 constexpr switch 实现从头文件分离，
 *          遵循规范第 5 条（头文件声明，源文件实现）。
 *          constexpr 方法（from_int / is_valid / to_int / value / 比较运算符）
 *          仍保留在头文件中以支持编译期求值。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::mapping {

    const char* grpc_status_t::to_string() const noexcept {
        switch (value_) {
            case ok:                  return "OK";
            case cancelled:           return "CANCELLED";
            case unknown:             return "UNKNOWN";
            case invalid_argument:    return "INVALID_ARGUMENT";
            case deadline_exceeded:   return "DEADLINE_EXCEEDED";
            case not_found:           return "NOT_FOUND";
            case already_exists:      return "ALREADY_EXISTS";
            case permission_denied:   return "PERMISSION_DENIED";
            case resource_exhausted:  return "RESOURCE_EXHAUSTED";
            case failed_precondition: return "FAILED_PRECONDITION";
            case aborted:             return "ABORTED";
            case out_of_range:        return "OUT_OF_RANGE";
            case unimplemented:       return "UNIMPLEMENTED";
            case internal:            return "INTERNAL";
            case unavailable:         return "UNAVAILABLE";
            case data_loss:           return "DATA_LOSS";
            case unauthenticated:     return "UNAUTHENTICATED";
            default:                  return "UNKNOWN";
        }
    }

}  // namespace error_system::mapping
