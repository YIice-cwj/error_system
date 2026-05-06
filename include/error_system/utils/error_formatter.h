#pragma once
#include "error_system/core/error_context.h"
#include <ostream>

namespace error_system::utils {

    /**
     * @brief 错误上下文输出流运算符重载
     * @param os 输出流
     * @param context 错误上下文
     * @return std::ostream& 输出流
     */
    inline std::ostream& operator<<(std::ostream& os, const error_system::core::error_context_t& context) {
        return os << context.to_string();
    }

}  // namespace error_system::utils
