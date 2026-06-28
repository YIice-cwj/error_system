#include "error_system/utils/json_lexer.h"

/**
 * @file json_lexer.cc
 * @brief JSON 词法分析器实现
 * @details 实现 JSON 字符串的词法分析，支持字符串（含 \uXXXX 转义与 UTF-16 代理对）、
 *          标点符号（{}[],:）、空白跳过等 token 识别，遵循 RFC 8259 §7。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */

#include <cstdint>
#include <optional>

namespace error_system::utils::detail {

    namespace {
        /**
         * @brief 将 Unicode 码点编码为 UTF-8 并追加到目标字符串
         * @details 根据 RFC 3629 将码点 [0, 0x10FFFF] 编码为 1~4 字节 UTF-8。
         *          非法码点（>0x10FFFF 或落在 0xD800~0xDFFF 代理区）被静默丢弃。
         * @param output 输出字符串
         * @param codepoint 待编码的 Unicode 码点
         */
        void append_utf8(std::string& output, uint32_t codepoint) noexcept {
            if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
                return;
            }
            if (codepoint <= 0x7F) {
                output.push_back(static_cast<char>(codepoint));
            } else if (codepoint <= 0x7FF) {
                output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else if (codepoint <= 0xFFFF) {
                output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else {
                output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        }

        /**
         * @brief 从 json_text[position+1..position+4] 解析 4 位十六进制 \u 转义
         * @details 调用方需保证 position+4 < json_text.size()。成功时返回码点并前移 position；
         *          失败（非法十六进制）返回 std::nullopt。
         * @param json_text 源 JSON 串
         * @param position 指向 'u' 的位置，函数返回时 position 指向最后一个十六进制位
         * @return std::optional<uint32_t> 解析出的码点；失败返回空
         */
        std::optional<uint32_t> parse_hex4(std::string_view json_text, size_t& position) noexcept {
            uint32_t codepoint = 0;
            for (int i = 1; i <= 4; ++i) {
                const char hex = json_text[position + static_cast<size_t>(i)];
                codepoint <<= 4;
                if (hex >= '0' && hex <= '9') {
                    codepoint |= static_cast<uint32_t>(hex - '0');
                } else if (hex >= 'a' && hex <= 'f') {
                    codepoint |= static_cast<uint32_t>(hex - 'a' + 10);
                } else if (hex >= 'A' && hex <= 'F') {
                    codepoint |= static_cast<uint32_t>(hex - 'A' + 10);
                } else {
                    return std::nullopt;
                }
            }
            position += 4;
            return codepoint;
        }
    }  // namespace

    /**
     * @brief 跳过JSON字符串中的空格字符
     * @details 跳过JSON字符串中的空格字符，直到遇到非空格字符
     */
    void json_lexer_t::skip_whitespace_() noexcept {
        while (pos_ < json_str_.size() && std::isspace(static_cast<unsigned char>(json_str_[pos_]))) {
            ++pos_;
        }
    }

    /**
     * @brief 解析JSON字符串中的字符串token
     * @details 解析JSON字符串中的字符串token，直到遇到非字符串字符。
     *          支持 \uXXXX 转义，并正确处理 UTF-16 代理对（RFC 8259 §7）：
     *          高代理（0xD800~0xDBFF）后必须紧跟 \uXXXX 低代理（0xDC00~0xDFFF），
     *          二者组合为 0x10000~0x10FFFF 范围内的码点，再编码为 UTF-8。
     *          孤立的代理码点按非法码点处理（不输出）。
     */

    namespace {

        /**
         * @brief 尝试合并 UTF-16 代理对
         * @details 若 codepoint 为高代理（0xD800~0xDBFF）且其后紧跟 \uXXXX 低代理（0xDC00~0xDFFF），
         *          则合并为 0x10000~0x10FFFF 范围内的码点，并推进 pos 越过低代理。
         *          孤立高代理或非低代理跟随时返回原 codepoint 不变。
         * @param text JSON 文本
         * @param pos 当前位置（指向高代理的 \u 后第一个十六进制位之后），成功时推进到低代理之后
         * @param codepoint 输入高代理码点，输出合并后的码点（若合并成功）
         * @return bool true=已合并代理对，false=未合并（codepoint 不变）
         */
        bool try_merge_surrogate_pair(std::string_view text, size_t& pos, uint32_t& codepoint) noexcept {
            if (codepoint < 0xD800 || codepoint > 0xDBFF) {
                return false;
            }
            if (pos + 6 >= text.size() || text[pos + 1] != '\\' || text[pos + 2] != 'u') {
                return false;
            }
            size_t low_pos = pos + 2;
            auto low_opt = parse_hex4(text, low_pos);
            if (!low_opt || *low_opt < 0xDC00 || *low_opt > 0xDFFF) {
                return false;
            }
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (*low_opt - 0xDC00);
            pos = low_pos;
            return true;
        }

    }  // namespace

    json_lexer_t::token_t json_lexer_t::parse_string_() noexcept {
        std::string str{};
        try {
            str.reserve(32);
        } catch (...) {
            std::fprintf(stderr, "[json_lexer] parse_string_: reserve failed\n");
        }
        ++pos_;

        while (pos_ < json_str_.size() && json_str_[pos_] != '"') {
            if (json_str_[pos_] != '\\' || pos_ + 1 >= json_str_.size()) {
                str += json_str_[pos_];
                ++pos_;
                continue;
            }
            ++pos_;
            switch (json_str_[pos_]) {
                case 'n':  str += '\n'; break;
                case 't':  str += '\t'; break;
                case 'r':  str += '\r'; break;
                case 'b':  str += '\b'; break;
                case 'f':  str += '\f'; break;
                case 'u': {
                    if (pos_ + 4 >= json_str_.size()) {
                        return {token_type_t::invalid, ""};
                    }
                    auto hex_opt = parse_hex4(json_str_, pos_);
                    if (!hex_opt) {
                        return {token_type_t::invalid, ""};
                    }
                    uint32_t codepoint = *hex_opt;
                    try_merge_surrogate_pair(json_str_, pos_, codepoint);
                    append_utf8(str, codepoint);
                    break;
                }
                default:
                    str += json_str_[pos_];
                    break;
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
     * @param json_text JSON字符串视图
     */
    json_lexer_t::json_lexer_t(std::string_view json_text) noexcept : json_str_(json_text), pos_(0) {}

    /**
     * @brief 获取下一个token
     * @details 获取下一个token，直到遇到文件结束标识
     * @return token_t 下一个token
     */
    json_lexer_t::token_t json_lexer_t::next() noexcept {
        skip_whitespace_();
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
            return parse_string_();
        }

        return {token_type_t::invalid, {}};
    }

}  // namespace error_system::utils::detail
