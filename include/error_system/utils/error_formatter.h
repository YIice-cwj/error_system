#pragma once
#include <cstdio>
#include <ostream>

#include "error_system/core/error_context.h"
#include "error_system/core/error_context_serializer.h"

/**
 * @file error_formatter.h
 * @brief 错误格式化
 * @details 定义错误格式化相关的函数，包括 error_context_t 的输出流运算符重载
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-06
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

/**
 * @brief 错误上下文输出流运算符重载
 * @param os 输出流
 * @param context 错误上下文
 * @return std::ostream& 输出流
 */
inline std::ostream& operator<<(std::ostream& os, const error_context_t& context) noexcept {
    try {
        return os << error_context_serializer_t::to_string(context);
    } catch (...) {
        std::fprintf(stderr, "[error_formatter] operator<< threw exception\n");
        return os;
    }
}

}  // namespace error_system::core