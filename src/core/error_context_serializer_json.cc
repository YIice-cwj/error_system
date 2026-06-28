#include "error_system/core/error_context_serializer.h"
#include "error_context_serializer_internal.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"
#include "error_system/utils/json_utils.h"

using error_system::config::feature_flags_t;
using error_system::core::detail::append_decimal;

/**
 * @file error_context_serializer_json.cc
 * @brief 错误上下文序列化器 - JSON 格式实现
 * @details 实现 error_context_serializer_t 的 JSON 序列化（to_json / to_json_impl_）
 *          与反序列化（from_json / from_json_node_）。
 *          从 error_context_serializer.cc 拆分而来，仅包含 JSON 格式相关的辅助函数与逻辑。
 *          反序列化采用流式解析（不构建中间 JSON 树），直接填充 error_context_t 私有字段。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
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
            } catch (...) {
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
            } catch (...) {
                return false;
            }
        }

        /**
         * @brief 跳过空白字符
         */
        void skip_ws(std::string_view text, size_t& position) noexcept {
            while (position < text.size()) {
                const char c = text[position];
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    ++position;
                } else {
                    break;
                }
            }
        }

        /**
         * @brief 匹配并消费指定字符
         */
        bool match_char(std::string_view text, size_t& position, char expected) noexcept {
            if (position < text.size() && text[position] == expected) {
                ++position;
                return true;
            }
            return false;
        }

        /**
         * @brief 匹配并消费指定关键字
         */
        bool consume_keyword(std::string_view text, size_t& position, std::string_view keyword) noexcept {
            if (position + keyword.size() > text.size()) {
                return false;
            }
            if (text.substr(position, keyword.size()) != keyword) {
                return false;
            }
            position += keyword.size();
            return true;
        }

        /**
         * @brief 解析 \uXXXX 转义序列的 4 位十六进制码点
         * @details position 指向第一个十六进制位，成功时越过第 4 位。
         *          仅处理 BMP（忽略代理对——错误上下文场景足够）。
         *          码点按 UTF-8 编码追加到 output。
         * @return bool true=成功，false=格式错误或数据不足
         */
        bool parse_unicode_escape(std::string_view text, size_t& position, std::string& output) noexcept {
            if (position + 4 > text.size()) {
                return false;
            }
            uint32_t cp = 0;
            for (int i = 0; i < 4; ++i) {
                const char h = text[position++];
                cp <<= 4;
                if (h >= '0' && h <= '9') {
                    cp |= static_cast<uint32_t>(h - '0');
                } else if (h >= 'a' && h <= 'f') {
                    cp |= static_cast<uint32_t>(h - 'a' + 10);
                } else if (h >= 'A' && h <= 'F') {
                    cp |= static_cast<uint32_t>(h - 'A' + 10);
                } else {
                    return false;
                }
            }
            if (cp < 0x80) {
                output.push_back(static_cast<char>(cp));
            } else if (cp < 0x800) {
                output.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                output.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            return true;
        }

        /**
         * @brief 处理反斜杠转义序列，position 指向转义字符
         * @details 成功时 position 越过转义字符，解码结果追加到 output。
         *          支持：\" \\ \/ \b \f \n \r \t \uXXXX。
         * @return bool true=成功，false=格式错误或数据不足
         */
        bool parse_escape_sequence(std::string_view text, size_t& position, std::string& output) noexcept {
            if (position >= text.size()) {
                return false;
            }
            const char esc = text[position++];
            switch (esc) {
                case '"':  output.push_back('"');  return true;
                case '\\': output.push_back('\\'); return true;
                case '/':  output.push_back('/');  return true;
                case 'b':  output.push_back('\b'); return true;
                case 'f':  output.push_back('\f'); return true;
                case 'n':  output.push_back('\n'); return true;
                case 'r':  output.push_back('\r'); return true;
                case 't':  output.push_back('\t'); return true;
                case 'u':  return parse_unicode_escape(text, position, output);
                default:   return false;
            }
        }

        /**
         * @brief 解析 JSON 字符串字面量（含转义），position 指向起始引号
         * @details 成功时 position 越过结束引号；失败返回 std::nullopt。
         *          支持标准 JSON 转义序列：\" \\ \/ \b \f \n \r \t \uXXXX。
         *          \uXXXX 按 UTF-8 编码追加（仅处理 BMP，忽略代理对——错误上下文场景足够）。
         *          分配失败（bad_alloc）时返回 std::nullopt 并记录日志。
         */
        std::optional<std::string> parse_json_string(std::string_view text, size_t& position) noexcept {
            if (position >= text.size() || text[position] != '"') {
                return std::nullopt;
            }
            ++position;
            std::string output;
            try {
                output.reserve(16);
                while (position < text.size()) {
                    const char c = text[position++];
                    if (c == '"') {
                        return output;
                    }
                    if (c == '\\') {
                        if (!parse_escape_sequence(text, position, output)) {
                            return std::nullopt;
                        }
                    } else {
                        output.push_back(c);
                    }
                }
            } catch (...) {
                std::fprintf(stderr, "[error_context_serializer] parse_json_string: std::bad_alloc\n");
                return std::nullopt;
            }
            return std::nullopt;
        }

        /**
         * @brief 解析 JSON 数字字面量到 double
         * @details 支持 JSON 数字语法：可选负号、整数部分、可选小数部分、可选指数部分（e/E +/- digits）。
         *          成功时 position 越过整个数字字面量，output 写入解析值。
         *          失败（空字面量或 std::stod 异常）时 position 不变，返回 false。
         */
        bool parse_json_number(std::string_view text, size_t& position, double& output) noexcept {
            const size_t start = position;
            if (position < text.size() && text[position] == '-') {
                ++position;
            }
            while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
                ++position;
            }
            if (position < text.size() && text[position] == '.') {
                ++position;
                while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
                    ++position;
                }
            }
            if (position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
                ++position;
                if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
                    ++position;
                }
                while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
                    ++position;
                }
            }
            if (position == start) {
                return false;
            }
            try {
                const std::string number_text(text.substr(start, position - start));
                size_t parsed_index = 0;
                const double value = std::stod(number_text, &parsed_index);
                if (parsed_index != number_text.size()) {
                    return false;
                }
                output = value;
                return true;
            } catch (...) {
                return false;
            }
        }

        /**
         * @brief 跳过任意 JSON 值（用于忽略未知字段）
         * @details 递归跳过完整的 JSON 值：对象 {}、数组 []、字符串、数字、true/false/null。
         *          成功时 position 越过整个值；失败返回 false（position 状态未定义）。
         *          递归深度无显式限制，依赖 JSON 输入深度合理性。
         */
        bool skip_json_value(std::string_view text, size_t& position) noexcept;

        /**
         * @brief 跳过 JSON 对象（{...}）
         * @details position 指向起始 '{'，成功时越过结束 '}'。
         *          支持空对象 {} 与嵌套对象。键必须是字符串，值递归调用 skip_json_value。
         */
        bool skip_json_object(std::string_view text, size_t& position) noexcept {
            ++position;
            skip_ws(text, position);
            if (position < text.size() && text[position] == '}') {
                ++position;
                return true;
            }
            while (true) {
                skip_ws(text, position);
                if (!parse_json_string(text, position)) {
                    return false;
                }
                skip_ws(text, position);
                if (!match_char(text, position, ':')) {
                    return false;
                }
                if (!skip_json_value(text, position)) {
                    return false;
                }
                skip_ws(text, position);
                if (match_char(text, position, ',')) {
                    continue;
                }
                return match_char(text, position, '}');
            }
        }

        /**
         * @brief 跳过 JSON 数组（[...]）
         * @details position 指向起始 '['，成功时越过结束 ']'。
         *          支持空数组 [] 与嵌套数组。元素递归调用 skip_json_value。
         */
        bool skip_json_array(std::string_view text, size_t& position) noexcept {
            ++position;
            skip_ws(text, position);
            if (position < text.size() && text[position] == ']') {
                ++position;
                return true;
            }
            while (true) {
                if (!skip_json_value(text, position)) {
                    return false;
                }
                skip_ws(text, position);
                if (match_char(text, position, ',')) {
                    continue;
                }
                return match_char(text, position, ']');
            }
        }

        bool skip_json_value(std::string_view text, size_t& position) noexcept {
            skip_ws(text, position);
            if (position >= text.size()) {
                return false;
            }
            const char c = text[position];
            switch (c) {
                case '{':
                    return skip_json_object(text, position);
                case '[':
                    return skip_json_array(text, position);
                case '"':
                    return parse_json_string(text, position).has_value();
                case 't':
                    return consume_keyword(text, position, "true");
                case 'f':
                    return consume_keyword(text, position, "false");
                case 'n':
                    return consume_keyword(text, position, "null");
                default:
                    if (c == '-' || (c >= '0' && c <= '9')) {
                        double d = 0;
                        return parse_json_number(text, position, d);
                    }
                    return false;
            }
        }

        /**
         * @brief 解析 JSON location 对象的单个字段
         * @details 用于 parse_json_location_field_ 的字段分发，替代 if-else 链。
         *          返回 true 表示已消费字段值（不论是否识别）；false 表示格式错误。
         *          out_file/out_func/out_line 通过引用回写，仅识别 file/function/line 三个键。
         */
        bool parse_location_member(std::string_view key, std::string_view json, size_t& offset,
                                   std::string& out_file, std::string& out_func, uint32_t& out_line,
                                   bool& got_file, bool& got_func, bool& got_line) noexcept {
            if (key == "file") {
                auto v = parse_json_string(json, offset);
                if (!v) { return false; }
                out_file = std::move(*v);
                got_file = true;
                return true;
            }
            if (key == "function") {
                auto v = parse_json_string(json, offset);
                if (!v) { return false; }
                out_func = std::move(*v);
                got_func = true;
                return true;
            }
            if (key == "line") {
                double d = 0;
                if (!parse_json_number(json, offset, d) || d < 0) {
                    return false;
                }
                out_line = static_cast<uint32_t>(d);
                got_line = true;
                return true;
            }
            return skip_json_value(json, offset);
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

            constexpr size_t MAX_CAUSE_DEPTH = 32;
            if (context.cause && depth < MAX_CAUSE_DEPTH) {
                json.append(",\"cause\":").append(to_json_impl_(*context.cause, depth + 1));
            }

            json.push_back('}');
        } catch (...) {
            std::fprintf(stderr, "[error_context_serializer] to_json: std::bad_alloc\n");
        }
        return json;
    }

    std::optional<error_context_t> error_context_serializer_t::from_json(std::string_view json) noexcept {
        size_t offset = 0;
        auto result = from_json_node_(json, offset);
        if (!result) {
            return std::nullopt;
        }
        skip_ws(json, offset);
        if (offset != json.size()) {
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

    std::optional<error_context_t> error_context_serializer_t::from_json_node_(
        std::string_view json, size_t& offset) noexcept {
        skip_ws(json, offset);
        if (!match_char(json, offset, '{')) {
            return std::nullopt;
        }

        error_context_t context;

        skip_ws(json, offset);
        if (match_char(json, offset, '}')) {
            return context;
        }

        while (true) {
            skip_ws(json, offset);
            auto key = parse_json_string(json, offset);
            if (!key) {
                return std::nullopt;
            }
            skip_ws(json, offset);
            if (!match_char(json, offset, ':')) {
                return std::nullopt;
            }
            skip_ws(json, offset);

            const auto& table = field_dispatcher_table_();
            auto it = table.find(*key);
            bool ok = (it != table.end())
                ? it->second(context, json, offset)
                : skip_json_value(json, offset);
            if (!ok) {
                return std::nullopt;
            }

            skip_ws(json, offset);
            if (match_char(json, offset, ',')) {
                continue;
            }
            if (match_char(json, offset, '}')) {
                break;
            }
            return std::nullopt;
        }
        return context;
    }

    /**
     * @brief 解析 "code" 字段：字符串形式错误码 → uint64 → error_code_t，并补齐元数据
     */
    bool error_context_serializer_t::parse_json_code_field_(
        error_context_t& context, std::string_view json, size_t& offset) noexcept {
        auto code_str = parse_json_string(json, offset);
        if (!code_str) {
            return false;
        }
        uint64_t raw_code = 0;
        if (!parse_uint64(*code_str, raw_code)) {
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
        error_context_t& context, std::string_view json, size_t& offset) noexcept {
        auto msg = parse_json_string(json, offset);
        if (!msg) {
            return false;
        }
        context.message = std::move(*msg);
        return true;
    }

    /**
     * @brief 解析 "location" 字段：{file,function,line} 对象
     * @details 仅当三个子字段全部成功才写入 context。字段分发委托 parse_location_member。
     */
    bool error_context_serializer_t::parse_json_location_field_(
        error_context_t& context, std::string_view json, size_t& offset) noexcept {
        if (!match_char(json, offset, '{')) {
            return false;
        }
        std::string file;
        std::string func;
        uint32_t line = 0;
        bool got_file = false, got_func = false, got_line = false;
        skip_ws(json, offset);
        if (match_char(json, offset, '}')) {
            return false;
        }
        while (true) {
            skip_ws(json, offset);
            auto loc_key = parse_json_string(json, offset);
            if (!loc_key) {
                return false;
            }
            skip_ws(json, offset);
            if (!match_char(json, offset, ':')) {
                return false;
            }
            skip_ws(json, offset);
            if (!parse_location_member(*loc_key, json, offset,
                                       file, func, line,
                                       got_file, got_func, got_line)) {
                return false;
            }
            skip_ws(json, offset);
            if (match_char(json, offset, ',')) {
                continue;
            }
            if (match_char(json, offset, '}')) {
                break;
            }
            return false;
        }
        if (got_file && got_func && got_line) {
            context.loc_file_storage_ = std::move(file);
            context.loc_func_storage_ = std::move(func);
            context.file_name = context.loc_file_storage_.c_str();
            context.source_location = utils::source_location_t(
                context.loc_file_storage_.c_str(), context.loc_func_storage_.c_str(), line);
        }
        return true;
    }

    /**
     * @brief 解析 "payload" 字段：{key:value,...} 对象
     * @details 限制项数 ≤ 100000 防止恶意输入
     */
    bool error_context_serializer_t::parse_json_payload_field_(
        error_context_t& context, std::string_view json, size_t& offset) noexcept {
        if (!match_char(json, offset, '{')) {
            return false;
        }
        skip_ws(json, offset);
        if (match_char(json, offset, '}')) {
            return true;
        }
        size_t payload_count = 0;
        while (true) {
            skip_ws(json, offset);
            auto pkey = parse_json_string(json, offset);
            if (!pkey) {
                return false;
            }
            skip_ws(json, offset);
            if (!match_char(json, offset, ':')) {
                return false;
            }
            skip_ws(json, offset);
            auto pval = parse_json_string(json, offset);
            if (!pval) {
                return false;
            }
            context.insert_or_update_payload_(std::move(*pkey), std::move(*pval));
            if (++payload_count > 100000) {
                return false;
            }
            skip_ws(json, offset);
            if (match_char(json, offset, ',')) {
                continue;
            }
            if (match_char(json, offset, '}')) {
                break;
            }
            return false;
        }
        return true;
    }

    /**
     * @brief 解析 "stack_frames" 字段：[frame1,frame2,...] 数组
     * @details STACKTRACE_ENABLED 关闭时跳过该字段值
     */
    bool error_context_serializer_t::parse_json_stack_frames_field_(
        error_context_t& context, std::string_view json, size_t& offset) noexcept {
        if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
            if (!match_char(json, offset, '[')) {
                return false;
            }
            skip_ws(json, offset);
            if (match_char(json, offset, ']')) {
                return true;
            }
            size_t frame_count = 0;
            while (true) {
                skip_ws(json, offset);
                auto frame = parse_json_string(json, offset);
                if (!frame) {
                    return false;
                }
                try {
                    context.stack_frames.push_back(std::move(*frame));
                } catch (...) {
                    std::fprintf(stderr, "[error_context_serializer] parse_json_stack_frames_field_: push_back failed\n");
                    return false;
                }
                if (++frame_count > 100000) {
                    return false;
                }
                skip_ws(json, offset);
                if (match_char(json, offset, ',')) {
                    continue;
                }
                if (match_char(json, offset, ']')) {
                    break;
                }
                return false;
            }
            return true;
        } else {
            return skip_json_value(json, offset);
        }
    }

    /**
     * @brief 解析 "cause" 字段：递归解析子对象到 context.cause
     */
    bool error_context_serializer_t::parse_json_cause_field_(
        error_context_t& context, std::string_view json, size_t& offset) noexcept {
        auto cause_ctx = from_json_node_(json, offset);
        if (!cause_ctx) {
            return false;
        }
        try {
            context.cause = std::make_shared<error_context_t>(std::move(*cause_ctx));
        } catch (...) {
            std::fprintf(stderr, "[error_context_serializer] parse_json_cause_field_: make_shared failed\n");
            return false;
        }
        return true;
    }

}  // namespace error_system::core
