#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

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
        /**
         * @brief 二进制序列化魔数（"ESER"），用于标识本库产生的二进制流
         */
        static constexpr uint32_t BINARY_MAGIC = 0x52455345u;  // 'E','S','E','R' 小端

        /**
         * @brief 二进制序列化格式版本号，字段布局变更时递增
         */
        static constexpr uint8_t BINARY_VERSION = 1;

    private:
        /**
         * @brief JSON 字段解析器函数指针类型
         * @details 签名：(context, json, offset) -> bool（true=成功，false=格式错误）
         */
        using field_parser_t = bool (*)(error_context_t&, std::string_view, size_t&) noexcept;

        /**
         * @brief 递归实现 to_string，按深度缩进渲染 cause 链
         * @details 从 error_registry_t 获取元数据，根据 enable_text_output 配置决定
         *          输出子系统/模块名称或原始 ID。包含：源位置、错误等级、系统域、
         *          子系统/模块、错误编号、消息、描述、payload、堆栈和因果链。
         *          depth 用于控制 cause 链各层级的缩进。
         * @param context 错误上下文
         * @param depth 因果链递归深度（顶层为 0）
         * @return std::string 格式化的错误上下文字符串
         */
        static std::string to_string_impl_(const error_context_t& context, size_t depth) noexcept;

        /**
         * @brief 递归实现 to_json，按深度缩进渲染 cause 链
         * @details 生成包含 code、message、payload、stack_frames、cause 等字段的 JSON。
         *          cause 字段递归调用自身以渲染下一层错误上下文。
         * @param context 错误上下文
         * @param depth 因果链递归深度（顶层为 0）
         * @return std::string 错误上下文的 JSON 表示
         */
        static std::string to_json_impl_(const error_context_t& context, size_t depth) noexcept;

        /**
         * @brief 递归实现 to_binary，按深度序列化 cause 链
         * @details 使用小端序编码，适合高性能 RPC 或持久化存储。
         *          cause 链通过递归调用自身追加到二进制流末尾。
         * @param context 错误上下文
         * @param depth 因果链递归深度（顶层为 0）
         * @return std::string 错误上下文的二进制表示
         */
        static std::string to_binary_impl_(const error_context_t& context, size_t depth) noexcept;

        /**
         * @brief 从二进制数据反序列化（递归实现，不含魔数/版本头）
         * @details 从指定偏移开始解析单个 error_context_t 节点，递归处理 cause 链。
         *          成功时 offset 更新为已消费字节数。
         * @param data 二进制数据
         * @param offset 当前解析偏移（输入输出参数）
         * @return std::optional<error_context_t> 反序列化结果，失败返回 std::nullopt
         */
        static std::optional<error_context_t> from_binary_node_(
            std::string_view data, size_t& offset) noexcept;

        /**
         * @brief 解析二进制 location 字段到 context.loc_file_storage_ /
         *        loc_func_storage_ / file_name / source_location
         * @details 读取 has_location 标记，若为 1 则继续读取 file/func/line 三个子字段。
         *          仅当三个子字段全部解析成功时才写入 context。
         * @param context 目标上下文
         * @param data 二进制数据
         * @param offset 当前解析偏移（输入输出参数，将越过字段值）
         * @return bool true=成功，false=格式错误
         */
        static bool parse_binary_location_field_(error_context_t& context,
                                                  std::string_view data, size_t& offset) noexcept;

        /**
         * @brief 解析二进制 payload 字段到 context.insert_or_update_payload_
         * @details 读取 4 字节 payload 计数，限制 ≤ 100000，随后逐项读取 key/value
         * @param context 目标上下文
         * @param data 二进制数据
         * @param offset 当前解析偏移（输入输出参数，将越过字段值）
         * @return bool true=成功，false=格式错误
         */
        static bool parse_binary_payload_field_(error_context_t& context,
                                                 std::string_view data, size_t& offset) noexcept;

        /**
         * @brief 解析二进制 cause 字段到 context.cause
         * @details 读取 has_cause 标记，若为 1 则递归调用 from_binary_node_ 解析 cause 子节点
         * @param context 目标上下文
         * @param data 二进制数据
         * @param offset 当前解析偏移（输入输出参数，将越过字段值）
         * @return bool true=成功，false=格式错误
         */
        static bool parse_binary_cause_field_(error_context_t& context,
                                              std::string_view data, size_t& offset) noexcept;

        /**
         * @brief 从 JSON 字符串递归反序列化单个 error_context_t 节点
         * @details 流式解析（不构建中间 JSON 树），从指定偏移开始解析一个 JSON 对象，
         *          直接填充 error_context_t 私有字段，递归处理 cause 链。
         *          成功时 offset 更新为已消费字节数。
         * @param json JSON 文本
         * @param offset 当前解析偏移（输入输出参数）
         * @return std::optional<error_context_t> 反序列化结果，失败返回 std::nullopt
         */
        static std::optional<error_context_t> from_json_node_(
            std::string_view json, size_t& offset) noexcept;

        /**
         * @brief 解析 JSON "code" 字段到 context.code_ / context.metadata_
         * @details 解析字符串形式的错误码，并从注册表补齐元数据
         * @param context 目标上下文
         * @param json JSON 文本
         * @param offset 当前解析偏移（输入输出参数，将越过字段值）
         * @return bool true=成功，false=格式错误
         */
        static bool parse_json_code_field_(error_context_t& context,
                                           std::string_view json, size_t& offset) noexcept;

        /**
         * @brief 解析 JSON "message" 字段到 context.message
         * @param context 目标上下文
         * @param json JSON 文本
         * @param offset 当前解析偏移（输入输出参数，将越过字段值）
         * @return bool true=成功，false=格式错误
         */
        static bool parse_json_message_field_(error_context_t& context,
                                              std::string_view json, size_t& offset) noexcept;

        /**
         * @brief 解析 JSON "location" 字段到 context.loc_file_storage_ /
         *        loc_func_storage_ / file_name / source_location
         * @details 仅当 file/function/line 三个子字段全部解析成功时才写入 context
         * @param context 目标上下文
         * @param json JSON 文本
         * @param offset 当前解析偏移（输入输出参数，将越过字段值）
         * @return bool true=成功，false=格式错误
         */
        static bool parse_json_location_field_(error_context_t& context,
                                               std::string_view json, size_t& offset) noexcept;

        /**
         * @brief 解析 JSON "payload" 字段到 context.insert_or_update_payload_
         * @details 限制 payload 项数 ≤ 100000，超过返回 false
         * @param context 目标上下文
         * @param json JSON 文本
         * @param offset 当前解析偏移（输入输出参数，将越过字段值）
         * @return bool true=成功，false=格式错误
         */
        static bool parse_json_payload_field_(error_context_t& context,
                                              std::string_view json, size_t& offset) noexcept;

        /**
         * @brief 解析 JSON "stack_frames" 字段到 context.stack_frames
         * @details 当 STACKTRACE_ENABLED 关闭时跳过该字段值
         * @param context 目标上下文
         * @param json JSON 文本
         * @param offset 当前解析偏移（输入输出参数，将越过字段值）
         * @return bool true=成功，false=格式错误
         */
        static bool parse_json_stack_frames_field_(error_context_t& context,
                                                   std::string_view json, size_t& offset) noexcept;

        /**
         * @brief 解析 JSON "cause" 字段到 context.cause
         * @details 递归调用 from_json_node_ 解析 cause 子对象
         * @param context 目标上下文
         * @param json JSON 文本
         * @param offset 当前解析偏移（输入输出参数，将越过字段值）
         * @return bool true=成功，false=格式错误
         */
        static bool parse_json_cause_field_(error_context_t& context,
                                            std::string_view json, size_t& offset) noexcept;

        /**
         * @brief JSON 顶层字段分发表（字段名 → 解析器函数指针）
         * @details 用于 from_json_node_ 的字段分发，替代 if-else 链。
         *          使用静态 const 哈希表，构造一次后只读访问，线程安全。
         *          未注册字段由调用方通过 skip_json_value 跳过。
         */
        static const std::unordered_map<std::string_view,
            bool (*)(error_context_t&, std::string_view, size_t&) noexcept>&
        field_dispatcher_table_() noexcept;

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
         * @param context 错误上下文
         * @return std::string 格式化的错误上下文字符串
         */
        static std::string to_string(const error_context_t& context) noexcept;

        /**
         * @brief 转换为 JSON 字符串
         * @details 生成包含 code、message、payload、stack_frames、cause 等字段的 JSON
         * @param context 错误上下文
         * @return std::string 错误上下文的 JSON 表示
         */
        static std::string to_json(const error_context_t& context) noexcept;

        /**
         * @brief 转换为紧凑二进制字符串
         * @details 使用小端序编码，适合高性能 RPC 或持久化存储
         * @param context 错误上下文
         * @return std::string 错误上下文的二进制表示
         */
        static std::string to_binary(const error_context_t& context) noexcept;

        /**
         * @brief 从二进制数据反序列化错误上下文
         * @details 解析 to_binary 产生的二进制流，校验魔数与版本号，
         *          还原 code/message/location/payload/cause 链。
         *          反序列化的文件名与函数名由 error_context_t 内部字符串存储持有，
         *          保证 file_name 与 source_location 中 const char* 的生命周期安全。
         *          任何格式错误或分配失败均返回 std::nullopt，不抛异常。
         * @param data 二进制数据视图
         * @return std::optional<error_context_t> 反序列化结果，失败返回 std::nullopt
         */
        [[nodiscard]] static std::optional<error_context_t> from_binary(std::string_view data) noexcept;

        /**
         * @brief 从 JSON 字符串反序列化错误上下文
         * @details 解析 to_json 产生的 JSON，还原 code/message/location/payload/
         *          stack_frames/cause 链。code 字段接受字符串形式（与 to_json 自洽）。
         *          任何格式错误或分配失败均返回 std::nullopt，不抛异常。
         * @param json JSON 字符串视图
         * @return std::optional<error_context_t> 反序列化结果，失败返回 std::nullopt
         */
        [[nodiscard]] static std::optional<error_context_t> from_json(std::string_view json) noexcept;
    };

}  // namespace error_system::core
