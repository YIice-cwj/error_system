#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

/**
 * @file string_format.h
 * @brief 字符串格式化引擎
 * @details 提供 std::format 风格的占位符替换功能，支持算术类型、指针、bool 及自定义 to_string 类型
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils {
    /**
     * @brief 检查类型 T 是否具有成员函数 to_string()
     * @tparam T 待检查类型
     * @details 基于 SFINAE 检测 T::to_string() 是否存在
     *          默认模板（无匹配时）继承 std::false_type
     * @return bool 是否有成员函数 to_string()
     */
    template <typename T, typename = std::void_t<>>
    struct is_member_to_string_t : std::false_type {};

    /**
     * @brief 检查类型 T 是否具有成员函数 to_string()（特化版本）
     * @tparam T 待检查类型
     * @details 基于 SFINAE 检测 T::to_string() 是否存在
     *          默认模板（无匹配时）继承 std::false_type
     * @return bool 是否有成员函数 to_string()
     */
    template <typename T>
    struct is_member_to_string_t<T, std::void_t<decltype(std::declval<const T&>().to_string())>>
        : std::is_convertible<decltype(std::declval<const T&>().to_string()), std::string_view> {};

    /**
     * @brief 检查类型 T 是否具有全局函数 to_string()
     * @tparam T 待检查类型
     * @details 基于 SFINAE 检测 to_string(T) 是否存在
     *          默认模板（无匹配时）继承 std::false_type
     * @return bool 是否有全局函数 to_string()
     */
    template <typename T, typename = std::void_t<>>
    struct is_global_to_string_t : std::false_type {};

    /**
     * @brief 检查类型 T 是否具有全局函数 to_string()（特化版本）
     * @tparam T 待检查类型
     * @details 基于 SFINAE 检测 to_string(T) 是否存在
     *          默认模板（无匹配时）继承 std::false_type
     * @return bool 是否有全局函数 to_string()
     */
    template <typename T>
    struct is_global_to_string_t<T, std::void_t<decltype(to_string(std::declval<const T&>()))>>
        : std::is_convertible<decltype(to_string(std::declval<const T&>())), std::string_view> {};

    /**
     * @brief is_member_to_string_t 的便捷变量模板
     * @tparam T 待检查类型
     * @details 等价于 is_member_to_string_t<T>::value
     */
    template <typename T>
    constexpr bool is_member_to_string_v = is_member_to_string_t<T>::value;

    /**
     * @brief is_global_to_string_t 的便捷变量模板
     * @tparam T 待检查类型
     * @details 等价于 is_global_to_string_t<T>::value
     */
    template <typename T>
    constexpr bool is_global_to_string_v = is_global_to_string_t<T>::value;

    /**
     * @brief 字符串格式化工具
     * @details 提供 std::format 风格的占位符替换功能
     *          支持: 算术类型、指针、bool、具有 to_string() 成员/全局函数的类型
     * @since 1.0.0
     */
    class string_format_t {
    private:
        /**
         * @brief 格式化追加器
         * @details 内部辅助结构，负责遍历格式字符串并追加字面量和参数值
         */
        struct format_appender_t {
            std::string& result;
            std::string_view format;
            size_t cursor = 0;

            /**
             * @brief 追加字面量大括号
             * @details 遍历格式字符串，将非占位符部分追加到结果中
             *          支持 {{ 和 }} 转义
             */
            void append_literal_braces() noexcept {
                while (cursor < format.size()) {
                    if (format[cursor] == '{') {
                        if (cursor + 1 < format.size() && format[cursor + 1] == '{') {
                            result.push_back('{');
                            cursor += 2;
                            continue;
                        }
                        break;
                    }
                    if (format[cursor] == '}') {
                        if (cursor + 1 < format.size() && format[cursor + 1] == '}') {
                            result.push_back('}');
                            cursor += 2;
                            continue;
                        }
                    }
                    result.push_back(format[cursor]);
                    ++cursor;
                }
            }

            /**
             * @brief 追加参数值
             * @tparam T 参数类型
             * @param value 待追加的参数值
             * @details 根据参数类型选择最佳追加方式:
             *          - string_view 可转换: 直接追加
             *          - char: 追加单字符
             *          - 指针: 追加十六进制地址
             *          - 算术类型: 使用 to_chars 追加
             *          - 有 to_string(): 调用 to_string()
             *          - 其他: 追加 "[unsupported type]"
             */
            template <typename T>
            void append_value(const T& value) noexcept {
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

            /**
             * @brief 完成格式化
             * @details 追加剩余的字面量部分
             */
            void finish() noexcept {
                append_literal_braces();
                if (cursor < format.size()) {
                    result.append(format.data() + cursor, format.size() - cursor);
                }
            }
        };

        string_format_t() = delete;

        ~string_format_t() noexcept = delete;

        string_format_t(const string_format_t&) = delete;

        string_format_t& operator=(const string_format_t&) = delete;

        string_format_t(string_format_t&&) = delete;

        string_format_t& operator=(string_format_t&&) = delete;

    public:
        /**
         * @brief 格式化字符串
         * @details 格式化字符串，将字符串中的占位符 {} 替换为实际值
         * @tparam ...Args 待格式化的参数类型
         * @param format_str 格式化字符串
         * @param args 待格式化的参数
         * @return std::string 格式化后的字符串
         * @par 使用示例:
         * @code
         * auto msg = string_format_t::format("code: {}, msg: {}", 404, "NF");
         * // 结果: "code: 404, msg: NF"
         * @endcode
         */
        template <typename... Args>
        [[nodiscard]] static inline std::string format(std::string_view format_str, Args&&... args) noexcept {
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
            } catch (...) {
                std::fprintf(stderr, "[string_format] format: reserve failed\n");
            }

            format_appender_t appender{result, format_str, 0};

            (appender.append_value(std::forward<Args>(args)), ...);

            appender.finish();

            return result;
        }
    };

}  // namespace error_system::utils
