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
         * @brief 解引用访问成功值
         * @details 与 std::expected::operator* 语义一致。错误时返回线程局部哨兵值引用，
         *          调用方应先通过 operator bool / is_success() 检查。
         * @return const value_type_t& 成功值引用
         */
        [[nodiscard]] const value_type_t& operator*() const noexcept;

        /**
         * @brief 解引用访问成功值（可变引用）
         * @return value_type_t& 成功值可变引用
         */
        [[nodiscard]] value_type_t& operator*() noexcept;

        /**
         * @brief 箭头访问成功值成员
         * @details 与 std::expected::operator-> 语义一致。错误时返回线程局部哨兵地址，
         *          调用方应先通过 operator bool / is_success() 检查。
         * @return const value_type_t* 成功值地址
         */
        [[nodiscard]] const value_type_t* operator->() const noexcept;

        /**
         * @brief 箭头访问成功值成员（可变指针）
         * @return value_type_t* 成功值地址
         */
        [[nodiscard]] value_type_t* operator->() noexcept;

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
         *          noexcept 性跟随用户回调：仅当两个回调均为 noexcept 时，本方法才 noexcept；
         *          否则用户回调抛出的异常会传播给调用方。
         *          要求返回类型可默认构造（用于理论上的哨兵路径，正常路径不会触发）。
         * @param success_fn 成功时的处理函数
         * @param error_fn 错误时的处理函数
         * @return 处理函数的返回值
         */
        template <typename SuccessFn, typename ErrorFn>
        [[nodiscard]] auto match(SuccessFn&& success_fn, ErrorFn&& error_fn) const
            noexcept(std::is_nothrow_invocable_v<SuccessFn&, const value_type_t&>
                     && std::is_nothrow_invocable_v<ErrorFn&, const error_context_t&>)
            -> decltype(success_fn(std::declval<const value_type_t&>()));

        /**
         * @brief 传播时附加 payload 上下文（左值版本）
         * @details 错误时对内部 error_context 调用 with(key, value) 追加负载，成功时无操作。
         *          完美转发到 error_context_t::with() 的 7 个重载，避免 API 表面积膨胀。
         *          典型用法：`return inner_call().context("host", host_);`
         * @tparam K 键类型（支持 string/string_view/const char*）
         * @tparam V 值类型（支持 string 及任意可转 string 的类型）
         * @param key 负载键
         * @param value 负载值
         * @return result_t& 自身引用（错误时已附加 payload，成功时保持原样）
         */
        template <typename K, typename V>
        result_t& context(K&& key, V&& value) & noexcept;

        /**
         * @brief 传播时附加 payload 上下文（右值版本）
         * @details 与左值版本语义一致，返回移动后的新对象，适用于链式调用末尾
         * @return result_t 移动后的结果对象
         */
        template <typename K, typename V>
        [[nodiscard]] result_t context(K&& key, V&& value) && noexcept;
    };

    /**
     * @brief 模板特化：当 T 为 void 时，特化为不包含值的 result_t
     * @details 特化 result_t 类模板，当 T 为 void 时，不存储值。
     *          成功路径持有 std::monostate（零 error_context_t 构造开销），
     *          失败路径持有完整 error_context_t
     */
    template <>
    class result_t<void> {
    public:
        using value_type_t = void;

    private:
        std::variant<std::monostate, error_context_t> storage_;

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

        /**
         * @brief 传播时附加 payload 上下文（左值版本）
         * @details 错误时对内部 error_context 调用 with(key, value) 追加负载，成功时无操作。
         *          完美转发到 error_context_t::with() 的 7 个重载。
         * @return result_t<void>& 自身引用
         */
        template <typename K, typename V>
        result_t<void>& context(K&& key, V&& value) & noexcept;

        /**
         * @brief 传播时附加 payload 上下文（右值版本）
         * @return result_t<void> 移动后的结果对象
         */
        template <typename K, typename V>
        [[nodiscard]] result_t<void> context(K&& key, V&& value) && noexcept;
    };

}  // namespace error_system::core

/**
 * @brief 早返回宏：当 expr 为错误时，从当前函数返回同类型错误结果
 * @details 声明变量 var 并赋值为 expr；若 var 为错误，则通过 result_t<value_type_t>::make_error()
 *          构造与 var 相同 value 类型的错误结果并返回。外层函数返回类型必须与 expr 的 value 类型一致。
 *          适用于需要保留成功值供后续使用的场景。
 * @param var 变量名（在调用点可见）
 * @param expr 返回 result_t 的表达式
 * @note 该宏展开为多语句，必须作为独立语句使用。
 */
#ifndef ERROR_SYSTEM_TRY
#define ERROR_SYSTEM_TRY(var, expr) \
    auto var = (expr); \
    if ((var).is_error()) { \
        return ::error_system::core::result_t<typename std::decay_t<decltype(var)>::value_type_t>::make_error((var).error()); \
    }
#endif

/**
 * @brief 早返回宏（丢弃值版本）：当 expr 为错误时，从当前函数返回同类型错误结果
 * @details 不保留成功值，仅检查错误并早返回。外层函数返回类型必须与 expr 的 value 类型一致。
 * @param expr 返回 result_t 的表达式
 */
#ifndef ERROR_SYSTEM_TRY_DISCARD
#define ERROR_SYSTEM_TRY_DISCARD(expr) \
    do { \
        auto&& _error_system_tmp = (expr); \
        if (_error_system_tmp.is_error()) { \
            return ::error_system::core::result_t<typename std::decay_t<decltype(_error_system_tmp)>::value_type_t>::make_error(_error_system_tmp.error()); \
        } \
    } while (0)
#endif

#include "details/result.inl"
