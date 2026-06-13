#pragma once
#include "error_system/core/error_context.h"
#include <string>

/**
 * @file error_exception_t.h
 * @brief 错误异常数据类定义
 * @details 定义错误异常数据结构
 * @author yiice
 * @version 1.0.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    class error_exception_t : public std::exception {
        private:
        error_context_t context_;
        std::string cached_message_;

        public:
        /**
         * @brief 从错误上下文构造异常
         */
        explicit error_exception_t(error_context_t context) noexcept
            : context_(std::move(context)), cached_message_(context_.to_string()) {}

        /**
         * @brief 实现 std::exception 接口
         * @return const char* 返回完整的错误详情字符串
         */
        const char* what() const noexcept override { return cached_message_.c_str(); }

        /**
         * @brief 获取原始的错误上下文，方便进行逻辑判断
         * @return const error_context_t& 错误上下文
         */
        const error_context_t& context() const noexcept { return context_; }

        /**
         * @brief 获取原始错误码
         * @return error_code_t 错误码
         */
        error_code_t code() const noexcept { return context_.get_code(); }
    };

}  // namespace error_system::core