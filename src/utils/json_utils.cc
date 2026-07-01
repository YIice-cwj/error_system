#include "error_system/utils/json_utils.h"

/**
 * @file json_utils.cc
 * @brief JSON 字典与序列化工具实现
 * @details 实现基于状态机的 JSON 解析（支持嵌套对象与点分路径键）、JSON 字典查询、
 *          以及 JSON 字符串安全转义（含控制字符 \u00XX 编码）。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>

#include "error_system/utils/file_utils.h"
#include "error_system/utils/json_lexer.h"

namespace error_system::utils {
    namespace {
        using json_lexer_t = detail::json_lexer_t;
        /**
         * @brief 解析状态枚举
         * @details 定义JSON解析器的解析状态，用于标识当前解析位置
         */
        enum class parse_state_t {
            expect_key_or_end,
            expect_colon,
            expect_value_or_start,
            expect_comma_or_end
        };

        /**
         * @brief 解析上下文结构体
         * @details 用于存储解析器的状态、路径栈、当前键名、临时字典等信息
         */
        struct parser_context_t {
            std::vector<std::string> path_stack{};
            std::string current_key{};
            std::string path_prefix{};
            std::unordered_map<std::string, std::string> temp_dict{};
            parse_state_t state{parse_state_t::expect_key_or_end};
        };

        /**
         * @brief 处理期待键名或结束状态
         * @details 处理JSON解析器的解析状态为 expect_key_or_end 时的情况，包括遇到右括号 '}' 或键名（字符串）
         */
        bool handle_expect_key_or_end(parser_context_t& context, const json_lexer_t::token_t& token) noexcept {
            if (token.type == json_lexer_t::token_type_t::right_brace) {
                if (!context.path_stack.empty()) {
                    context.path_stack.pop_back();
                    auto dot_pos = context.path_prefix.rfind('.');
                    if (dot_pos != std::string::npos) {
                        context.path_prefix.resize(dot_pos);
                    } else {
                        context.path_prefix.clear();
                    }
                }
                context.state = parse_state_t::expect_comma_or_end;
                return true;
            }
            if (token.type == detail::json_lexer_t::token_type_t::string) {
                context.current_key = token.value;
                context.state = parse_state_t::expect_colon;
                return true;
            }
            return false;
        }

        /**
         * @brief 处理期待冒号状态
         * @details 处理JSON解析器的解析状态为 expect_colon 时的情况，包括遇到冒号 ':'
         */
        bool handle_expect_colon(parser_context_t& context, const json_lexer_t::token_t& token) noexcept {
            if (token.type == json_lexer_t::token_type_t::colon) {
                context.state = parse_state_t::expect_value_or_start;
                return true;
            }
            return false;
        }

        /**
         * @brief 处理期待值或嵌套起点状态
         * @details 处理JSON解析器的解析状态为 expect_value_or_start 时的情况，包括遇到左括号 '{' 或值（字符串）
         */
        bool handle_expect_value_or_start(parser_context_t& context, const json_lexer_t::token_t& token) noexcept {
            if (token.type == json_lexer_t::token_type_t::left_brace) {
                context.path_stack.push_back(context.current_key);
                if (context.path_prefix.empty()) {
                    context.path_prefix = context.current_key;
                } else {
                    context.path_prefix += ".";
                    context.path_prefix += context.current_key;
                }
                context.current_key.clear();
                context.state = parse_state_t::expect_key_or_end;
                return true;
            }
            if (token.type == json_lexer_t::token_type_t::string) {
                try {
                    std::string full_path;
                    full_path.reserve(context.path_prefix.size() + context.current_key.size() + 2);
                    if (!context.path_prefix.empty()) {
                        full_path = context.path_prefix;
                        full_path += ".";
                    }
                    full_path += context.current_key;
                    context.temp_dict.emplace(std::move(full_path), token.value);
                } catch (const std::bad_alloc&) {
                    std::fprintf(stderr, "[json_utils] handle_expect_value_or_start: emplace failed\n");
                }

                context.current_key.clear();
                context.state = parse_state_t::expect_comma_or_end;
                return true;
            }
            return false;
        }

        /**
         * @brief 处理期待逗号或结束状态
         * @details 处理JSON解析器的解析状态为 expect_comma_or_end 时的情况，包括遇到逗号 ',' 或右括号 '}'
         */
        bool handle_expect_comma_or_end(parser_context_t& context, const json_lexer_t::token_t& token) noexcept {
            if (token.type == json_lexer_t::token_type_t::comma) {
                context.state = parse_state_t::expect_key_or_end;
                return true;
            }
            if (token.type == json_lexer_t::token_type_t::right_brace) {
                if (!context.path_stack.empty()) {
                    context.path_stack.pop_back();
                    auto dot_pos = context.path_prefix.rfind('.');
                    if (dot_pos != std::string::npos) {
                        context.path_prefix.resize(dot_pos);
                    } else {
                        context.path_prefix.clear();
                    }
                }
                return true;
            }
            return false;
        }
    }  // namespace

    /**
     * @brief 获取JSON字典中的字符串值
     * @details 根据键获取JSON字典中的字符串值
     * @param key JSON键，格式为 "key1.key2"
     * @return std::optional<std::string> 字符串值，若键不存在则返回空可选
     */
    std::optional<std::string> json_dict_t::operator[](const std::string& key) const noexcept {
        return get_value(key);
    }

    /**
     * @brief 获取JSON字典中的字符串值
     * @details 根据键获取JSON字典中的字符串值
     * @param key JSON键，格式为 "key1.key2"
     * @return std::optional<std::string> 字符串值，若键不存在则返回空可选
     */
    std::optional<std::string> json_dict_t::get_value(const std::string& key) const noexcept {
        auto it = dict_.find(key);
        if (it != dict_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief 获取JSON字典中的字符串值
     * @details 根据键获取JSON字典中的字符串值，若键不存在则返回默认值
     * @param key JSON键，格式为 "key1.key2"
     * @param default_value 键不存在时返回的默认值
     * @return std::string 若键存在则返回对应值，否则返回 default_value
     */
    std::string json_dict_t::get_value_or(const std::string& key,
                                          const std::string& default_value) const noexcept {
        auto value = get_value(key);
        if (value.has_value()) {
            return std::move(*value);
        }
        return default_value;
    }

    /**
     * @brief 检查JSON字典是否包含指定键
     * @details 检查JSON字典是否包含指定键
     * @param key JSON键，格式为 "key1.key2"
     * @return bool 若包含则返回true，否则返回false
     */
    bool json_dict_t::contains(const std::string& key) const noexcept {
        if (empty()) {
            return false;
        }
        return get_value(key).has_value();
    }

    /**
     * @brief 检查JSON字典是否为空
     * @details 检查JSON字典是否为空
     * @return bool 若为空则返回true，否则返回false
     */
    bool json_dict_t::empty() const noexcept {
        return dict_.empty();
    }

    /**
     * @brief 获取JSON字典的大小
     * @details 获取JSON字典的大小，即键值对的数量
     * @return size_t JSON字典的大小
     */
    size_t json_dict_t::size() const noexcept {
        return dict_.size();
    }

    /**
     * @brief 从文件加载JSON字典
     * @details 从指定文件路径加载JSON字典，若文件不存在或解析失败则返回空可选
     * @param json_path JSON文件路径
     * @return std::optional<json_dict_t> JSON字典，若文件不存在或解析失败则返回空可选
     */
    std::optional<json_dict_t> json_dict_t::from_file(const std::filesystem::path& json_path) noexcept {
        auto content_opt = file_utils_t::read_file(json_path);

        if (!content_opt.has_value()) {
            return std::nullopt;
        }

        return parse(content_opt.value());
    }

    /**
     * @brief 解析JSON字符串
     * @details 解析JSON字符串为JSON字典，若解析失败则返回空可选
     * @param json_content JSON字符串内容
     * @return std::optional<json_dict_t> 解析后的JSON字典，若解析失败则返回空可选
     */
    std::optional<json_dict_t> json_dict_t::parse(const std::string& json_content) noexcept {
        if (json_content.empty()) {
            return std::nullopt;
        }

        try {
            detail::json_lexer_t lexer(json_content);
            detail::json_lexer_t::token_t token = lexer.next();
            if (token.type != detail::json_lexer_t::token_type_t::left_brace) {
                return std::nullopt;
            }
            parser_context_t context{};
            context.temp_dict.reserve(64);

            while (true) {
                token = lexer.next();
                if (token.type == detail::json_lexer_t::token_type_t::eof)
                    break;
                if (token.type == detail::json_lexer_t::token_type_t::invalid)
                    return std::nullopt;

                bool success = false;
                switch (context.state) {
                    case parse_state_t::expect_key_or_end:
                        success = handle_expect_key_or_end(context, token);
                        break;
                    case parse_state_t::expect_colon:
                        success = handle_expect_colon(context, token);
                        break;
                    case parse_state_t::expect_value_or_start:
                        success = handle_expect_value_or_start(context, token);
                        break;
                    case parse_state_t::expect_comma_or_end:
                        success = handle_expect_comma_or_end(context, token);
                        break;
                }

                if (!success) {
                    return std::nullopt;
                }
            }

            json_dict_t dict{};
            dict.dict_ = std::move(context.temp_dict);
            return dict;

        } catch (const std::bad_alloc&) {
            return std::nullopt;
        }
    }

    namespace {

        /**
         * @brief 将控制字符转义为 \u00XX 形式
         * @details 用于 escape_json 的 default 分支，处理 < 0x20 的控制字符。
         *          使用 std::to_chars 生成两位十六进制，不足两位前补 '0'。
         */
        void append_control_escape(std::string& result, unsigned char c) noexcept {
            result.append("\\u00");
            std::array<char, 2> buffer;
            auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), c, 16);
            if (ec != std::errc{}) {
                return;
            }
            const size_t written = static_cast<size_t>(ptr - buffer.data());
            if (written == 1) {
                result.push_back('0');
                result.push_back(buffer[0]);
            } else {
                result.append(buffer.data(), 2);
            }
        }

    }  // namespace

    /**
     * @brief 安全转义 JSON 字符串
     * @details 将包含控制字符的字符串转义为合法的 JSON 字符串格式
     * @param value 输入字符串视图
     * @return std::string 转义后的字符串
     */
    std::string json_serializer_t::escape_json(std::string_view value) noexcept {
        std::string result{};
        try {
            result.reserve(value.size() + 16);
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[json_utils] escape_json: reserve failed\n");
        }

        for (char c : value) {
            switch (c) {
                case '"':  result.append("\\\""); break;
                case '\\': result.append("\\\\"); break;
                case '\b': result.append("\\b");  break;
                case '\f': result.append("\\f");  break;
                case '\n': result.append("\\n");  break;
                case '\r': result.append("\\r");  break;
                case '\t': result.append("\\t");  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        append_control_escape(result, static_cast<unsigned char>(c));
                    } else {
                        result.push_back(c);
                    }
            }
        }
        return result;
    }
}  // namespace error_system::utils
