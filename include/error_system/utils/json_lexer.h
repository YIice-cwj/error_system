#pragma once
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
         * 定义JSON词法分析器的token类型枚举，包括字符串、冒号、逗号、左大括号、右大括号、文件结束标识和无效字符或错误
         */
        enum class token_type_t {
            string,       // 字符串 (键或值)
            colon,        // 冒号 :
            comma,        // 逗号 ,
            left_brace,   // 左大括号 {
            right_brace,  // 右大括号 }
            eof,          // 文件结束标识
            invalid       // 无效字符或错误
        };

        /**
         * @brief JSON词法分析器的token结构体
         * @details 定义JSON词法分析器的token结构体，包含token类型和token值
         */
        struct token_t {
            token_type_t type;  // token类型
            std::string value;  // 保存解析后的字符串内容
        };

        private:
        std::string_view json_str_{};

        size_t pos_{0};
        /**
         * @brief 跳过JSON字符串中的空格字符
         * @details 跳过JSON字符串中的空格字符，直到遇到非空格字符
         */
        void __skip_whitespace() noexcept;

        /**
         * @brief 解析JSON字符串中的字符串token
         * @details 解析JSON字符串中的字符串token，直到遇到非字符串字符
         */
        token_t __parse_string() noexcept;

        public:
        /**
         * @brief 构造函数
         * @details 构造函数，初始化JSON字符串
         * @param json_str JSON字符串视图
         */
        explicit json_lexer_t(std::string_view json_str) noexcept;

        /**
         * @brief 获取下一个token
         * @details 获取下一个token，直到遇到文件结束标识
         * @return token_t 下一个token
         */
        token_t next() noexcept;
    };

}  // namespace error_system::utils::detail