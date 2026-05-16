#pragma once
#include "error_system/core/error_context.h"
#include <ostream>

/**
 * @file error_formatter.h
 * @brief 错误格式化
 * @details 定义错误格式化相关的函数
 * @author yiice
 * @version 1.0.0
 * @date 2026-05-06
 * @copyright Copyright (c) 2026
 */
/**
 * @brief 错误上下文输出流运算符重载
 * @param os 输出流
 * @param context 错误上下文
 * @return std::ostream& 输出流
 */
inline std::ostream& operator<<(std::ostream& os, const error_system::core::error_context_t& context) {
    return os << context.to_string();
}
