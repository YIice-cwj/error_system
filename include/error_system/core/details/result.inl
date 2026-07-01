#pragma once

/**
 * @file result.inl
 * @brief result_t 模板实现
 * @details result_t<T> 主模板与 result_t<void> 特化的成员函数实现
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    // ============================================================
    // result_t<T> 主模板实现
    // ============================================================

    template <typename T>
    result_t<T> result_t<T>::make_error(error_code_t code, const std::string& message,
                                        utils::source_location_t location) noexcept {
        return result_t(error_context_t{located_code_t{code, location}, message});
    }

    template <typename T>
    result_t<T> result_t<T>::make_error(error_code_t code, std::string&& message,
                                        utils::source_location_t location) noexcept {
        return result_t(error_context_t{located_code_t{code, location}, std::move(message)});
    }

    template <typename T>
    result_t<T> result_t<T>::make_error(const error_context_t& context) noexcept {
        return result_t(context);
    }

    template <typename T>
    result_t<T> result_t<T>::make_success(value_type_t value) noexcept {
        return result_t(std::move(value));
    }

    template <typename T>
    result_t<T>::result_t(const value_type_t& value) noexcept : value_or_error_(value) {}

    template <typename T>
    result_t<T>::result_t(value_type_t&& value) noexcept : value_or_error_(std::move(value)) {}

    template <typename T>
    result_t<T>::result_t(const error_context_t& error_context) noexcept : value_or_error_(error_context) {}

    template <typename T>
    bool result_t<T>::is_error() const noexcept {
        return std::holds_alternative<error_context_t>(value_or_error_);
    }

    template <typename T>
    bool result_t<T>::is_success() const noexcept {
        return std::holds_alternative<value_type_t>(value_or_error_);
    }

    template <typename T>
    const error_context_t& result_t<T>::error() const noexcept {
        assert(is_error() && "result_t::error() called on a success result");
        auto* ptr = std::get_if<error_context_t>(&value_or_error_);
        if (ptr) {
            return *ptr;
        }
        static thread_local const error_context_t sentinel{};
        return sentinel;
    }

    template <typename T>
    error_context_t& result_t<T>::error() noexcept {
        assert(is_error() && "result_t::error() called on a success result");
        auto* ptr = std::get_if<error_context_t>(&value_or_error_);
        if (ptr) {
            return *ptr;
        }
        static thread_local error_context_t sentinel{};
        return sentinel;
    }

    template <typename T>
    const T& result_t<T>::value() const noexcept {
        static_assert(std::is_default_constructible_v<value_type_t>,
                      "result_t::value() requires T to be default-constructible. "
                      "Use value_pointer() for non-default-constructible types.");
        assert(is_success() && "result_t::value() called on an error result");
        auto* ptr = std::get_if<value_type_t>(&value_or_error_);
        if (ptr) {
            return *ptr;
        }
        static thread_local const value_type_t sentinel{};
        return sentinel;
    }

    template <typename T>
    T& result_t<T>::value() noexcept {
        static_assert(std::is_default_constructible_v<value_type_t>,
                      "result_t::value() requires T to be default-constructible. "
                      "Use value_pointer() for non-default-constructible types.");
        assert(is_success() && "result_t::value() called on an error result");
        auto* ptr = std::get_if<value_type_t>(&value_or_error_);
        if (ptr) {
            return *ptr;
        }
        static thread_local value_type_t sentinel{};
        return sentinel;
    }

    template <typename T>
    const T* result_t<T>::value_pointer() const noexcept {
        return std::get_if<value_type_t>(&value_or_error_);
    }

    template <typename T>
    T* result_t<T>::value_pointer() noexcept {
        return std::get_if<value_type_t>(&value_or_error_);
    }

    template <typename T>
    const T& result_t<T>::value_or(const value_type_t& default_value) const noexcept {
        auto* ptr = std::get_if<value_type_t>(&value_or_error_);
        return ptr ? *ptr : default_value;
    }

    template <typename T>
    result_t<T>::operator bool() const noexcept {
        return is_success();
    }

    template <typename T>
    template <typename Function>
    auto result_t<T>::map(Function&& function) const& noexcept
        -> result_t<decltype(std::invoke(std::forward<Function>(function),
                                         std::declval<const value_type_t&>()))> {
        using new_type = decltype(std::invoke(std::forward<Function>(function), std::declval<const value_type_t&>()));
        if (is_error()) {
            return result_t<new_type>(error());
        }
        try {
            return result_t<new_type>(std::invoke(std::forward<Function>(function), value()));
        } catch (const std::exception&) {
            std::fprintf(stderr, "[result_t] map: std::invoke threw exception\n");
            return result_t<new_type>(detail::make_invoke_exception_context("map: function threw exception"));
        }
    }

    template <typename T>
    template <typename Function>
    auto result_t<T>::map(Function&& function) && noexcept
        -> result_t<decltype(std::invoke(std::forward<Function>(function),
                                         std::move(value())))> {
        using new_type = decltype(std::invoke(std::forward<Function>(function), std::move(value())));
        if (is_error()) {
            return result_t<new_type>(std::move(error()));
        }
        try {
            return result_t<new_type>(std::invoke(std::forward<Function>(function), std::move(value())));
        } catch (const std::exception&) {
            std::fprintf(stderr, "[result_t] map(&&): std::invoke threw exception\n");
            return result_t<new_type>(detail::make_invoke_exception_context("map(&&): function threw exception"));
        }
    }

    template <typename T>
    template <typename Function>
    result_t<T> result_t<T>::map_error(Function&& function) const& noexcept {
        if (is_error()) {
            try {
                return result_t<value_type_t>(std::invoke(std::forward<Function>(function), error()));
            } catch (const std::exception&) {
                std::fprintf(stderr, "[result_t] map_error: std::invoke threw exception\n");
                return result_t<value_type_t>(error());
            }
        }
        return result_t<value_type_t>(value());
    }

    template <typename T>
    template <typename Function>
    result_t<T> result_t<T>::map_error(Function&& function) && noexcept {
        if (is_error()) {
            try {
                return result_t<value_type_t>(std::invoke(std::forward<Function>(function), std::move(error())));
            } catch (const std::exception&) {
                std::fprintf(stderr, "[result_t] map_error(&&): std::invoke threw exception\n");
                return result_t<value_type_t>(detail::make_invoke_exception_context("map_error(&&): function threw exception"));
            }
        }
        return std::move(*this);
    }

    template <typename T>
    template <typename Function>
    auto result_t<T>::and_then(Function&& function) && noexcept
        -> decltype(std::invoke(std::forward<Function>(function), std::move(value()))) {
        using return_type = decltype(std::invoke(std::forward<Function>(function), std::move(value())));
        if (is_error()) {
            return return_type(std::move(error()));
        }
        try {
            return std::invoke(std::forward<Function>(function), std::move(value()));
        } catch (const std::exception&) {
            std::fprintf(stderr, "[result_t] and_then(&&): std::invoke threw exception\n");
            return return_type(detail::make_invoke_exception_context("and_then(&&): function threw exception"));
        }
    }

    template <typename T>
    template <typename Function>
    auto result_t<T>::and_then(Function&& function) & noexcept
        -> decltype(std::invoke(std::forward<Function>(function), value())) {
        using return_type = decltype(std::invoke(std::forward<Function>(function), value()));
        if (is_error()) {
            return return_type(error());
        }
        try {
            return std::invoke(std::forward<Function>(function), value());
        } catch (const std::exception&) {
            std::fprintf(stderr, "[result_t] and_then(&): std::invoke threw exception\n");
            return return_type(detail::make_invoke_exception_context("and_then(&): function threw exception"));
        }
    }

    template <typename T>
    template <typename Function>
    auto result_t<T>::and_then(Function&& function) const& noexcept
        -> decltype(std::invoke(std::forward<Function>(function), value())) {
        using return_type = decltype(std::invoke(std::forward<Function>(function), value()));
        if (is_error()) {
            return return_type(error());
        }
        try {
            return std::invoke(std::forward<Function>(function), value());
        } catch (const std::exception&) {
            std::fprintf(stderr, "[result_t] and_then(const&): std::invoke threw exception\n");
            return return_type(detail::make_invoke_exception_context("and_then(const&): function threw exception"));
        }
    }

    template <typename T>
    template <typename Function>
    result_t<T> result_t<T>::or_else(Function&& function) && noexcept {
        if (is_error()) {
            try {
                return std::invoke(std::forward<Function>(function), std::move(error()));
            } catch (const std::exception&) {
                std::fprintf(stderr, "[result_t] or_else(&&): std::invoke threw exception\n");
                return std::move(*this);
            }
        }
        return std::move(*this);
    }

    template <typename T>
    template <typename Function>
    result_t<T> result_t<T>::or_else(Function&& function) & noexcept {
        if (is_error()) {
            try {
                return std::invoke(std::forward<Function>(function), error());
            } catch (const std::exception&) {
                std::fprintf(stderr, "[result_t] or_else(&): std::invoke threw exception\n");
                return *this;
            }
        }
        return *this;
    }

    template <typename T>
    template <typename SuccessFn, typename ErrorFn>
    auto result_t<T>::match(SuccessFn&& success_fn, ErrorFn&& error_fn) const
        -> decltype(success_fn(std::declval<const value_type_t&>())) {
        if (is_success()) {
            auto* ptr = std::get_if<value_type_t>(&value_or_error_);
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

    // ============================================================
    // result_t<void> 特化实现
    // ============================================================

    inline result_t<void>::result_t() noexcept : error_context_{} {}

    inline result_t<void>::result_t(const error_context_t& error_context) noexcept
        : error_context_(error_context) {}

    inline result_t<void> result_t<void>::make_error(error_code_t code, const std::string& message,
                                                     utils::source_location_t location) noexcept {
        return result_t<void>(error_context_t{located_code_t{code, location}, message});
    }

    inline result_t<void> result_t<void>::make_error(error_code_t code, std::string&& message,
                                                     utils::source_location_t location) noexcept {
        return result_t<void>(error_context_t{located_code_t{code, location}, std::move(message)});
    }

    inline result_t<void> result_t<void>::make_error(const error_context_t& context) noexcept {
        return result_t<void>(context);
    }

    inline result_t<void> result_t<void>::make_success() noexcept {
        return result_t();
    }

    inline result_t<void>::operator bool() const noexcept {
        return is_success();
    }

    inline bool result_t<void>::is_error() const noexcept {
        return error_context_.is_error();
    }

    inline bool result_t<void>::is_success() const noexcept {
        return !is_error();
    }

    inline const error_context_t& result_t<void>::error() const noexcept {
        return error_context_;
    }

    template <typename Function>
    auto result_t<void>::and_then(Function&& function) && noexcept
        -> decltype(std::invoke(std::forward<Function>(function))) {
        using return_type = decltype(std::invoke(std::forward<Function>(function)));
        if (is_error()) {
            return return_type(std::move(error_context_));
        }
        try {
            return std::invoke(std::forward<Function>(function));
        } catch (const std::exception&) {
            std::fprintf(stderr, "[result_t<void>] and_then(&&): std::invoke threw exception\n");
            return return_type(detail::make_invoke_exception_context("and_then(&&): function threw exception"));
        }
    }

    template <typename Function>
    auto result_t<void>::and_then(Function&& function) & noexcept
        -> decltype(std::invoke(std::forward<Function>(function))) {
        using return_type = decltype(std::invoke(std::forward<Function>(function)));
        if (is_error()) {
            return return_type(error_context_);
        }
        try {
            return std::invoke(std::forward<Function>(function));
        } catch (const std::exception&) {
            std::fprintf(stderr, "[result_t<void>] and_then(&): std::invoke threw exception\n");
            return return_type(detail::make_invoke_exception_context("and_then(&): function threw exception"));
        }
    }

    template <typename Function>
    result_t<void> result_t<void>::map_error(Function&& function) const& noexcept {
        if (is_error()) {
            try {
                return result_t<void>(std::invoke(std::forward<Function>(function), error_context_));
            } catch (const std::exception&) {
                std::fprintf(stderr, "[result_t<void>] map_error: std::invoke threw exception\n");
                return result_t<void>(error_context_);
            }
        }
        return *this;
    }

    template <typename Function>
    result_t<void> result_t<void>::map_error(Function&& function) && noexcept {
        if (is_error()) {
            try {
                return result_t<void>(std::invoke(std::forward<Function>(function), std::move(error_context_)));
            } catch (const std::exception&) {
                std::fprintf(stderr, "[result_t<void>] map_error(&&): std::invoke threw exception\n");
                return result_t<void>(detail::make_invoke_exception_context("map_error(&&): function threw exception"));
            }
        }
        return std::move(*this);
    }

    template <typename Function>
    result_t<void> result_t<void>::or_else(Function&& function) && noexcept {
        if (is_error()) {
            try {
                return std::invoke(std::forward<Function>(function), std::move(error_context_));
            } catch (const std::exception&) {
                std::fprintf(stderr, "[result_t<void>] or_else(&&): std::invoke threw exception\n");
                return std::move(*this);
            }
        }
        return std::move(*this);
    }

    template <typename Function>
    result_t<void> result_t<void>::or_else(Function&& function) & noexcept {
        if (is_error()) {
            try {
                return std::invoke(std::forward<Function>(function), error_context_);
            } catch (const std::exception&) {
                std::fprintf(stderr, "[result_t<void>] or_else(&): std::invoke threw exception\n");
                return *this;
            }
        }
        return *this;
    }

}  // namespace error_system::core
