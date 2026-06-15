#include "error_system/utils/string_utils.h"
#include <algorithm>
#include <array>
#include <charconv>

namespace error_system::utils {

    /**
     * @brief 替换字符串中所有的指定子串
     * @details 改造为单次扫描追加模式，将复杂度从原先的 O(N^2) 降为 O(N)
     */
    std::string string_utils_t::replace_all(std::string string, std::string_view from, std::string_view to) noexcept {
        if (from.empty() || string.empty()) {
            return string;
        }

        size_t start_pos = string.find(from);
        if (start_pos == std::string::npos) {
            return string;
        }

        std::string result{};
        if (to.size() > from.size()) {
            result.reserve(string.size() + (to.size() - from.size()) * 2);
        } else {
            result.reserve(string.size());
        }

        size_t current_pos = 0;
        while (start_pos != std::string::npos) {
            result.append(string.data() + current_pos, start_pos - current_pos);
            result.append(to);
            current_pos = start_pos + from.length();
            start_pos = string.find(from, current_pos);
        }

        if (current_pos < string.size()) {
            result.append(string.data() + current_pos, string.size() - current_pos);
        }

        return result;
    }

    /**
     * @brief 分割字符串视图，返回视图向量
     * @details 该函数不修改原始字符串，仅返回分割后的视图向量
     * @param string 输入字符串视图
     * @param delimiter 分隔符视图
     * @return std::vector<std::string_view> 分割后的视图向量
     */
    std::vector<std::string_view> string_utils_t::split(std::string_view string, std::string_view delimiter) noexcept {
        if (string.empty()) {
            return {};
        }
        std::vector<std::string_view> result{};
        size_t start = 0;
        size_t end = string.find(delimiter);
        while (end != std::string_view::npos) {
            if (start != end) {
                result.push_back(string.substr(start, end - start));
            }
            start = end + delimiter.length();
            end = string.find(delimiter, start);
        }
        if (start < string.length()) {
            result.push_back(string.substr(start));
        }
        return result;
    }

    /**
     * @brief 合并字符串视图向量
     * @details 该函数不修改原始视图向量，仅返回合并后的字符串视图
     * @param tokens 输入字符串视图向量
     * @param delimiter 分隔符视图
     * @return std::string 合并后的字符串
     */
    std::string string_utils_t::join(const std::vector<std::string_view>& tokens, std::string_view delimiter) noexcept {
        if (tokens.empty()) {
            return {};
        }
        size_t total_length = 0;
        for (const auto& token : tokens) {
            total_length += token.size();
        }
        total_length += delimiter.size() * (tokens.size() - 1);
        std::string result{};
        result.reserve(total_length);
        result.append(tokens[0]);
        for (size_t i = 1; i < tokens.size(); ++i) {
            result.append(delimiter);
            result.append(tokens[i]);
        }
        return result;
    }

    /**
     * @brief 移除字符串视图首尾的空白符
     * @details 该函数不修改原始字符串视图，仅返回移除空白符后的视图
     * @param string 输入字符串视图
     * @return std::string_view 移除空白符后的字符串视图
     */
    std::string_view string_utils_t::trim(std::string_view string) noexcept {
        constexpr std::string_view whitespace = " \t\n\r\f\v";
        size_t first = string.find_first_not_of(whitespace);
        if (first == std::string_view::npos) {
            return {};
        }
        size_t last = string.find_last_not_of(whitespace);
        return string.substr(first, (last - first + 1));
    }

    /**
     * @brief 将字符串视图转换为小写
     * @details 该函数不修改原始字符串视图，仅返回转换后的字符串视图
     * @param string 输入字符串视图
     * @return std::string 小写的字符串
     */
    std::string string_utils_t::to_lower(std::string_view string) noexcept {
        std::string result{};
        result.resize(string.size());
        std::transform(string.begin(), string.end(), result.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return result;
    }

    /**
     * @brief 将字符串视图转换为大写
     * @details 该函数不修改原始字符串视图，仅返回转换后的字符串视图
     * @param string 输入字符串视图
     * @return std::string 大写的字符串
     */
    std::string string_utils_t::to_upper(std::string_view string) noexcept {
        std::string result{};
        result.resize(string.size());

        std::transform(string.begin(), string.end(), result.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return result;
    }

    /**
     * @brief 安全转义 JSON 字符串视图
     * @details 该函数不修改原始字符串视图，仅返回转义后的字符串视图
     * @param string 输入字符串视图
     * @return std::string 转义后的字符串
     */
    std::string string_utils_t::escape_json(std::string_view string) noexcept {
        std::string result{};
        result.reserve(string.size() + 16);  // 预分配稍大一点的合理空间

        for (char c : string) {
            switch (c) {
                case '"':
                    result.append("\\\"");
                    break;
                case '\\':
                    result.append("\\\\");
                    break;
                case '\b':
                    result.append("\\b");
                    break;
                case '\f':
                    result.append("\\f");
                    break;
                case '\n':
                    result.append("\\n");
                    break;
                case '\r':
                    result.append("\\r");
                    break;
                case '\t':
                    result.append("\\t");
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        result.append("\\u00");
                        std::array<char, 2> buffer;
                        auto [ptr, ec] =
                            std::to_chars(buffer.data(), buffer.data() + buffer.size(), static_cast<uint8_t>(c), 16);
                        if (ec == std::errc{}) {
                            if (ptr - buffer.data() == 1) {
                                result.push_back('0');
                                result.push_back(buffer[0]);
                            } else {
                                result.append(buffer.data(), 2);
                            }
                        }
                    } else {
                        result.push_back(c);
                    }
            }
        }
        return result;
    }
}  // namespace error_system::utils