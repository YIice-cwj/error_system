#pragma once
#include "error_system/core/error_context.h"
#include "error_system/core/result.h"
#include <exception>
#include <type_traits>

/**
 * @file try_catch.h
 * @brief try/catch 便捷包装，将异常自动转换为 result_t 错误
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-11
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 执行函数并自动捕获异常，转换为 result_t 错误
     * @details 包装 try/catch 模式，将 std::exception::what() 作为错误消息。
     *          通过 if constexpr 自动区分 void 和非 void 返回值。
     * @tparam Func 可调用对象类型
     * @param error_code 异常时使用的错误码
     * @param func 需要执行的函数
     * @return result_t 函数返回值或错误上下文
     *
     * @example
     * // 非 void 版本
     * auto result = try_catch_or_error(db_errors::ERR_TIMEOUT, []() {
     *     return database.query("SELECT * FROM users");
     * });
     * if (result) { auto data = result.value(); }
     *
     * // void 版本
     * auto result = try_catch_or_error(db_errors::ERR_FLUSH, []() {
     *     database.flush_logs();
     * });
     */
    template <typename Func>
    auto try_catch_or_error(error_code_t error_code, Func&& func) noexcept {
        using value_type = decltype(func());
        try {
            if constexpr (std::is_void_v<value_type>) {
                func();
                return result_t<void>{};
            } else {
                return result_t<value_type>(func());
            }
        } catch (const std::exception& e) {
            if constexpr (std::is_void_v<value_type>) {
                return result_t<void>::make_error(error_code, e.what());
            } else {
                return result_t<value_type>::make_error(error_code, e.what());
            }
        } catch (...) {
            if constexpr (std::is_void_v<value_type>) {
                return result_t<void>::make_error(error_code, "unknown exception");
            } else {
                return result_t<value_type>::make_error(error_code, "unknown exception");
            }
        }
    }

}  // namespace error_system::core