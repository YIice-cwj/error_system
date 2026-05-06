#include "error_system/utils/string_utils.h"

namespace error_system::utils {

    /**
     * @brief 替换字符串中所有的指定子串
     * @details 注意：因为要生成新字符串，所以返回 std::string
     * @param string 输入字符串
     * @param from 要替换的子串
     * @param to 替换后的子串
     * @return std::string 替换后的字符串
     */
    std::string string_utils_t::replace_all(std::string string, std::string_view from, std::string_view to) noexcept {
        if (from.empty()) {
            return string;
        }

        size_t start_pos = 0;
        while ((start_pos = string.find(from, start_pos)) != std::string::npos) {
            string.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return string;
    }

    /**
     * @brief 分割字符串
     * @details 将字符串根据指定分隔符分割为多个字符串视图
     * @param string 输入字符串
     * @param delimiter 分隔符
     * @return std::vector<std::string_view> 分割后的字符串视图向量
     */
    std::vector<std::string_view> string_utils_t::split(std::string_view string, std::string_view delimiter) noexcept {
        if (string.empty()) {
            return {};
        }

        std::vector<std::string_view> result;
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
     * @details 将字符串视图向量中的字符串用指定分隔符连接起来
     * @param tokens 输入字符串视图向量
     * @param delimiter 分隔符
     * @return std::string 合并后的字符串
     */
    std::string string_utils_t::join(const std::vector<std::string_view>& tokens, std::string_view delimiter) noexcept {
        if (tokens.empty()) {
            return {};
        }
        std::string result{tokens[0]};
        for (size_t i = 1; i < tokens.size(); ++i) {
            result += delimiter;
            result += tokens[i];
        }
        return result;
    }

    /**
     * @brief 移除字符串首尾的空白符
     * @details 移除字符串首尾的空白符，包括空格、制表符、换行符、回页符和换页符
     * @param string 输入字符串
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
     * @brief 将字符串转换为小写
     * @details 将字符串中的所有字符转换为小写
     * @param string 输入字符串
     * @return std::string 小写后的字符串
     */
    std::string string_utils_t::to_lower(std::string_view string) noexcept {
        std::string result(string);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    /**
     * @brief 将字符串转换为大写
     * @details 将字符串中的所有字符转换为大写
     * @param string 输入字符串
     * @return std::string 大写后的字符串
     */
    std::string string_utils_t::to_upper(std::string_view string) noexcept {
        std::string result(string);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::toupper(c); });
        return result;
    }

    /**
     * @brief 安全转义 JSON 字符串
     * @details 将包含控制字符的字符串转义为合法的 JSON 字符串格式
     * @param string 输入字符串
     * @return std::string 转义后的字符串
     */
    std::string string_utils_t::escape_json(std::string_view string) noexcept {
        std::string result;
        result.reserve(string.size() + 10);  // 预分配一点额外空间
        for (char c : string) {
            switch (c) {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\b':
                    result += "\\b";
                    break;
                case '\f':
                    result += "\\f";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        // 处理其他不可见的控制字符
                        char buf[7];
                        snprintf(buf, sizeof(buf), "\\u%04x", c);
                        result += buf;
                    } else {
                        result += c;
                    }
            }
        }
        return result;
    }
}  // namespace error_system::utils
