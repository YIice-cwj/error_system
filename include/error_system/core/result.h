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
         * @param ctx 错误上下文
         * @return result_t 包装了错误的结果对象
         */
        static result_t make_error(const error_context_t& ctx) noexcept {
            return result_t(ctx);
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
            auto* ptr = std::get_if<error_context_t>(&value_or_error_);
            if (ptr) {
                return *ptr;
            }
            static const error_context_t sentinel{};
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
            auto* ptr = std::get_if<value_type>(&value_or_error_);
            if (ptr) {
                return *ptr;
            }
            static const value_type sentinel{};
            return sentinel;
        }

        /**
         * @brief 获取成功值（可变引用）
         * @details 使用 std::get_if 安全获取，若当前为错误状态则返回静态哨兵值（T{}）。
         *          调用方应在调用前通过 is_success() 检查，否则返回的哨兵值可能无意义。
         * @return value_type& 成功值
         */
        value_type& value() noexcept {
            static_assert(std::is_default_constructible_v<value_type>,
                          "result_t::value() requires T to be default-constructible. "
                          "Use value_pointer() for non-default-constructible types.");
            auto* ptr = std::get_if<value_type>(&value_or_error_);
            if (ptr) {
                return *ptr;
            }
            static value_type sentinel{};
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
         * @brief 布尔转换运算符
         * @return bool 如果结果为成功则返回 true
         */
        explicit operator bool() const noexcept { return is_success(); }

        /**
         * @brief 断言获取成功值（带自定义消息）
         * @details 若当前为成功状态则返回值的只读引用；若为错误状态，在 Debug 模式下触发断言失败，
         *          在 Release 模式下返回静态哨兵值（T{}）。适用于调用方已确认不会出错的场景。
         *          要求 T 必须可默认构造。
         * @param msg 断言失败时的提示消息
         * @return const value_type& 成功值引用
         *
         * @example
         * auto result = compute();
         * int value = result.expect("compute() should never fail here");
         */
        const value_type& expect(const char* msg) const noexcept {
            static_assert(std::is_default_constructible_v<value_type>,
                          "result_t::expect() requires T to be default-constructible. "
                          "Use value_pointer() for non-default-constructible types.");
            auto* ptr = std::get_if<value_type>(&value_or_error_);
            assert(ptr && msg);
            if (ptr) {
                return *ptr;
            }
            static const value_type sentinel{};
            return sentinel;
        }

        /**
         * @brief 对成功值进行映射转换
         * @tparam Function 映射函数
         * @param function 映射函数，接受 value_type 返回新类型
         * @return result_t 转换后的结果，错误时传递错误上下文
         */
        template <typename Function>
        auto map(Function&& function) const& -> result_t<decltype(std::invoke(std::forward<Function>(function),
                                                                               std::declval<const value_type&>()))> {
            using new_type = decltype(std::invoke(std::forward<Function>(function), std::declval<const value_type&>()));
            if (is_error()) {
                return result_t<new_type>(error());
            }
            return result_t<new_type>(std::invoke(std::forward<Function>(function), value()));
        }

        /**
         * @brief 对成功值进行映射转换（移动语义）
         */
        template <typename Function>
        auto map(Function&& function) && -> result_t<decltype(std::invoke(std::forward<Function>(function),
                                                                           std::move(value())))> {
            using new_type = decltype(std::invoke(std::forward<Function>(function), std::move(value())));
            if (is_error()) {
                return result_t<new_type>(std::move(error()));
            }
            return result_t<new_type>(std::invoke(std::forward<Function>(function), std::move(value())));
        }

        /**
         * @brief 对错误上下文进行映射转换
         * @tparam Function 映射函数
         * @param function 映射函数，接受 error_context_t 返回新的 error_context_t
         * @return result_t 映射后的结果，成功时保持不变
         */
        template <typename Function>
        result_t<value_type> map_error(Function&& function) const& {
            if (is_error()) {
                return result_t<value_type>(std::invoke(std::forward<Function>(function), error()));
            }
            return result_t<value_type>(value());
        }

        /**
         * @brief 对错误上下文进行映射转换（移动语义）
         */
        template <typename Function>
        result_t<value_type> map_error(Function&& function) && {
            if (is_error()) {
                return result_t<value_type>(std::invoke(std::forward<Function>(function), std::move(error())));
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
        auto and_then(Function&& function) && -> decltype(std::invoke(std::forward<Function>(function),
                                                                      std::move(value()))) {
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
         * @param ctx 错误上下文
         * @return result_t<void> 包装了错误的结果对象
         */
        static result_t<void> make_error(const error_context_t& ctx) noexcept {
            return result_t<void>(ctx);
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
         * @brief 断言当前结果为成功（带自定义消息）
         * @details 若当前为错误状态，在 Debug 模式下触发断言失败并输出 msg。
         *          在 Release 模式下为无操作。适用于 void 返回类型中确认不会出错的场景。
         * @param msg 断言失败时的提示消息
         *
         * @example
         * auto result = do_something();
         * result.expect("do_something() should never fail here");
         */
        void expect(const char* msg) const noexcept {
            assert(is_success() && msg);
            (void)msg;
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

}  // namespace error_system::core