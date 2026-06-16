#include "error_system/utils/json_lexer.h"

namespace error_system::utils::detail {

    /**
     * @brief 跳过JSON字符串中的空格字符
     * @details 跳过JSON字符串中的空格字符，直到遇到非空格字符
     */
    void json_lexer_t::__skip_whitespace() noexcept {
        while (pos_ < json_str_.size() && std::isspace(static_cast<unsigned char>(json_str_[pos_]))) {
            ++pos_;
        }
    }

    /**
     * @brief 解析JSON字符串中的字符串token
     * @details 解析JSON字符串中的字符串token，直到遇到非字符串字符
     */
    json_lexer_t::token_t json_lexer_t::__parse_string() noexcept {
        std::string str{};
        try {
            str.reserve(32);
        } catch (...) {
            std::fprintf(stderr, "[json_lexer] __parse_string: reserve failed\n");
        }
        ++pos_;

        while (pos_ < json_str_.size() && json_str_[pos_] != '"') {
            if (json_str_[pos_] == '\\' && pos_ + 1 < json_str_.size()) {
                ++pos_;
                if (json_str_[pos_] == 'n') {
                    str += '\n';
                } else if (json_str_[pos_] == 't') {
                    str += '\t';
                } else if (json_str_[pos_] == 'r') {
                    str += '\r';
                } else if (json_str_[pos_] == 'b') {
                    str += '\b';
                } else if (json_str_[pos_] == 'f') {
                    str += '\f';
                } else if (json_str_[pos_] == 'u' && pos_ + 4 < json_str_.size()) {
                    uint32_t codepoint = 0;
                    for (int i = 1; i <= 4; ++i) {
                        char hex = json_str_[pos_ + i];
                        codepoint <<= 4;
                        if (hex >= '0' && hex <= '9') {
                            codepoint |= hex - '0';
                        } else if (hex >= 'a' && hex <= 'f') {
                            codepoint |= hex - 'a' + 10;
                        } else if (hex >= 'A' && hex <= 'F') {
                            codepoint |= hex - 'A' + 10;
                        } else {
                            return {token_type_t::invalid, ""};
                        }
                    }
                    pos_ += 4;
                    if (codepoint <= 0x7F) {
                        str += static_cast<char>(codepoint);
                    } else if (codepoint <= 0x7FF) {
                        str += static_cast<char>(0xC0 | (codepoint >> 6));
                        str += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0xFFFF) {
                        str += static_cast<char>(0xE0 | (codepoint >> 12));
                        str += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        str += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0x10FFFF) {
                        str += static_cast<char>(0xF0 | (codepoint >> 18));
                        str += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        str += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        str += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                } else {
                    str += json_str_[pos_];
                }
            } else {
                str += json_str_[pos_];
            }
            ++pos_;
        }

        if (pos_ < json_str_.size() && json_str_[pos_] == '"') {
            ++pos_;
            return {token_type_t::string, str};
        }
        return {token_type_t::invalid, ""};
    }

    /**
     * @brief 构造函数
     * @details 构造函数，初始化JSON字符串
     * @param json_str JSON字符串视图
     */
    json_lexer_t::json_lexer_t(std::string_view json_str) noexcept : json_str_(json_str), pos_(0) {}

    /**
     * @brief 获取下一个token
     * @details 获取下一个token，直到遇到文件结束标识
     * @return token_t 下一个token
     */
    json_lexer_t::token_t json_lexer_t::next() noexcept {
        __skip_whitespace();
        if (pos_ >= json_str_.size()) {
            return {token_type_t::eof, ""};
        }

        char c = json_str_[pos_];
        if (c == '{') {
            ++pos_;
            return {token_type_t::left_brace, "{"};
        }
        if (c == '}') {
            ++pos_;
            return {token_type_t::right_brace, "}"};
        }
        if (c == ':') {
            ++pos_;
            return {token_type_t::colon, ":"};
        }
        if (c == ',') {
            ++pos_;
            return {token_type_t::comma, ","};
        }

        if (c == '"') {
            return __parse_string();
        }

        return {token_type_t::invalid, {}};
    }

}  // namespace error_system::utils::detail
