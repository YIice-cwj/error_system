#pragma once
#include <cassert>
#include <functional>
#include <string_view>
#include <type_traits>
#include <variant>

#include "error_system/core/error_context.h"

/**
 * @file result.h
 * @brief 结果数据类定义
 * @details 定义结果数据结构、字段解析和访问接口
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    namespace detail {
        constexpr uint16_t FATAL_ERROR_NUMBER = 0xFFFE;  ///< 用户函数抛出异常时使用的 fatal 错误编号

        /**
         * @brief 构造用户函数抛出异常时的错误上下文
         * @details 在 map/and_then 中，当用户函数抛出异常时统一构造 fatal 错误上下文。
         *          错误码固定为 (fatal, none, 0, 0, FATAL_ERROR_NUMBER)。
         * @param message 错误消息
         * @return error_context_t fatal 错误上下文
         */
        inline error_context_t make_invoke_exception_context(const char* message) noexcept {
            return error_context_t{
                located_code_t{error_code_t(error_level_t::fatal, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{FATAL_ERROR_NUMBER})},
                message};
        }
    }  // namespace detail

    /**
     * @brief 结果数据类
     * @details 封装结果信息，提供字段解析和访问功能
     * @tparam T 结果类型
     */
    template <typename T>
    class result_t {
    public:
        using value_type_t = T;

    private:
        std::variant<value_type_t, error_context_t> value_or_error_{};

    public:
        /**
         * @brief 错误构造工厂函数（推荐）
         * @details 替代直接使用构造函数构造错误结果，语义更清晰。
         *          避免构造函数重载混淆（value vs error_context vs error_code）。
         * @param code 错误码
         * @param message 错误信息，默认为空
         * @param location 源位置（默认捕获调用者位置）
         * @return result_t 包装了错误的结果对象
         *
         * @example
         * // 替代 result_t<int>(code, "失败")
         * return result_t<int>::make_error(ERR_DB_FAIL, "数据库操作失败");
         */
        [[nodiscard]] static result_t make_error(error_code_t code, const std::string& message = "",
                                                 utils::source_location_t location = utils::source_location_t::current()) noexcept;

        /**
         * @brief 错误构造工厂函数（移动消息版本）
         * @param code 错误码
         * @param message 错误信息
         * @param location 源位置（默认捕获调用者位置）
         * @return result_t 包装了错误的结果对象
         */
        [[nodiscard]] static result_t make_error(error_code_t code, std::string&& message,
                                                 utils::source_location_t location = utils::source_location_t::current()) noexcept;

        /**
         * @brief 错误构造工厂函数（从已有 error_context_t）
         * @param context 错误上下文
         * @return result_t 包装了错误的结果对象
         */
        [[nodiscard]] static result_t make_error(const error_context_t& context) noexcept;

        /**
         * @brief 创建成功结果
         * @details 工厂方法，创建包含成功值的结果
         * @param value 成功值
         * @return result_t 成功结果
         */
        [[nodiscard]] static result_t make_success(value_type_t value) noexcept;

        /**
         * @brief 构造函数
         * @param value 成功值
         */
        explicit result_t(const value_type_t& value) noexcept;

        /**
         * @brief 移动构造函数
         * @param value 成功值
         */
        explicit result_t(value_type_t&& value) noexcept;

        /**
         * @brief 构造函数
         * @param error_context 错误上下文
         */
        explicit result_t(const error_context_t& error_context) noexcept;

        /**
         * @brief 拷贝构造，noexcept 性跟随 value_type_t 与 error_context_t
         */
        result_t(const result_t&) noexcept(std::is_nothrow_copy_constructible_v<value_type_t>
                                           && std::is_nothrow_copy_constructible_v<error_context_t>) = default;
        /**
         * @brief 移动构造，noexcept 性跟随 value_type_t
         * @details error_context_t 已保证 noexcept 移动。无条件 noexcept 会在 T 抛出移动异常时调用 std::terminate。
         */
        result_t(result_t&&) noexcept(std::is_nothrow_move_constructible_v<value_type_t>) = default;
        result_t& operator=(const result_t&) noexcept = delete;
        /**
         * @brief 移动赋值，noexcept 性跟随 value_type_t
         * @details error_context_t 已保证 noexcept 移动赋值。允许复用变量，改善易用性。
         */
        result_t& operator=(result_t&&) noexcept(std::is_nothrow_move_assignable_v<value_type_t>) = default;
        ~result_t() noexcept = default;

        /**
         * @brief 检查结果是否为错误
         * @return bool 如果结果为错误则返回true
         */
        [[nodiscard]] bool is_error() const noexcept;

        /**
         * @brief 检查结果是否为成功
         * @return bool 如果结果为成功则返回true
         */
        [[nodiscard]] bool is_success() const noexcept;

        /**
         * @brief 获取错误上下文
         * @details 使用 std::get_if 安全获取，若当前为成功状态则返回静态哨兵值（空 error_context_t）。
         *          调用方应在调用前通过 is_error() 检查，否则返回的哨兵值 is_error() 为 false。
         * @return const error_context_t& 错误上下文
         */
        [[nodiscard]] const error_context_t& error() const noexcept;

        /**
         * @brief 获取错误上下文（可变引用）
         * @details 使用 std::get_if 安全获取，若当前为成功状态则返回线程局部哨兵值。
         *          调用方应在调用前通过 is_error() 检查，否则返回的哨兵值无意义。
         * @return error_context_t& 错误上下文可变引用
         */
        [[nodiscard]] error_context_t& error() noexcept;

        /**
         * @brief 获取成功值
         * @details 使用 std::get_if 安全获取，若当前为错误状态则返回静态哨兵值（T{}）。
         *          调用方应在调用前通过 is_success() 检查，否则返回的哨兵值可能无意义。
         *          要求 T 必须可默认构造，若 T 不可默认构造请使用 value_pointer() 替代。
         * @return const value_type_t& 成功值
         */
        [[nodiscard]] const value_type_t& value() const noexcept;

        /**
         * @brief 获取成功值（可变引用）
         * @details 使用 std::get_if 安全获取，若当前为错误状态则返回线程局部哨兵值（T{}）。
         *          调用方应在调用前通过 is_success() 检查，否则返回的哨兵值可能无意义。
         * @return value_type_t& 成功值
         */
        [[nodiscard]] value_type_t& value() noexcept;

        /**
         * @brief 安全获取成功值指针
         * @return const value_type_t* 成功值指针，如果为错误则返回 nullptr
         */
        [[nodiscard]] const value_type_t* value_pointer() const noexcept;

        /**
         * @brief 安全获取成功值指针
         * @return value_type_t* 成功值指针，如果为错误则返回 nullptr
         */
        [[nodiscard]] value_type_t* value_pointer() noexcept;

        /**
         * @brief 安全获取成功值，失败时返回默认值
         * @param default_value 默认值引用，调用方保证其生命周期
         * @return const value_type_t& 成功值或默认值引用
         */
        [[nodiscard]] const value_type_t& value_or(const value_type_t& default_value) const noexcept;

        /**
         * @brief 布尔转换运算符
         * @return bool 如果结果为成功则返回 true
         */
        explicit operator bool() const noexcept;

        /**
         * @brief 对成功值进行映射转换
         * @tparam Function 映射函数
         * @param function 映射函数，接受 value_type_t 返回新类型
         * @return result_t 转换后的结果，错误时传递错误上下文
         */
        template <typename Function>
        [[nodiscard]] auto map(Function&& function) const& noexcept
            -> result_t<decltype(std::invoke(std::forward<Function>(function),
                                             std::declval<const value_type_t&>()))>;

        /**
         * @brief 对成功值进行映射转换（移动语义）
         */
        template <typename Function>
        [[nodiscard]] auto map(Function&& function) && noexcept
            -> result_t<decltype(std::invoke(std::forward<Function>(function),
                                             std::move(value())))>;

        /**
         * @brief 对错误上下文进行映射转换
         * @tparam Function 映射函数
         * @param function 映射函数，接受 error_context_t 返回新的 error_context_t
         * @return result_t 映射后的结果，成功时保持不变
         */
        template <typename Function>
        [[nodiscard]] result_t<value_type_t> map_error(Function&& function) const& noexcept;

        /**
         * @brief 对错误上下文进行映射转换（移动语义）
         */
        template <typename Function>
        [[nodiscard]] result_t<value_type_t> map_error(Function&& function) && noexcept;

        /**
         * @brief 对结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function), std::move(value()))) 操作结果
         * @details 如果当前结果为成功，则调用 function 处理值并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误的新结果（移动语义）
         */
        template <typename Function>
        [[nodiscard]] auto and_then(Function&& function) && noexcept
            -> decltype(std::invoke(std::forward<Function>(function), std::move(value())));

        /**
         * @brief 对结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function), value())) 操作结果
         * @details 如果当前结果为成功，则调用 function 处理值并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误的新结果
         */
        template <typename Function>
        [[nodiscard]] auto and_then(Function&& function) & noexcept
            -> decltype(std::invoke(std::forward<Function>(function), value()));

        /**
         * @brief 对结果进行链式操作（const 左值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function), value())) 操作结果
         * @details 如果当前结果为成功，则调用 function 处理值并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误的新结果
         */
        template <typename Function>
        [[nodiscard]] auto and_then(Function&& function) const& noexcept
            -> decltype(std::invoke(std::forward<Function>(function), value()));

        /**
         * @brief 对错误结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<value_type_t> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误并返回其结果；
         *          如果当前结果为成功，则返回当前对象（移动语义）
         */
        template <typename Function>
        [[nodiscard]] result_t<value_type_t> or_else(Function&& function) && noexcept;

        /**
         * @brief 对错误结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<value_type_t> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误并返回其结果；
         *          如果当前结果为成功，则返回当前对象的副本
         */
        template <typename Function>
        [[nodiscard]] result_t<value_type_t> or_else(Function&& function) & noexcept;

        /**
         * @brief 模式匹配处理成功和错误两种路径
         * @details 若结果为成功，调用 success_fn；否则调用 error_fn。两个函数必须返回相同类型。
         *          该函数调用用户提供的回调，回调可能抛出异常，因此不标记 noexcept —— 异常会传播给调用方。
         *          要求返回类型可默认构造（用于理论上的哨兵路径，正常路径不会触发）。
         * @param success_fn 成功时的处理函数
         * @param error_fn 错误时的处理函数
         * @return 处理函数的返回值
         */
        template <typename SuccessFn, typename ErrorFn>
        [[nodiscard]] auto match(SuccessFn&& success_fn, ErrorFn&& error_fn) const
            -> decltype(success_fn(std::declval<const value_type_t&>()));
    };

    /**
     * @brief 模板特化：当 T 为 void 时，特化为不包含值的 result_t
     * @details 特化 result_t 类模板，当 T 为 void 时，不存储值，仅存储错误上下文
     */
    template <>
    class result_t<void> {
    public:
        using value_type_t = void;

    private:
        error_context_t error_context_;

    public:
        /**
         * @brief 构造函数
         */
        result_t() noexcept;

        result_t(const result_t&) noexcept = default;
        result_t(result_t&&) noexcept = default;
        result_t& operator=(const result_t&) noexcept = delete;
        result_t& operator=(result_t&&) noexcept = default;
        ~result_t() noexcept = default;

        /**
         * @brief 错误构造工厂函数
         * @param code 错误码
         * @param message 错误信息，默认为空
         * @param location 源位置（默认捕获调用者位置）
         * @return result_t<void> 包装了错误的结果对象
         */
        [[nodiscard]] static result_t<void> make_error(error_code_t code, const std::string& message = "",
                                                       utils::source_location_t location = utils::source_location_t::current()) noexcept;

        /**
         * @brief 错误构造工厂函数（移动消息版本）
         * @param code 错误码
         * @param message 错误信息
         * @param location 源位置（默认捕获调用者位置）
         * @return result_t<void> 包装了错误的结果对象
         */
        [[nodiscard]] static result_t<void> make_error(error_code_t code, std::string&& message,
                                                       utils::source_location_t location = utils::source_location_t::current()) noexcept;

        /**
         * @brief 错误构造工厂函数（从已有 error_context_t）
         * @param context 错误上下文
         * @return result_t<void> 包装了错误的结果对象
         */
        [[nodiscard]] static result_t<void> make_error(const error_context_t& context) noexcept;

        /**
         * @brief 创建成功结果
         * @details 工厂方法，创建成功结果（void 类型）
         * @return result_t<void> 成功结果
         */
        [[nodiscard]] static result_t make_success() noexcept;

        /**
         * @brief 构造函数
         * @param error_context 错误上下文
         */
        explicit result_t(const error_context_t& error_context) noexcept;

        /**
         * @brief 布尔转换运算符
         * @return bool 如果结果为成功则返回 true
         */
        explicit operator bool() const noexcept;

        /**
         * @brief 检查结果是否为错误
         * @return bool 如果结果为错误则返回true
         */
        [[nodiscard]] bool is_error() const noexcept;

        /**
         * @brief 检查结果是否为成功
         * @return bool 如果结果为成功则返回true
         */
        [[nodiscard]] bool is_success() const noexcept;

        /**
         * @brief 获取错误上下文
         * @return const error_context_t& 错误上下文
         */
        [[nodiscard]] const error_context_t& error() const noexcept;

        /**
         * @brief 对结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function))) 操作结果
         * @details 如果当前结果为成功，则调用 function 并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误上下文的新结果（移动语义）
         */
        template <typename Function>
        [[nodiscard]] auto and_then(Function&& function) && noexcept
            -> decltype(std::invoke(std::forward<Function>(function)));

        /**
         * @brief 对结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return decltype(std::invoke(std::forward<Function>(function))) 操作结果
         * @details 如果当前结果为成功，则调用 function 并返回其结果；
         *          如果当前结果为错误，则返回包含当前错误上下文的新结果
         */
        template <typename Function>
        [[nodiscard]] auto and_then(Function&& function) & noexcept
            -> decltype(std::invoke(std::forward<Function>(function)));

        /**
         * @brief 对错误上下文进行映射转换
         * @tparam Function 映射函数,接受 error_context_t 返回新的 error_context_t
         * @param function 映射函数
         * @return result_t<void> 映射后的结果,成功时保持不变
         */
        template <typename Function>
        [[nodiscard]] result_t<void> map_error(Function&& function) const& noexcept;

        /**
         * @brief 对错误上下文进行映射转换（移动语义）
         */
        template <typename Function>
        [[nodiscard]] result_t<void> map_error(Function&& function) && noexcept;

        /**
         * @brief 对错误结果进行链式操作（右值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<void> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误上下文并返回其结果；
         *          如果当前结果为成功，则返回当前对象（移动语义）
         */
        template <typename Function>
        [[nodiscard]] result_t<void> or_else(Function&& function) && noexcept;

        /**
         * @brief 对错误结果进行链式操作（左值引用版本）
         * @param function 要执行的操作函数
         * @return result_t<void> 操作结果
         * @details 如果当前结果为错误，则调用 function 处理错误上下文并返回其结果；
         *          如果当前结果为成功，则返回当前对象的副本
         */
        template <typename Function>
        [[nodiscard]] result_t<void> or_else(Function&& function) & noexcept;
    };

}  // namespace error_system::core

#include "details/result.inl"
