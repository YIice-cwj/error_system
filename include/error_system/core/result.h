#pragma once
#include "error_system/core/error_context.h"
#include <cassert>
#include <functional>
#include <string_view>
#include <type_traits>
#include <variant>

/**
 * @file result_t.h
 * @brief 结果数据类定义
 * @details 定义结果数据结构、字段解析和访问接口
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 结果数据类
     * @details 封装结果信息，提供字段解析和访问功能
     * @tparam T 结果类型
     */
    template <typename T>
    class result_t {
        public:
        using value_type = T;

        private:
        std::variant<value_type, error_context_t> value_or_error_;

        public:
        /**
         * @brief 错误构造工厂函数（推荐）
         * @details 替代直接使用构造函数构造错误结果，语义更清晰。
         *          避免构造函数重载混淆（value vs error_context vs error_code）。
         * @param code 错误码
         * @param message 错误信息，默认为空
         * @return result_t 包装了错误的结果对象
         *
         * @example
         * // 替代 result_t<int>(code, "失败")
         * return result_t<int>::make_error(ERR_DB_FAIL, "数据库操作失败");
         */
        static result_t make_error(error_code_t code, const std::string& message = "") noexcept {
            return result_t(error_context_t{code, message});
        }

        /**
         * @brief 错误构造工厂函数（移动消息版本）
         * @param code 错误码
         * @param message 错误信息
         * @return result_t 包装了错误的结果对象
         */
        static result_t make_error(error_code_t code, std::string&& message) noexcept {
            return result_t(error_context_t{code, std::move(message)});
        }

        /**
         * @brief 错误构造工厂函数（从已有 error_context_t）
         * @param context 错误上下文
         * @return result_t 包装了错误的结果对象
         */
        static result_t make_error(const error_context_t& context) noexcept {
            return result_t(context);
        }

        /**
         * @brief 创建成功结果
         * @details 工厂方法，创建包含成功值的结果
         * @param value 成功值
         * @return result_t 成功结果
         */
        static result_t make_success(value_type value) noexcept {
            return result_t(std::move(value));
        }

        /**
         * @brief 构造函数
         * @param value 成功值
         */
        explicit result_t(const value_type& value) noexcept : value_or_error_(value) {}

        /**
         * @brief 移动构造函数
         * @param value 成功值
         */
        explicit result_t(value_type&& value) noexcept : value_or_error_(std::move(value)) {}

        /**
         * @brief 构造函数
         * @param error_context 错误上下文
         */
        explicit result_t(const error_context_t& error_context) noexcept : value_or_error_(error_context) {}

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
         * @details 使用 std::get_if 安全获取，若当前为成功状态则返回静态哨兵值（空 error_context_t）。
         *          调用方应在调用前通过 is_error() 检查，否则返回的哨兵值 is_error() 为 false。
         * @return const error_context_t& 错误上下文
         */
        const error_context_t& error() const noexcept {
            assert(is_error() && "result_t::error() called on a success result");
            auto* ptr = std::get_if<error_context_t>(&value_or_error_);
            if (ptr) {
                return *ptr;
            }
            static thread_local const error_context_t sentinel{};
            return sentinel;
        }

        /**
         * @brief 获取错误上下文（可变引用）
         * @details 使用 std::get_if 安全获取，若当前为成功状态则返回线程局部哨兵值。
         *          调用方应在调用前通过 is_error() 检查，否则返回的哨兵值无意义。
         * @return error_context_t& 错误上下文可变引用
         */
        error_context_t& error() noexcept {
            assert(is_error() && "result_t::error() called on a success result");
            auto* ptr = std::get_if<error_context_t>(&value_or_error_);
            if (ptr) {
                return *ptr;
            }
            static thread_local error_context_t sentinel{};
            return sentinel;
        }

        /**
         * @brief 获取成功值
         * @details 使用 std::get_if 安全获取，若当前为错误状态则返回静态哨兵值（T{}）。
         *          调用方应在调用前通过 is_success() 检查，否则返回的哨兵值可能无意义。
         *          要求 T 必须可默认构造，若 T 不可默认构造请使用 value_pointer() 替代。
         * @return const value_type& 成功值
         */
        const value_type& value() const noexcept {
            static_assert(std::is_default_constructible_v<value_type>,
                          "result_t::value() requires T to be default-constructible. "
                          "Use value_pointer() for non-default-constructible types.");
            assert(is_success() && "result_t::value() called on an error result");
            auto* ptr = std::get_if<value_type>(&value_or_error_);
            if (ptr) {
                return *ptr;
            }
            static thread_local const value_type sentinel{};
            return sentinel;
        }

        /**
         * @brief 获取成功值（可变引用）
         * @details 使用 std::get_if 安全获取，若当前为错误状态则返回线程局部哨兵值（T{}）。
         *          调用方应在调用前通过 is_success() 检查，否则返回的哨兵值可能无意义。
         * @return value_type& 成功值
         */
        value_type& value() noexcept {
            static_assert(std::is_default_constructible_v<value_type>,
                          "result_t::value() requires T to be default-constructible. "
                          "Use value_pointer() for non-default-constructible types.");
            assert(is_success() && "result_t::value() called on an error result");
            auto* ptr = std::get_if<value_type>(&value_or_error_);
            if (ptr) {
                return *ptr;
            }
            static thread_local value_type sentinel{};
            return sentinel;
        }

        /**
         * @brief 安全获取成功值指针
         * @return const value_type* 成功值指针，如果为错误则返回 nullptr
         */
        const value_type* value_pointer() const noexcept { return std::get_if<value_type>(&value_or_error_); }

        /**
         * @brief 安全获取成功值指针
         * @return value_type* 成功值指针，如果为错误则返回 nullptr
         */
        value_type* value_pointer() noexcept { return std::get_if<value_type>(&value_or_error_); }

        /**
         * @brief 安全获取成功值，失败时返回默认值
         * @param default_value 默认值引用，调用方保证其生命周期
         * @return const value_type& 成功值或默认值引用
         */
        const value_type& value_or(const value_type& default_value) const noexcept {
            auto* ptr = std::get_if<value_type>(&value_or_error_);
            return ptr ? *ptr : default_value;
        }

        /**
         * @brief 获取成功值，失败时返回默认值
         * @details 若结果为成功，返回包含的值；否则返回 T{} 默认值
         * @return T 成功值或默认值
         */
        value_type unwrap() const noexcept {
            const value_type* ptr = std::get_if<value_type>(&value_or_error_);
            return ptr ? *ptr : value_type{};
        }

        /**
         * @brief 获取成功值，失败时返回指定默认值
         * @details 若结果为成功，返回包含的值；否则返回用户提供的默认值
         * @param default_value 失败时的默认值
         * @return T 成功值或默认值
         */
        value_type unwrap_or(value_type default_value) const noexcept {
            const value_type* ptr = std::get_if<value_type>(&value_or_error_);
            return ptr ? *ptr : std::move(default_value);
        }

        /**
         * @brief 布尔转换运算符
         * @return bool 如果结果为成功则返回 true
         */
        explicit operator bool() const noexcept { return is_success(); }

        /**
         * @brief 断言获取成功值
         * @details 若当前为成功状态则返回值的只读引用；若为错误状态，在 Debug 模式下触发断言失败，
         *          在 Release 模式下返回静态哨兵值（T{}）。适用于调用方已确认不会出错的场景。
         *          要求 T 必须可默认构造。
         * @return const value_type& 成功值引用
         *
         * @example
         * auto result = compute();
         * int value = result.expect();
         */
        const value_type& expect() const noexcept {
            static_assert(std::is_default_constructible_v<value_type>,
                          "result_t::expect() requires T to be default-constructible. "
                          "Use value_pointer() for non-default-constructible types.");
            auto* ptr = std::get_if<value_type>(&value_or_error_);
            assert(ptr);
            if (ptr) {
                return *ptr;
            }
            static thread_local const value_type sentinel{};
            return sentinel;
        }

        /**
         * @brief 对成功值进行映射转换
         * @tparam Function 映射函数
         * @param function 映射函数，接受 value_type 返回新类型
         * @return result_t 转换后的结果，错误时传递错误上下文
         */
        template <typename Function>
        auto map(Function&& function) const& noexcept -> result_t<decltype(std::invoke(std::forward<Function>(function),
                                                                               std::declval<const value_type&>()))> {
            using new_type = decltype(std::invoke(std::forward<Function>(function), std::declval<const value_type&>()));
            if (is_error()) {
                return result_t<new_type>(error());
            }
            try {
                return result_t<new_type>(std::invoke(std::forward<Function>(function), value()));
            } catch (...) {
                std::fprintf(stderr, "[result_t] map: std::invoke threw exception\n");
                return result_t<new_type>(error_context_t{
                    error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFE),
                    "map: function threw exception"});
            }
        }

        /**
         * @brief 对成功值进行映射转换（移动语义）
         */
        template <typename Function>
        auto map(Function&& function) && noexcept -> result_t<decltype(std::invoke(std::forward<Function>(function),
                                                                           std::move(value())))> {
            using new_type = decltype(std::invoke(std::forward<Function>(function), std::move(value())));
            if (is_error()) {
                return result_t<new_type>(std::move(error()));
            }
            try {
                return result_t<new_type>(std::invoke(std::forward<Function>(function), std::move(value())));
            } catch (...) {
                std::fprintf(stderr, "[result_t] map(&&): std::invoke threw exception\n");
                return result_t<new_type>(error_context_t{
                    error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFE),
                    "map(&&): function threw exception"});
            }
        }

        /**
         * @brief 对错误上下文进行映射转换
         * @tparam Function 映射函数
         * @param function 映射函数，接受 error_context_t 返回新的 error_context_t
         * @return result_t 映射后的结果，成功时保持不变
         */
        template <typename Function>
        result_t<value_type> map_error(Function&& function) const& noexcept {
            if (is_error()) {
                try {
                    return result_t<value_type>(std::invoke(std::forward<Function>(function), error()));
                } catch (...) {
                    std::fprintf(stderr, "[result_t] map_error: std::invoke threw exception\n");
                    return result_t<value_type>(error());
                }
            }
            return result_t<value_type>(value());
        }

        /**
         * @brief 对错误上下文进行映射转换（移动语义）
         */
        template <typename Function>
        result_t<value_type> map_error(Function&& function) && noexcept {
            if (is_error()) {
                try {
                    return result_t<value_type>(std::invoke(std::forward<Function>(function), std::move(error())));
                } catch (...) {
                    std::fprintf(stderr, "[result_t] map_error(&&): std::invoke threw exception\n");
                    return std::move(*this);
                }
            }
            return std::move(*this);
        }

        /**
         * @brief 对结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function), std::move(value_))) 操作结果
         * @details 如果当前结果为成功，则调用 function 处理值并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误的新结果（移动语义）
         */
        template <typename Function>
        auto and_then(Function&& function) && noexcept -> decltype(std::invoke(std::forward<Function>(function),
                                                                      std::move(value()))) {
            using return_type = decltype(std::invoke(std::forward<Function>(function), std::move(value())));
            if (is_error()) {
                return return_type(std::move(error()));
            }
            try {
                return std::invoke(std::forward<Function>(function), std::move(value()));
            } catch (...) {
                std::fprintf(stderr, "[result_t] and_then(&&): std::invoke threw exception\n");
                return return_type(error_context_t{
                    error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFE),
                    "and_then(&&): function threw exception"});
            }
        }

        /**
         * @brief 对结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function), value())) 操作结果
         * @details 如果当前结果为成功，则调用 function 处理值并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误的新结果
         */
        template <typename Function>
        auto and_then(Function&& function) & noexcept -> decltype(std::invoke(std::forward<Function>(function), value())) {
            using return_type = decltype(std::invoke(std::forward<Function>(function), value()));
            if (is_error()) {
                return return_type(error());
            }
            try {
                return std::invoke(std::forward<Function>(function), value());
            } catch (...) {
                std::fprintf(stderr, "[result_t] and_then(&): std::invoke threw exception\n");
                return return_type(error_context_t{
                    error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFE),
                    "and_then(&): function threw exception"});
            }
        }

        /**
         * @brief 对结果进行链式操作（const 左值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function), value())) 操作结果
         * @details 如果当前结果为成功，则调用 function 处理值并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误的新结果
         */
        template <typename Function>
        auto and_then(Function&& function) const& noexcept -> decltype(std::invoke(std::forward<Function>(function), value())) {
            using return_type = decltype(std::invoke(std::forward<Function>(function), value()));
            if (is_error()) {
                return return_type(error());
            }
            try {
                return std::invoke(std::forward<Function>(function), value());
            } catch (...) {
                std::fprintf(stderr, "[result_t] and_then(const&): std::invoke threw exception\n");
                return return_type(error_context_t{
                    error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFE),
                    "and_then(const&): function threw exception"});
            }
        }

        /**
         * @brief 对错误结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<value_type> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误并返回其结果；
         *          如果当前结果为成功，则返回当前对象（移动语义）
         */
        template <typename Function>
        result_t<value_type> or_else(Function&& function) && noexcept {
            if (is_error()) {
                try {
                    return std::invoke(std::forward<Function>(function), std::move(error()));
                } catch (...) {
                    std::fprintf(stderr, "[result_t] or_else(&&): std::invoke threw exception\n");
                    return std::move(*this);
                }
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
        result_t<value_type> or_else(Function&& function) & noexcept {
            if (is_error()) {
                try {
                    return std::invoke(std::forward<Function>(function), error());
                } catch (...) {
                    std::fprintf(stderr, "[result_t] or_else(&): std::invoke threw exception\n");
                    return *this;
                }
            }
            return *this;
        }

        /**
         * @brief 模式匹配处理成功和错误两种路径
         * @details 若结果为成功，调用 success_fn；否则调用 error_fn。两个函数必须返回相同类型
         * @param success_fn 成功时的处理函数
         * @param error_fn 错误时的处理函数
         * @return 处理函数的返回值
         */
        template <typename SuccessFn, typename ErrorFn>
        auto match(SuccessFn&& success_fn, ErrorFn&& error_fn) const noexcept
            -> decltype(success_fn(std::declval<const value_type&>()))
        {
            if (is_success()) {
                auto* ptr = std::get_if<value_type>(&value_or_error_);
                if (ptr) {
                    return success_fn(*ptr);
                }
            } else {
                auto* ptr = std::get_if<error_context_t>(&value_or_error_);
                if (ptr) {
                    return error_fn(*ptr);
                }
            }
            return {};
        }
    };

    /**
     * @brief 模板特化：当 T 为 void 时，特化为不包含值的 result_t
     * @details 特化 result_t 类模板，当 T 为 void 时，不存储值，仅存储错误上下文
     */
    template <>
    class result_t<void> {
        public:
        using value_type = void;

        private:
        error_context_t error_context_;

        public:
        /**
         * @brief 构造函数
         */
        result_t() noexcept : error_context_{} {}

        /**
         * @brief 错误构造工厂函数
         * @param code 错误码
         * @param message 错误信息，默认为空
         * @return result_t<void> 包装了错误的结果对象
         */
        static result_t<void> make_error(error_code_t code, const std::string& message = "") noexcept {
            return result_t<void>(error_context_t{code, message});
        }

        /**
         * @brief 错误构造工厂函数（移动消息版本）
         * @param code 错误码
         * @param message 错误信息
         * @return result_t<void> 包装了错误的结果对象
         */
        static result_t<void> make_error(error_code_t code, std::string&& message) noexcept {
            return result_t<void>(error_context_t{code, std::move(message)});
        }

        /**
         * @brief 错误构造工厂函数（从已有 error_context_t）
         * @param context 错误上下文
         * @return result_t<void> 包装了错误的结果对象
         */
        static result_t<void> make_error(const error_context_t& context) noexcept {
            return result_t<void>(context);
        }

        /**
         * @brief 创建成功结果
         * @details 工厂方法，创建成功结果（void 类型）
         * @return result_t<void> 成功结果
         */
        static result_t make_success() noexcept {
            return result_t();
        }

        /**
         * @brief 构造函数
         * @param error_context 错误上下文
         */
        explicit result_t(const error_context_t& error_context) noexcept : error_context_(error_context) {}

        /**
         * @brief 布尔转换运算符
         * @return bool 如果结果为成功则返回 true
         */
        explicit operator bool() const noexcept { return is_success(); }

        /**
         * @brief 检查结果是否为错误
         * @return bool 如果结果为错误则返回true
         */
        bool is_error() const noexcept { return error_context_.is_error(); }

        /**
         * @brief 检查结果是否为成功
         * @return bool 如果结果为成功则返回true
         */
        bool is_success() const noexcept { return !is_error(); }

        /**
         * @brief 断言当前结果为成功
         * @details 若当前为错误状态，在 Debug 模式下触发断言失败。
         *          在 Release 模式下为无操作。适用于 void 返回类型中确认不会出错的场景。
         *
         * @example
         * auto result = do_something();
         * result.expect();
         */
        void expect() const noexcept {
            assert(is_success());
        }

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
        auto and_then(Function&& function) && noexcept -> decltype(std::invoke(std::forward<Function>(function))) {
            using return_type = decltype(std::invoke(std::forward<Function>(function)));
            if (is_error()) {
                return return_type(std::move(error_context_));
            }
            try {
                return std::invoke(std::forward<Function>(function));
            } catch (...) {
                std::fprintf(stderr, "[result_t<void>] and_then(&&): std::invoke threw exception\n");
                return return_type(error_context_t{
                    error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFE),
                    "and_then(&&): function threw exception"});
            }
        }

        /**
         * @brief 对结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function))) 操作结果
         * @details 如果当前结果为成功，则调用 function 并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误上下文的新结果
         */
        template <typename Function>
        auto and_then(Function&& function) & noexcept -> decltype(std::invoke(std::forward<Function>(function))) {
            using return_type = decltype(std::invoke(std::forward<Function>(function)));
            if (is_error()) {
                return return_type(error_context_);
            }
            try {
                return std::invoke(std::forward<Function>(function));
            } catch (...) {
                std::fprintf(stderr, "[result_t<void>] and_then(&): std::invoke threw exception\n");
                return return_type(error_context_t{
                    error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFE),
                    "and_then(&): function threw exception"});
            }
        }

        /**
         * @brief 对错误结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<void> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误上下文并返回其结果；
         *          如果当前结果为成功，则返回当前对象（移动语义）
         */
        template <typename Function>
        result_t<void> or_else(Function&& function) && noexcept {
            if (is_error()) {
                try {
                    return std::invoke(std::forward<Function>(function), std::move(error_context_));
                } catch (...) {
                    std::fprintf(stderr, "[result_t<void>] or_else(&&): std::invoke threw exception\n");
                    return std::move(*this);
                }
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
        result_t<void> or_else(Function&& function) & noexcept {
            if (is_error()) {
                try {
                    return std::invoke(std::forward<Function>(function), error_context_);
                } catch (...) {
                    std::fprintf(stderr, "[result_t<void>] or_else(&): std::invoke threw exception\n");
                    return *this;
                }
            }
            return *this;
        }
    };

}  // namespace error_system::core