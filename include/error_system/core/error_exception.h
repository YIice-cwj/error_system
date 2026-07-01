#pragma once
#include <string>

#include "error_system/core/error_context.h"
#include "error_system/core/error_context_serializer.h"

/**
 * @file error_exception.h
 * @brief 错误异常数据类定义
 * @details 定义错误异常数据结构
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误异常类
     * @details 继承自 std::exception，将 error_context_t 封装为可抛出异常。
     *          构造时通过 error_context_serializer_t 缓存错误详情字符串，
     *          what() 返回该缓存字符串。适用于需要跨调用栈传播错误上下文的场景。
     */
    class error_exception_t : public std::exception {
    private:
        error_context_t context_{};
        std::string cached_message_{};

    public:
        /**
         * @brief 从错误上下文构造异常
         */
        explicit error_exception_t(error_context_t context) noexcept
            : context_(std::move(context)), cached_message_(error_context_serializer_t::to_string(context_)) {}

        error_exception_t(const error_exception_t&) = default;

        ~error_exception_t() noexcept override = default;

        error_exception_t& operator=(const error_exception_t&) = delete;

        error_exception_t(error_exception_t&&) noexcept = default;
        
        error_exception_t& operator=(error_exception_t&&) = delete;

        /**
         * @brief 实现 std::exception 接口
         * @return const char* 返回完整的错误详情字符串
         */
        const char* what() const noexcept override { return cached_message_.c_str(); }

        /**
         * @brief 获取原始的错误上下文，方便进行逻辑判断
         * @return const error_context_t& 错误上下文
         */
        [[nodiscard]] const error_context_t& context() const noexcept { return context_; }

        /**
         * @brief 获取原始错误码
         * @return error_code_t 错误码
         */
        [[nodiscard]] error_code_t code() const noexcept { return context_.get_code(); }
    };

}  // namespace error_system::core