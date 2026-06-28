#pragma once
#include <cstddef>
#include <cstdint>

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @file string_utils.h
 * @brief 字符串工具函数
 * @details 定义字符串相关的工具函数，用于处理字符串的哈希、变换、解析等操作
 * @author yiice
 * @version 3.0.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils {
    /**
     * @brief 字符串工具函数
     * @details 提供通用字符串操作: 哈希计算、前缀/后缀检测、数字解析、字符串变换等
     *          格式化功能已迁移至 string_format_t
     *          JSON 转义功能已迁移至 json_serializer_t
     */
    class string_utils_t {
    private:
        string_utils_t() = delete;

        ~string_utils_t() noexcept = delete;

        string_utils_t(const string_utils_t&) = delete;

        string_utils_t& operator=(const string_utils_t&) = delete;

        string_utils_t(string_utils_t&&) = delete;

        string_utils_t& operator=(string_utils_t&&) = delete;

    public:
        /**
         * @brief 计算字符串的 FNV-1a 哈希值
         * @details 使用 FNV-1a 算法计算字符串哈希，支持 constexpr
         * @param string 输入字符串
         * @return uint64_t 字符串的哈希值
         */
        [[nodiscard]] static constexpr uint64_t hash(std::string_view string) noexcept {
            constexpr uint64_t FNV_PRIME = 1099511628211ULL;
            constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
            uint64_t hash_value = FNV_OFFSET_BASIS;

            for (char ch : string) {
                hash_value ^= static_cast<uint64_t>(ch);
                hash_value *= FNV_PRIME;
            }

            return hash_value;
        }

        /**
         * @brief 计算字符串的哈希值，限制哈希长度
         * @details 对超过 max_length 的字符串仅计算前 max_length 个字符的哈希
         * @param string 输入字符串
         * @param max_length 最大哈希长度
         * @return uint64_t 字符串的哈希值
         */
        [[nodiscard]] static constexpr uint64_t hash_limit(std::string_view string, size_t max_length = 128) noexcept {
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
        [[nodiscard]] static constexpr bool starts_with(std::string_view string, std::string_view prefix) noexcept {
            return string.size() >= prefix.size() && string.compare(0, prefix.size(), prefix) == 0;
        }

        /**
         * @brief 检查字符串是否以指定后缀结尾
         * @param string 输入字符串
         * @param suffix 后缀
         * @return bool 是否以指定后缀结尾
         */
        [[nodiscard]] static constexpr bool ends_with(std::string_view string, std::string_view suffix) noexcept {
            return string.size() >= suffix.size() &&
                   string.compare(string.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        /**
         * @brief 将字符串解析为数字
         * @details 使用 std::from_chars 进行高效解析
         * @tparam T 目标数字类型
         * @param string 输入字符串
         * @return std::optional<T> 解析后的数字，失败返回 nullopt
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
         * @brief 替换字符串中所有的指定子串
         * @param string 输入字符串
         * @param from 要替换的子串
         * @param to 替换后的子串
         * @return std::string 替换后的字符串
         */
        static std::string replace_all(std::string string, std::string_view from, std::string_view to) noexcept;

        /**
         * @brief 分割字符串视图
         * @param string 输入字符串视图
         * @param delimiter 分隔符
         * @return std::vector<std::string_view> 分割后的字符串视图向量
         */
        [[nodiscard]] static std::vector<std::string_view> split(std::string_view string, std::string_view delimiter) noexcept;

        /**
         * @brief 合并字符串视图向量
         * @param tokens 输入字符串视图向量
         * @param delimiter 分隔符
         * @return std::string 合并后的字符串
         */
        [[nodiscard]] static std::string join(const std::vector<std::string_view>& tokens, std::string_view delimiter) noexcept;

        /**
         * @brief 移除字符串视图首尾的空白符
         * @param string 输入字符串视图
         * @return std::string_view 移除空白符后的字符串视图
         */
        [[nodiscard]] static std::string_view trim(std::string_view string) noexcept;

        /**
         * @brief 将字符串视图转换为小写
         * @param string 输入字符串视图
         * @return std::string 转换后的字符串
         */
        [[nodiscard]] static std::string to_lower(std::string_view string) noexcept;

        /**
         * @brief 将字符串视图转换为大写
         * @param string 输入字符串视图
         * @return std::string 转换后的字符串
         */
        [[nodiscard]] static std::string to_upper(std::string_view string) noexcept;
    };

}  // namespace error_system::utils
