#pragma once
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
// IWYU pragma: begin_exports
#include <algorithm>
// IWYU pragma: end_exports

/**
 * @file string_utils.h
 * @brief 字符串工具函数
 * @details 定义字符串相关的工具函数，用于处理字符串
 * @author yiice
 * @version 1.0.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils {
    namespace {
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
         * @brief 检查类型 T 是否具有成员函数 to_string()
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
         * @brief 检查类型 T 是否具有全局函数 to_string()
         * @tparam T 待检查类型
         * @details 基于 SFINAE 检测 to_string(T) 是否存在
         *          默认模板（无匹配时）继承 std::false_type
         * @return bool 是否有全局函数 to_string()
         */
        template <typename T>
        struct is_global_to_string_t<T, std::void_t<decltype(to_string(std::declval<const T&>()))>>
            : std::is_convertible<decltype(to_string(std::declval<const T&>())), std::string_view> {};

        /**
         * @brief 检查类型 T 是否具有成员函数 to_string()
         * @tparam T 待检查类型
         * @details 基于 SFINAE 检测 T::to_string() 是否存在
         *          默认模板（无匹配时）继承 std::false_type
         * @return bool 是否有成员函数 to_string()
         */
        template <typename T>
        constexpr bool is_member_to_string_v = is_member_to_string_t<T>::value;

        /**
         * @brief 检查类型 T 是否具有全局函数 to_string()
         * @tparam T 待检查类型
         * @details 基于 SFINAE 检测 to_string(T) 是否存在
         *          默认模板（无匹配时）继承 std::false_type
         * @return bool 是否有全局函数 to_string()
         */
        template <typename T>
        constexpr bool is_global_to_string_v = is_global_to_string_t<T>::value;

    }  // namespace
    /**
     * @brief 字符串工具函数
     * @details 定义字符串相关的工具函数，用于处理字符串
     */
    class string_utils_t {
        private:
        struct format_appender {
            std::string& result;
            std::string_view format;
            size_t cursor = 0;

            template <typename T>
            void append_value(const T& value) noexcept {
                size_t pos = format.find("{}", cursor);
                if (pos == std::string_view::npos) {
                    return;
                }
                result.append(format.data() + cursor, pos - cursor);
                cursor = pos + 2;

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
                            result.append(buffer.data(), pointer - buffer.data());
                        }
                    }
                } else if constexpr (std::is_convertible_v<T, std::string_view>) {
                    result.append(std::string_view(value));
                } else if constexpr (std::is_same_v<T, char>) {
                    result.push_back(value);
                } else if constexpr (std::is_arithmetic_v<T>) {
                    if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
                        result.append(value ? "true" : "false");
                    } else {
                        std::array<char, 64> buffer;
                        auto [pointer, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
                        if (error == std::errc{}) {
                            result.append(buffer.data(), pointer - buffer.data());
                        }
                    }
                } else if constexpr (is_member_to_string_v<T>) {
                    result.append(value.to_string());
                } else if constexpr (is_global_to_string_v<T>) {
                    using namespace std;
                    result.append(to_string(value));
                } else {
                    result.append("[unsupported type]");
                }
            }

            void finish() noexcept {
                if (cursor < format.size()) {
                    result.append(format.data() + cursor, format.size() - cursor);
                }
            }
        };

        private:
        string_utils_t() = delete;

        ~string_utils_t() = delete;

        string_utils_t(const string_utils_t&) = delete;

        string_utils_t& operator=(const string_utils_t&) = delete;

        string_utils_t(string_utils_t&&) = delete;

        string_utils_t& operator=(string_utils_t&&) = delete;

        public:
        /**
         * @brief 计算字符串的哈希值
         * @details 计算字符串的哈希值
         * @param string 输入字符串
         * @param max_hash_length 最大哈希长度
         * @return size_t 字符串的哈希值
         */
        static constexpr uint64_t hash(std::string_view string) noexcept {
            constexpr uint64_t fnv_prime = 1099511628211ULL;
            constexpr uint64_t fnv_offset_basis = 14695981039346656037ULL;
            uint64_t hash = fnv_offset_basis;

            for (char ch : string) {
                hash ^= static_cast<uint64_t>(ch);
                hash *= fnv_prime;
            }

            return hash;
        }

        /**
         * @brief 计算字符串的哈希值，限制哈希长度
         * @details 计算字符串的哈希值，限制哈希长度
         * @param string 输入字符串
         * @param max_length 最大哈希长度
         * @return size_t 字符串的哈希值
         */
        static constexpr uint64_t hash_limit(std::string_view string, size_t max_length = 128) noexcept {
            if (string.size() > max_length) {
                return hash(string.substr(0, max_length));
            }
            return hash(string);
        }

        /**
         * @brief 检查字符串是否以指定前缀开头
         * @param string 输入字符串
         * @param prefix 前缀
         * @return bool 是否以指定前缀开头
         */
        static constexpr bool starts_with(std::string_view string, std::string_view prefix) noexcept {
            return string.size() >= prefix.size() && string.compare(0, prefix.size(), prefix) == 0;
        }

        /**
         * @brief 检查字符串是否以指定后缀结尾
         * @details 检查字符串是否以指定后缀结尾
         * @param string 输入字符串
         * @param suffix 后缀
         * @return bool 是否以指定后缀结尾
         */
        static constexpr bool ends_with(std::string_view string, std::string_view suffix) noexcept {
            return string.size() >= suffix.size() &&
                   string.compare(string.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        /**
         * @brief 将字符串解析为数字
         * @details 将字符串解析为指定类型的数字
         * @param string 输入字符串
         * @return std::optional<T> 解析后的数字
         */
        template <typename T>
        static inline std::optional<T> parse_number(std::string_view string) noexcept {
            T value{};
            auto [pointer, error] = std::from_chars(string.data(), string.data() + string.size(), value);
            if (error == std::errc{}) {
                return value;
            }
            return std::nullopt;
        }

        /**
         * @brief 格式化字符串
         * @details 格式化字符串，将字符串中的占位符替换为实际值
         * @tparam ...Args 待格式化的参数类型
         * @param format 格式化字符串
         * @param args 待格式化的参数
         * @return std::string 格式化后的字符串
         */
        template <typename... Args>
        static inline std::string format(std::string_view format, Args&&... args) noexcept {
            std::string result{};

            result.reserve(format.size() + (sizeof...(args) * 16));

            format_appender appender{result, format, 0};

            (appender.append_value(std::forward<Args>(args)), ...);

            appender.finish();

            return result;
        }

        /**
         * @brief 替换字符串中所有的指定子串
         * @details 注意：因为要生成新字符串，所以返回 std::string
         * @param string 输入字符串
         * @param from 要替换的子串
         * @param to 替换后的子串
         * @return std::string 替换后的字符串
         */
        static std::string replace_all(std::string string, std::string_view from, std::string_view to) noexcept;

        /**
         * @brief 分割字符串
         * @details 将字符串根据指定分隔符分割为多个字符串视图
         * @param string 输入字符串
         * @param delimiter 分隔符
         * @return std::vector<std::string_view> 分割后的字符串视图向量
         */
        static std::vector<std::string_view> split(std::string_view string, std::string_view delimiter) noexcept;

        /**
         * @brief 合并字符串视图向量
         * @details 将字符串视图向量中的字符串用指定分隔符连接起来
         * @param tokens 输入字符串视图向量
         * @param delimiter 分隔符
         * @return std::string 合并后的字符串
         */
        static std::string join(const std::vector<std::string_view>& tokens, std::string_view delimiter) noexcept;

        /**
         * @brief 移除字符串首尾的空白符
         * @details 移除字符串首尾的空白符，包括空格、制表符、换行符、回页符和换页符
         * @param string 输入字符串
         * @return std::string_view 移除空白符后的字符串视图
         */
        static std::string_view trim(std::string_view string) noexcept;

        /**
         * @brief 将字符串转换为小写
         * @details 将字符串中的所有字符转换为小写
         * @param string 输入字符串
         * @return std::string 转换后的字符串
         */
        static std::string to_lower(std::string_view string) noexcept;

        /**
         * @brief 将字符串转换为大写
         * @details 将字符串中的所有字符转换为大写
         * @param string 输入字符串
         * @return std::string 转换后的字符串
         */
        static std::string to_upper(std::string_view string) noexcept;

        /**
         * @brief 安全转义 JSON 字符串
         * @details 将包含控制字符的字符串转义为合法的 JSON 字符串格式
         * @param string 输入字符串
         * @return std::string 转义后的字符串
         */
        static std::string escape_json(std::string_view string) noexcept;
    };

}  // namespace error_system::utils