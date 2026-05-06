#pragma once
#include <variant>
#include <functional>
#include "error_system/core/error_context.h"

/**
 * @file result_t.h
 * @brief 结果数据类定义
 * @details 定义结果数据结构、字段解析和访问接口
 * @author yiice
 * @version 1.0.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 结果数据类
     * @details 封装结果信息，提供字段解析和访问功能
     * @tparam T 结果类型
     */
    template<typename T>
    class result_t {
        public:
        using value_type = T;

        private:
        std::variant<value_type, error_context_t> value_or_error_;

        public:
        /**
         * @brief 构造函数
         * @param value 成功值
         */
        result_t(const value_type& value) : value_or_error_(value) {}

        /**
         * @brief 移动构造函数
         * @param value 成功值
         */
        result_t(value_type&& value) : value_or_error_(std::move(value)) {}

        /**
         * @brief 构造函数
         * @param error_context 错误上下文
         */
        result_t(const error_context_t& error_context) : value_or_error_(error_context) {}
        
        /**
         * @brief 构造函数
         * @param code 错误码
         * @param message 错误信息
         */
        result_t(error_code_t code, const std::string& message = "") 
            : value_or_error_(error_context_t{code, message}) {}

        public:        
        /**
         * @brief 检查结果是否为错误
         * @return bool 如果结果为错误则返回true
         */
        bool is_error() const noexcept { return std::holds_alternative<error_context_t>(value_or_error_); }
        
        /**
         * @brief 检查结果是否为成功
         * @return bool 如果结果为成功则返回true
         */
        bool is_success() const noexcept { return std::holds_alternative<value_type>(value_or_error_); }

        /**
         * @brief 获取错误上下文
         * @return const error_context_t& 错误上下文
         */
        const error_context_t& error() const noexcept { return std::get<error_context_t>(value_or_error_); }
        
        /**
         * @brief 获取成功值
         * @return const value_type& 成功值
         */
        const value_type& value() const noexcept { return std::get<value_type>(value_or_error_); }
        
        /**
         * @brief 获取成功值
         * @return value_type& 成功值
         */
        value_type& value() noexcept { return std::get<value_type>(value_or_error_); }

        /**
         * @brief 对结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function), std::move(value_))) 操作结果
         * @details 如果当前结果为成功，则调用 function 处理值并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误的新结果（移动语义）
        */
        template <typename Function>
        auto and_then(Function&& function) && -> decltype(std::invoke(std::forward<Function>(function), std::move(value()))) {
            using return_type = decltype(std::invoke(std::forward<Function>(function), std::move(value())));
            if (is_error()) {
                return return_type(std::move(error())); 
            }
            return std::invoke(std::forward<Function>(function), std::move(value()));
        }

        
        /**
         * @brief 对结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function), value())) 操作结果
         * @details 如果当前结果为成功，则调用 function 处理值并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误的新结果
        */
        template <typename Function>
        auto and_then(Function&& function) & -> decltype(std::invoke(std::forward<Function>(function), value())) {
            using return_type = decltype(std::invoke(std::forward<Function>(function), value()));
            if (is_error()) {
                return return_type(error());
            }
            return std::invoke(std::forward<Function>(function), value());
        }

        /**
         * @brief 对错误结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<value_type> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误并返回其结果；
         *          如果当前结果为成功，则返回当前对象（移动语义）
         */
        template <typename Function>
        result_t<value_type> or_else(Function&& function) && {
            if (is_error()) {
                return std::invoke(std::forward<Function>(function), std::move(error()));
            }
            return std::move(*this);
        }
        
        /**
         * @brief 对错误结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<value_type> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误并返回其结果；
         *          如果当前结果为成功，则返回当前对象的副本
         */
        template <typename Function>
        result_t<value_type> or_else(Function&& function) & {
            if (is_error()) {
                return std::invoke(std::forward<Function>(function), error());
            }
            return *this;
        }
    };

    /** 
     * @brief 模板特化：当 T 为 void 时，特化为不包含值的 result_t
     * @details 特化 result_t 类模板，当 T 为 void 时，不存储值，仅存储错误上下文
     */
    template<>
    class result_t<void> {
        public:
        using value_type = void;

        private:
        error_context_t error_context_;

        public:
        /**
         * @brief 构造函数
         */
        result_t() : error_context_{} {}

        /**
         * @brief 构造函数
         * @param error_context 错误上下文
         */
        result_t(const error_context_t& error_context) : error_context_(error_context) {}
        
        /**
         * @brief 构造函数
         * @param code 错误码
         * @param message 错误信息
         */
        result_t(error_code_t code, const std::string& message = "") 
            : error_context_(error_context_t{code, message}) {}
        
        /**
         * @brief 检查结果是否为错误
         * @return bool 如果结果为错误则返回true
         */
        bool is_error() const noexcept { return static_cast<bool>(error_context_); }
        
        /**
         * @brief 检查结果是否为成功
         * @return bool 如果结果为成功则返回true
         */
        bool is_success() const noexcept { return !is_error(); }

        /**
         * @brief 获取错误上下文
         * @return const error_context_t& 错误上下文
         */
        const error_context_t& error() const noexcept { return error_context_; }

        /**
         * @brief 对结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function))) 操作结果
         * @details 如果当前结果为成功，则调用 function 并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误上下文的新结果（移动语义）
         */
        template <typename Function>
        auto and_then(Function&& function) && -> decltype(std::invoke(std::forward<Function>(function))) {
            using return_type = decltype(std::invoke(std::forward<Function>(function)));
            if (is_error()) {
                return return_type(std::move(error_context_)); 
            }
            return std::invoke(std::forward<Function>(function));
        }

        /**
         * @brief 对结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function))) 操作结果
         * @details 如果当前结果为成功，则调用 function 并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误上下文的新结果
         */
        template <typename Function>
        auto and_then(Function&& function) & -> decltype(std::invoke(std::forward<Function>(function))) {
            using return_type = decltype(std::invoke(std::forward<Function>(function)));
            if (is_error()) {
                return return_type(error_context_);
            }
            return std::invoke(std::forward<Function>(function));
        }

        /**
         * @brief 对错误结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<void> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误上下文并返回其结果；
         *          如果当前结果为成功，则返回当前对象（移动语义）
         */
        template <typename Function>
        result_t<void> or_else(Function&& function) && {
            if (is_error()) {
                return std::invoke(std::forward<Function>(function), std::move(error_context_));
            }
            return std::move(*this);
        }
        
        /**
         * @brief 对错误结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<void> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误上下文并返回其结果；
         *          如果当前结果为成功，则返回当前对象的副本
         */
        template <typename Function>
        result_t<void> or_else(Function&& function) & {
            if (is_error()) {   
                return std::invoke(std::forward<Function>(function), error_context_);
            }
            return *this;
        }
    };

}