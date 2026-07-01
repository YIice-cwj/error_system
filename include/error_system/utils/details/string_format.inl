#pragma once

namespace error_system::utils {

template <typename T>
void string_format_t::format_appender_t::append_value(const T& value) noexcept {
    append_literal_braces();

    if (cursor >= format.size() || format[cursor] != '{') {
        return;
    }
    cursor++;
    if (cursor < format.size() && format[cursor] == '}') {
        cursor++;
    } else {
        return;
    }

    if constexpr (std::is_convertible_v<T, std::string_view>) {
        result.append(std::string_view(value));
    } else if constexpr (std::is_same_v<T, char>) {
        result.push_back(value);
    } else if constexpr (std::is_pointer_v<std::decay_t<T>>) {
        if (value == nullptr) {
            result.append("nullptr");
        } else {
            result.append("0x");
            uintptr_t addr = reinterpret_cast<uintptr_t>(value);
            std::array<char, 32> buffer;
            auto [pointer, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), addr, 16);
            if (error == std::errc{}) {
                result.append(buffer.data(), static_cast<size_t>(pointer - buffer.data()));
            }
        }

    } else if constexpr (std::is_arithmetic_v<T>) {
        if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            result.append(value ? "true" : "false");
        } else {
            std::array<char, 64> buffer;
            auto [pointer, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if (error == std::errc{}) {
                result.append(buffer.data(), static_cast<size_t>(pointer - buffer.data()));
            }
        }
    } else if constexpr (is_member_to_string_v<T>) {
        result.append(value.to_string());
    } else if constexpr (is_global_to_string_v<T>) {
        result.append(to_string(value));
    } else {
        result.append("[unsupported type]");
    }
}

template <typename... Args>
std::string string_format_t::format(std::string_view format_str, Args&&... args) noexcept {
    std::string result{};

    size_t estimated_size = format_str.size();
    auto add_size = [&estimated_size](const auto& argument) {
        using arg_t = std::decay_t<decltype(argument)>;
        if constexpr (std::is_integral_v<arg_t>) {
            estimated_size += 24;
        } else if constexpr (std::is_floating_point_v<arg_t>) {
            estimated_size += 32;
        } else if constexpr (std::is_pointer_v<arg_t> || std::is_same_v<arg_t, std::nullptr_t>) {
            estimated_size += 24;
        } else {
            estimated_size += std::string_view{argument}.size();
        }
    };
    (add_size(args), ...);
    try {
        result.reserve(estimated_size);
    } catch (const std::bad_alloc&) {
        std::fprintf(stderr, "[string_format] format: reserve failed\n");
    }

    format_appender_t appender{result, format_str, 0};

    (appender.append_value(std::forward<Args>(args)), ...);

    appender.finish();

    return result;
}

}  // namespace error_system::utils
