#pragma once
#include <cstddef>

#include <string>
#include <string_view>

/**
 * @file json_lexer.h
 * @brief JSON词法分析器
 * @details 定义JSON词法分析器相关的操作，如解析JSON字符串为JSON字典
 * @author yiice
 * @version 2.3.0
 * @date 2026-04-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils::detail {

    /**
     * @brief JSON词法分析器
     * @details 定义JSON词法分析器相关的操作，如解析JSON字符串为JSON字典
     */
    class json_lexer_t {
    public:
        /**
     * @brief JSON词法分析器的token类型枚举
     * @details
     * 定义JSON词法分析器的token类型枚举，包括字符串、数字、关键字、
     * 冒号、逗号、左右大括号、左右中括号、文件结束标识和无效字符或错误。
     */
    enum class token_type_t {
        string,        // 字符串 (键或值)
        number,        // 数字字面量
        true_literal,  // true
        false_literal, // false
        null_literal,  // null
        colon,         // 冒号 :
        comma,         // 逗号 ,
        left_brace,    // 左大括号 {
        right_brace,   // 右大括号 }
        left_bracket,  // 左中括号 [
        right_bracket, // 右中括号 ]
        eof,           // 文件结束标识
        invalid        // 无效字符或错误
    };

        /**
         * @brief JSON词法分析器的token结构体
         * @details 定义JSON词法分析器的token结构体，包含token类型和token值
         */
        struct token_t {
            token_type_t type{token_type_t::eof};   ///< token 类型
            std::string value;                      ///< 保存解析后的字符串内容
        };

    private:
        std::string_view json_str_{};

        size_t pos_{0};
        /**
         * @brief 跳过JSON字符串中的空格字符
         * @details 跳过JSON字符串中的空格字符，直到遇到非空格字符
         */
        void skip_whitespace_() noexcept;

        /**
         * @brief 解析JSON字符串中的字符串token
         * @details 解析JSON字符串中的字符串token，直到遇到非字符串字符
         */
        token_t parse_string_() noexcept;

        /**
         * @brief 解析JSON数字字面量token
         * @details 解析JSON数字字面量（RFC 8259 §6），包括可选负号、整数部分、
         *          可选小数部分、可选指数部分。token 的 value 保存原始数字字符串。
         */
        token_t parse_number_() noexcept;

        /**
         * @brief 解析JSON关键字字面量token
         * @details 解析 true/false/null 关键字，token 的 value 保存关键字字符串。
         * @param keyword 期望的关键字字符串
         * @param type 对应的关键字 token 类型
         */
        token_t parse_keyword_(std::string_view keyword, token_type_t type) noexcept;

    public:
        /**
         * @brief 构造函数
         * @details 构造函数，初始化JSON字符串
         * @param json_text JSON字符串视图
         */
        explicit json_lexer_t(std::string_view json_text) noexcept;

        ~json_lexer_t() noexcept = default;
        json_lexer_t(const json_lexer_t&) = default;
        json_lexer_t& operator=(const json_lexer_t&) = default;
        json_lexer_t(json_lexer_t&&) noexcept = default;
        json_lexer_t& operator=(json_lexer_t&&) noexcept = default;

        /**
         * @brief 获取下一个token
         * @details 获取下一个token，直到遇到文件结束标识
         * @return token_t 下一个token
         */
        [[nodiscard]] token_t next() noexcept;
    };

}  // namespace error_system::utils::detail