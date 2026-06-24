#include "error_system/utils/json_utils.h"

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
            expect_key_or_end,      // 期待键名（字符串）或 结尾 '}'
            expect_colon,           // 期待冒号 ':'
            expect_value_or_start,  // 期待值（字符串）或 嵌套起点 '{'
            expect_comma_or_end     // 期待逗号 ',' 或 结尾 '}'
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
                } catch (...) {
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
     * @details 根据键获取JSON字典中的字符串值
     * @param key JSON键，格式为 "key1.key2"
     * @return std::optional<std::string> 字符串值，若键不存在则返回空可选
     */
    std::optional<std::string> json_dict_t::get_value_or(const std::string& key,
                                                         const std::string& default_value) const noexcept {
        auto value = get_value(key);
        if (value.has_value()) {
            return value;
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
        } catch (...) {
            std::fprintf(stderr, "[json_utils] parse exception caught and ignored\n");
            return std::nullopt;
        }
    }
}  // namespace error_system::utils
