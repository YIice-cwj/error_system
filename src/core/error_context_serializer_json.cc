#include "error_system/core/error_context_serializer.h"
#include "error_context_serializer_internal.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"
#include "error_system/utils/json_lexer.h"
#include "error_system/utils/json_utils.h"

using error_system::config::feature_flags_t;
using error_system::core::detail::append_decimal;
using json_lexer_t = error_system::utils::detail::json_lexer_t;

/**
 * @file error_context_serializer_json.cc
 * @brief 错误上下文序列化器 - JSON 格式实现
 * @details 实现 error_context_serializer_t 的 JSON 序列化（to_json / to_json_impl_）
 *          与反序列化（from_json / from_json_node_）。
 *          从 error_context_serializer.cc 拆分而来，仅包含 JSON 格式相关的辅助函数与逻辑。
 *          反序列化复用 utils::detail::json_lexer_t 进行 token 化解析，避免重复实现
 *          JSON 字符串/数字/关键字/跳过逻辑。
 * @author yiice
 * @version 1.1.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {
    namespace {

        /**
         * @brief 追加转义后的 JSON 字符串字面量（含首尾引号）
         */
        void append_escaped_json_string(std::string& output, std::string_view value) noexcept {
            try {
                output.push_back('"');
                output.append(utils::json_serializer_t::escape_json(std::string(value)));
                output.push_back('"');
            } catch (const std::bad_alloc&) {
                std::fprintf(stderr, "[error_context_serializer] append_escaped_json_string: std::bad_alloc\n");
            }
        }

        /**
         * @brief 估算 JSON 序列化结果字符串容量
         */
        size_t estimate_json_capacity(const error_context_t& context) noexcept {
            size_t capacity = 64 + context.message.size();

            context.for_each_payload([&](const std::string& key, const std::string& value) {
                capacity += key.size() + value.size() + 8;
            });

            if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
                for (const auto& frame : context.stack_frames) {
                    capacity += frame.size() + 4;
                }
            }

            if (context.cause) {
                capacity += 16 + context.cause->message.size();
            }
            return capacity;
        }

        /**
         * @brief 追加 location 字段（若启用且存在）
         * @return true 表示已写入字段（影响后续字段分隔符）
         */
        bool append_location_json(std::string& json, const error_context_t& context,
                                  bool& first_field) noexcept {
            if constexpr (feature_flags_t::LOCATION_ENABLED) {
                if (feature_flags_t::is_source_location_enabled() && context.file_name != nullptr) {
                    if (!first_field) {
                        json.push_back(',');
                    }
                    first_field = false;
                    json.append("\"location\":{");
                    json.append("\"file\":");
                    append_escaped_json_string(json, context.file_name);
                    json.append(",\"function\":");
                    append_escaped_json_string(json, context.source_location.function_name());
                    json.append(",\"line\":");
                    append_decimal(json, context.source_location.line());
                    json.push_back('}');
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief 追加 payload 字段（"payload":{"k":"v",...}）
         */
        void append_payload_json(std::string& json, const error_context_t& context) noexcept {
            if (context.payload_size() == 0) {
                return;
            }
            json.append(",\"payload\":{");
            bool first_payload = true;
            context.for_each_payload([&](const std::string& key, const std::string& value) {
                if (!first_payload) {
                    json.push_back(',');
                }
                append_escaped_json_string(json, key);
                json.push_back(':');
                append_escaped_json_string(json, value);
                first_payload = false;
            });
            json.push_back('}');
        }

        /**
         * @brief 追加 stack_frames 字段（"stack_frames":["frame1","frame2"]）
         */
        void append_stacktrace_json(std::string& json, const error_context_t& context) noexcept {
            if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
                if (context.stack_frames.empty()) {
                    return;
                }
                json.append(",\"stack_frames\":[");
                bool first_frame = true;
                for (const auto& frame : context.stack_frames) {
                    if (!first_frame) {
                        json.push_back(',');
                    }
                    append_escaped_json_string(json, frame);
                    first_frame = false;
                }
                json.push_back(']');
            }
        }

        /**
         * @brief 将十进制字符串解析为 uint64_t（用于 JSON 中字符串形式的错误码）
         */
        bool parse_uint64(const std::string& text, uint64_t& output) noexcept {
            if (text.empty()) {
                return false;
            }
            try {
                size_t parsed_index = 0;
                const unsigned long long value = std::stoull(text, &parsed_index);
                if (parsed_index != text.size()) {
                    return false;
                }
                output = static_cast<uint64_t>(value);
                return true;
            } catch (const std::invalid_argument&) {
                return false;
            } catch (const std::out_of_range&) {
                return false;
            }
        }

        /**
         * @brief 将 JSON 数字字符串解析为 double
         * @details 复用 lexer 已切分的数字字符串，调用 std::stod 转换。
         *          失败（空串或异常）返回 false。
         * @param text 数字字符串
         * @param output 输出参数
         * @return bool true=成功，false=失败
         */
        bool parse_json_number_value(const std::string& text, double& output) noexcept {
            if (text.empty()) {
                return false;
            }
            try {
                size_t parsed_index = 0;
                const double value = std::stod(text, &parsed_index);
                if (parsed_index != text.size()) {
                    return false;
                }
                output = value;
                return true;
            } catch (const std::invalid_argument&) {
                return false;
            } catch (const std::out_of_range&) {
                return false;
            }
        }

        /**
         * @brief 跳过 JSON 对象（{...}）
         * @details 调用前 lexer 已消费起始 '{'。键必须是字符串，值递归调用 skip_json_value_。
         *          成功时 lexer 已消费结束 '}'。
         * @return bool true=成功，false=格式错误
         */
        bool skip_json_object_(json_lexer_t& lexer) noexcept;

        /**
         * @brief 跳过 JSON 数组（[...]）
         * @details 调用前 lexer 已消费起始 '['。元素递归调用 skip_value_with_token_。
         *          成功时 lexer 已消费结束 ']'。
         * @return bool true=成功，false=格式错误
         */
        bool skip_json_array_(json_lexer_t& lexer) noexcept;

        /**
         * @brief 使用已消费的首 token 跳过任意 JSON 值
         * @details 根据 token 类型分发到对象、数组或标量处理。
         * @param lexer JSON 词法分析器
         * @param token 已消费的首个 token
         * @return bool true=成功，false=格式错误
         */
        bool skip_value_with_token_(json_lexer_t& lexer, const json_lexer_t::token_t& token) noexcept {
            using token_type_t = json_lexer_t::token_type_t;
            switch (token.type) {
                case token_type_t::left_brace:
                    return skip_json_object_(lexer);
                case token_type_t::left_bracket:
                    return skip_json_array_(lexer);
                case token_type_t::string:
                case token_type_t::number:
                case token_type_t::true_literal:
                case token_type_t::false_literal:
                case token_type_t::null_literal:
                    return true;
                default:
                    return false;
            }
        }

        /**
         * @brief 跳过任意 JSON 值（用于忽略未知字段）
         * @details 递归跳过完整的 JSON 值：对象 {}、数组 []、字符串、数字、true/false/null。
         *          成功时 lexer 已消费整个值；失败返回 false。
         * @param lexer JSON 词法分析器
         * @return bool true=成功，false=格式错误
         */
        bool skip_json_value_(json_lexer_t& lexer) noexcept {
            return skip_value_with_token_(lexer, lexer.next());
        }

        bool skip_json_object_(json_lexer_t& lexer) noexcept {
            using token_type_t = json_lexer_t::token_type_t;
            while (true) {
                auto token = lexer.next();
                if (token.type == token_type_t::right_brace) {
                    return true;
                }
                if (token.type != token_type_t::string) {
                    return false;
                }
                token = lexer.next();
                if (token.type != token_type_t::colon) {
                    return false;
                }
                if (!skip_json_value_(lexer)) {
                    return false;
                }
                token = lexer.next();
                if (token.type == token_type_t::right_brace) {
                    return true;
                }
                if (token.type != token_type_t::comma) {
                    return false;
                }
            }
        }

        bool skip_json_array_(json_lexer_t& lexer) noexcept {
            using token_type_t = json_lexer_t::token_type_t;
            while (true) {
                auto token = lexer.next();
                if (token.type == token_type_t::right_bracket) {
                    return true;
                }
                if (!skip_value_with_token_(lexer, token)) {
                    return false;
                }
                token = lexer.next();
                if (token.type == token_type_t::right_bracket) {
                    return true;
                }
                if (token.type != token_type_t::comma) {
                    return false;
                }
            }
        }

        /**
         * @brief JSON location 对象解析状态
         * @details 聚合 file/function/line 三个字段的解析结果与识别标志，
         *          将 parse_location_member_ 的参数数量收敛到规范建议的 4 个以内。
         */
        struct location_parse_state_t {
            std::string file{};          ///< 文件名
            std::string function{};      ///< 函数名
            uint32_t line{0};            ///< 行号
            bool got_file{false};        ///< 是否已识别 file 字段
            bool got_function{false};    ///< 是否已识别 function 字段
            bool got_line{false};        ///< 是否已识别 line 字段
        };

        /**
         * @brief 解析 JSON location 对象的单个字段
         * @details 用于 parse_json_location_field_ 的字段分发，替代 if-else 链。
         *          返回 true 表示已消费字段值（不论是否识别）；false 表示格式错误。
         * @param lexer JSON 词法分析器
         * @param key 字段名
         * @param state 解析状态（输入输出）
         * @return bool true=成功，false=格式错误
         */
        bool parse_location_member_(json_lexer_t& lexer, const std::string& key,
                                    location_parse_state_t& state) noexcept {
            using token_type_t = json_lexer_t::token_type_t;
            if (key == "file") {
                auto token = lexer.next();
                if (token.type != token_type_t::string) { return false; }
                state.file = std::move(token.value);
                state.got_file = true;
                return true;
            }
            if (key == "function") {
                auto token = lexer.next();
                if (token.type != token_type_t::string) { return false; }
                state.function = std::move(token.value);
                state.got_function = true;
                return true;
            }
            if (key == "line") {
                auto token = lexer.next();
                if (token.type != token_type_t::number) { return false; }
                double d = 0;
                if (!parse_json_number_value(token.value, d) || d < 0) {
                    return false;
                }
                state.line = static_cast<uint32_t>(d);
                state.got_line = true;
                return true;
            }
            return skip_json_value_(lexer);
        }

        /**
         * @brief 解析 location 对象体（{file,...,function,...,line:...}）
         * @details 循环读取对象成员并委托 parse_location_member_，直到遇到右大括号。
         *          空对象返回 false（location 字段要求至少包含一个成员语义上不完整）。
         * @param lexer JSON 词法分析器（已消费左大括号）
         * @param state 解析状态（输出）
         * @return bool true=成功，false=格式错误
         */
        bool parse_location_object_body_(json_lexer_t& lexer, location_parse_state_t& state) noexcept {
            using token_type_t = json_lexer_t::token_type_t;
            auto token = lexer.next();
            if (token.type == token_type_t::right_brace) {
                return false;
            }
            while (true) {
                if (token.type != token_type_t::string) {
                    return false;
                }
                auto key = std::move(token.value);

                token = lexer.next();
                if (token.type != token_type_t::colon) {
                    return false;
                }

                if (!parse_location_member_(lexer, key, state)) {
                    return false;
                }

                token = lexer.next();
                if (token.type == token_type_t::right_brace) {
                    break;
                }
                if (token.type != token_type_t::comma) {
                    return false;
                }
                token = lexer.next();
            }
            return true;
        }

    }  // namespace

    std::string error_context_serializer_t::to_json(const error_context_t& context) noexcept {
        return to_json_impl_(context, 0);
    }

    std::string error_context_serializer_t::to_json_impl_(const error_context_t& context, size_t depth) noexcept {
        std::string json;
        try {
            json.reserve(estimate_json_capacity(context));
            json.push_back('{');

            bool first_field = true;
            append_location_json(json, context, first_field);

            auto append_separator = [&json, &first_field]() {
                if (!first_field) {
                    json.push_back(',');
                }
                first_field = false;
            };

            append_separator();
            json.append("\"code\":");
            append_escaped_json_string(json, std::to_string(context.code_.get_code()));
            json.append(",\"message\":");
            append_escaped_json_string(json, context.message);
            append_payload_json(json, context);
            append_stacktrace_json(json, context);

            if (context.cause && depth < MAX_CAUSE_DEPTH) {
                json.append(",\"cause\":").append(to_json_impl_(*context.cause, depth + 1));
            }

            json.push_back('}');
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_context_serializer] to_json: std::bad_alloc\n");
        }
        return json;
    }

    std::optional<error_context_t> error_context_serializer_t::from_json(std::string_view json) noexcept {
        if (json.empty()) {
            return std::nullopt;
        }
        utils::detail::json_lexer_t lexer(json);
        auto result = from_json_node_(lexer);
        if (!result) {
            return std::nullopt;
        }
        const auto tail = lexer.next();
        if (tail.type != utils::detail::json_lexer_t::token_type_t::eof) {
            return std::nullopt;
        }
        return result;
    }

    const std::unordered_map<std::string_view, error_context_serializer_t::field_parser_t>&
    error_context_serializer_t::field_dispatcher_table_() noexcept {
        static const std::unordered_map<std::string_view, field_parser_t> table = {
            {"code",         &error_context_serializer_t::parse_json_code_field_},
            {"message",      &error_context_serializer_t::parse_json_message_field_},
            {"location",     &error_context_serializer_t::parse_json_location_field_},
            {"payload",      &error_context_serializer_t::parse_json_payload_field_},
            {"stack_frames", &error_context_serializer_t::parse_json_stack_frames_field_},
            {"cause",        &error_context_serializer_t::parse_json_cause_field_},
        };
        return table;
    }

    std::optional<error_context_t> error_context_serializer_t::from_json_node_(json_lexer_t& lexer) noexcept {
        using token_type_t = json_lexer_t::token_type_t;
        auto token = lexer.next();
        if (token.type != token_type_t::left_brace) {
            return std::nullopt;
        }

        error_context_t context;

        token = lexer.next();
        if (token.type == token_type_t::right_brace) {
            return context;
        }

        while (true) {
            if (token.type != token_type_t::string) {
                return std::nullopt;
            }
            auto key = std::move(token.value);

            token = lexer.next();
            if (token.type != token_type_t::colon) {
                return std::nullopt;
            }

            const auto& table = field_dispatcher_table_();
            auto it = table.find(key);
            bool ok = (it != table.end())
                ? it->second(context, lexer)
                : skip_json_value_(lexer);
            if (!ok) {
                return std::nullopt;
            }

            token = lexer.next();
            if (token.type == token_type_t::right_brace) {
                break;
            }
            if (token.type != token_type_t::comma) {
                return std::nullopt;
            }
            token = lexer.next();
        }
        return context;
    }

    /**
     * @brief 解析 "code" 字段：字符串形式错误码 → uint64 → error_code_t，并补齐元数据
     */
    bool error_context_serializer_t::parse_json_code_field_(
        error_context_t& context, json_lexer_t& lexer) noexcept {
        const auto token = lexer.next();
        if (token.type != json_lexer_t::token_type_t::string) {
            return false;
        }
        uint64_t raw_code = 0;
        if (!parse_uint64(token.value, raw_code)) {
            return false;
        }
        context.code_ = error_code_t{raw_code};
        if (auto info = error_registry_t::instance().get_info(context.code_)) {
            context.metadata_ = *info;
        }
        return true;
    }

    /**
     * @brief 解析 "message" 字段：字符串 → context.message
     */
    bool error_context_serializer_t::parse_json_message_field_(
        error_context_t& context, json_lexer_t& lexer) noexcept {
        auto token = lexer.next();
        if (token.type != json_lexer_t::token_type_t::string) {
            return false;
        }
        context.message = std::move(token.value);
        return true;
    }

    /**
     * @brief 解析 "location" 字段：{file,function,line} 对象
     * @details 仅当三个子字段全部成功才写入 context。字段分发委托 parse_location_member_。
     */
    bool error_context_serializer_t::parse_json_location_field_(
        error_context_t& context, json_lexer_t& lexer) noexcept {
        using token_type_t = json_lexer_t::token_type_t;
        auto token = lexer.next();
        if (token.type != token_type_t::left_brace) {
            return false;
        }

        location_parse_state_t state;
        if (!parse_location_object_body_(lexer, state)) {
            return false;
        }

        if (state.got_file && state.got_function && state.got_line) {
            context.loc_file_storage_ = std::move(state.file);
            context.loc_func_storage_ = std::move(state.function);
            context.file_name = context.loc_file_storage_.c_str();
            context.source_location = utils::source_location_t(
                context.loc_file_storage_.c_str(), context.loc_func_storage_.c_str(), state.line);
        }
        return true;
    }

    /**
     * @brief 解析 payload 对象中的单个键值对
     * @details 调用前 lexer 已消费 key token；本函数从 colon 开始消费并写入 context。
     * @param lexer JSON 词法分析器
     * @param context 目标上下文
     * @param key payload 键名
     * @return bool true=成功，false=格式错误
     */
    bool error_context_serializer_t::parse_single_payload_entry_(
        json_lexer_t& lexer, error_context_t& context, std::string key) noexcept {
        using token_type_t = json_lexer_t::token_type_t;
        auto token = lexer.next();
        if (token.type != token_type_t::colon) {
            return false;
        }

        token = lexer.next();
        if (token.type != token_type_t::string) {
            return false;
        }
        context.insert_or_update_payload_(std::move(key), std::move(token.value));
        return true;
    }

    /**
     * @brief 解析 "payload" 字段：{key:value,...} 对象
     * @details 限制项数 ≤ MAX_PAYLOAD_ITEMS 防止恶意输入
     */
    bool error_context_serializer_t::parse_json_payload_field_(
        error_context_t& context, json_lexer_t& lexer) noexcept {
        using token_type_t = json_lexer_t::token_type_t;
        auto token = lexer.next();
        if (token.type != token_type_t::left_brace) {
            return false;
        }
        token = lexer.next();
        if (token.type == token_type_t::right_brace) {
            return true;
        }
        size_t payload_count = 0;
        while (true) {
            if (token.type != token_type_t::string) {
                return false;
            }
            auto pkey = std::move(token.value);

            if (!parse_single_payload_entry_(lexer, context, std::move(pkey))) {
                return false;
            }
            if (++payload_count > MAX_PAYLOAD_ITEMS) {
                return false;
            }

            token = lexer.next();
            if (token.type == token_type_t::right_brace) {
                break;
            }
            if (token.type != token_type_t::comma) {
                return false;
            }
            token = lexer.next();
        }
        return true;
    }

    /**
     * @brief 解析 "stack_frames" 字段：[frame1,frame2,...] 数组
     * @details STACKTRACE_ENABLED 关闭时跳过该字段值
     */
    bool error_context_serializer_t::parse_json_stack_frames_field_(
        error_context_t& context, json_lexer_t& lexer) noexcept {
        using token_type_t = json_lexer_t::token_type_t;
        if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
            auto token = lexer.next();
            if (token.type != token_type_t::left_bracket) {
                return false;
            }
            token = lexer.next();
            if (token.type == token_type_t::right_bracket) {
                return true;
            }
            size_t frame_count = 0;
            while (true) {
                if (token.type != token_type_t::string) {
                    return false;
                }
                try {
                    context.stack_frames.push_back(std::move(token.value));
                } catch (const std::bad_alloc&) {
                    std::fprintf(stderr, "[error_context_serializer] parse_json_stack_frames_field_: push_back failed\n");
                    return false;
                }
                if (++frame_count > MAX_STACK_FRAMES) {
                    return false;
                }

                token = lexer.next();
                if (token.type == token_type_t::right_bracket) {
                    break;
                }
                if (token.type != token_type_t::comma) {
                    return false;
                }
                token = lexer.next();
            }
            return true;
        } else {
            return skip_json_value_(lexer);
        }
    }

    /**
     * @brief 解析 "cause" 字段：递归解析子对象到 context.cause
     */
    bool error_context_serializer_t::parse_json_cause_field_(
        error_context_t& context, json_lexer_t& lexer) noexcept {
        auto cause_ctx = from_json_node_(lexer);
        if (!cause_ctx) {
            return false;
        }
        try {
            context.cause = std::make_shared<error_context_t>(std::move(*cause_ctx));
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_context_serializer] parse_json_cause_field_: make_shared failed\n");
            return false;
        }
        return true;
    }

}  // namespace error_system::core
