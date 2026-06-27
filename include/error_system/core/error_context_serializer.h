#pragma once
#include <string>

#include "error_system/core/error_context.h"

/**
 * @file error_context_serializer.h
 * @brief 错误上下文序列化器
 * @details 从 error_context_t 拆分而来，单一职责：将 error_context_t 转换为
 *          人类可读文本、JSON 字符串和紧凑二进制表示。所有方法均为静态方法，
 *          接受 const error_context_t& 参数，不持有任何状态。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误上下文序列化器
     * @details 提供 error_context_t 的文本/JSON/二进制序列化能力。
     *          类仅包含静态成员，禁止实例化。通过 error_context_t 的 friend 声明
     *          访问其私有成员 code_ 与 metadata_。
     */
    class error_context_serializer_t {
    public:
        error_context_serializer_t() = delete;
        ~error_context_serializer_t() = delete;
        error_context_serializer_t(const error_context_serializer_t&) = delete;
        error_context_serializer_t& operator=(const error_context_serializer_t&) = delete;
        error_context_serializer_t(error_context_serializer_t&&) = delete;
        error_context_serializer_t& operator=(error_context_serializer_t&&) = delete;

        /**
         * @brief 转换为人类可读字符串
         * @details 从 error_registry_t 获取元数据，根据 enable_text_output 配置决定
         *          输出子系统/模块名称或原始 ID。包含：源位置、错误等级、系统域、
         *          子系统/模块、错误编号、消息、描述、payload、堆栈和因果链
         * @param ctx 错误上下文
         * @return std::string 格式化的错误上下文字符串
         */
        static std::string to_string(const error_context_t& ctx) noexcept;

        /**
         * @brief 转换为 JSON 字符串
         * @details 生成包含 code、message、payload、stack_frames、cause 等字段的 JSON
         * @param ctx 错误上下文
         * @return std::string 错误上下文的 JSON 表示
         */
        static std::string to_json(const error_context_t& ctx) noexcept;

        /**
         * @brief 转换为紧凑二进制字符串
         * @details 使用小端序编码，适合高性能 RPC 或持久化存储
         * @param ctx 错误上下文
         * @return std::string 错误上下文的二进制表示
         */
        static std::string to_binary(const error_context_t& ctx) noexcept;

    private:
        static std::string to_string_impl_(const error_context_t& ctx, size_t depth) noexcept;
        static std::string to_json_impl_(const error_context_t& ctx, size_t depth) noexcept;
        static std::string to_binary_impl_(const error_context_t& ctx, size_t depth) noexcept;
    };

}  // namespace error_system::core
