#include "error_system/core/error_context_serializer.h"

#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"
#include "error_system/utils/json_utils.h"

using error_system::config::feature_flags_t;
using error_system::config::formatter_config_t;

/**
 * @file error_context_serializer.cc
 * @brief 错误上下文序列化器实现
 * @details 实现 error_context_serializer_t 的文本/JSON/二进制序列化方法，
 *          以及对应的匿名命名空间辅助函数。从 error_context.cc 拆分而来。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {
    namespace {
        template <typename T>
        void write_little_endian(std::string& buf, T value) noexcept {
            static_assert(std::is_integral_v<T>, "T must be an integral type");
            for (size_t i = 0; i < sizeof(T); ++i) {
                buf.push_back(static_cast<char>((value >> (i * 8)) & 0xFF));
            }
        }

        template <typename T>
        void append_decimal(std::string& out, T value) noexcept {
            try {
                out.append(std::to_string(value));
            } catch (...) {
                std::fprintf(stderr, "[error_context_serializer] append_decimal: std::bad_alloc\n");
            }
        }

        void append_escaped_json_string(std::string& out, std::string_view value) noexcept {
            try {
                out.push_back('"');
                out.append(utils::json_serializer_t::escape_json(std::string(value)));
                out.push_back('"');
            } catch (...) {
                std::fprintf(stderr, "[error_context_serializer] append_escaped_json_string: std::bad_alloc\n");
            }
        }

        size_t estimate_string_capacity(const error_context_t& context,
                                        size_t name_size,
                                        size_t desc_size,
                                        size_t subsys_module_size) noexcept {
            size_t capacity = 96 + name_size + desc_size + subsys_module_size + context.message.size();

            context.for_each_payload([&](const std::string& key, const std::string& value) {
                capacity += key.size() + value.size() + 4;
            });

            if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
                for (const auto& frame : context.stack_frames) {
                    capacity += frame.size() + 12;
                }
            }

            if (context.cause) {
                capacity += 16 + context.cause->message.size();
            }
            return capacity;
        }

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

        void append_location_text(std::string& result, const error_context_t& context) noexcept {
            if constexpr (feature_flags_t::LOCATION_ENABLED) {
                if (feature_flags_t::is_source_location_enabled() && context.file_name != nullptr) {
                    result.append(" [Location: ").append(context.file_name).append(":");
                    append_decimal(result, context.source_location.line());
                    result.append(" @ ").append(context.source_location.function_name()).append("]");
                }
            }
        }

        void append_subheader_text(std::string& result, const error_context_t& context,
                                     const std::string& subsys_module_str) noexcept {
            result.append("[Sign: ")
                .append(context.is_error() ? "Error" : "Success")
                .append(" Level: ")
                .append(core::to_string(context.get_code().get_level()))
                .append(", System: ")
                .append(domain::to_string(context.get_code().get_system()))
                .append(", ")
                .append(subsys_module_str)
                .append("] Code: ");
            append_decimal(result, context.get_code().get_number());
        }

        void append_payload_text(std::string& result, const error_context_t& context) noexcept {
            if (context.payload_size() == 0) {
                return;
            }
            result.append(" {");
            bool first = true;
            context.for_each_payload([&](const std::string& key, const std::string& value) {
                if (!first) {
                    result.append(", ");
                }
                result.append(key).append("=").append(value);
                first = false;
            });
            result.push_back('}');
        }

        void append_stacktrace_text(std::string& result, const error_context_t& context) noexcept {
            if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
                if (context.stack_frames.empty()) {
                    return;
                }
                result.append("\n  [Stacktrace]:");
                for (size_t i = 0; i < context.stack_frames.size(); ++i) {
                    result.append("\n    #");
                    append_decimal(result, i);
                    result.append("  ").append(context.stack_frames[i]);
                }
            }
        }

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

        void write_string_len_prefixed(std::string& buf, const std::string& str) noexcept {
            const size_t str_size = str.size();
            if (str_size > 0xFFFFFFFFULL) {
                std::fprintf(stderr, "[error_context_serializer] write_string_len_prefixed: string too long, truncated\n");
            }
            const uint32_t len = static_cast<uint32_t>(str_size > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : str_size);
            write_little_endian(buf, len);
            try {
                buf.append(str.data(), len);
            } catch (...) {
                std::fprintf(stderr, "[error_context_serializer] to_binary: write_string append failed\n");
            }
        }

        void write_location_binary(std::string& buf, const error_context_t& context) noexcept {
            uint8_t has_location = 0;
            if constexpr (feature_flags_t::LOCATION_ENABLED) {
                if (feature_flags_t::is_source_location_enabled() && context.file_name != nullptr) {
                    has_location = 1;
                }
            }
            buf.push_back(static_cast<char>(has_location));
            if (has_location) {
                write_string_len_prefixed(buf, context.file_name);
                write_string_len_prefixed(buf, context.source_location.function_name());
                write_little_endian(buf, context.source_location.line());
            }
        }

        void write_payload_binary(std::string& buf, const error_context_t& context) noexcept {
            const size_t total = context.payload_size();
            if (total > 0xFFFFFFFFULL) {
                std::fprintf(stderr, "[error_context_serializer] write_payload_binary: payload count overflow, truncated\n");
            }
            write_little_endian(buf, static_cast<uint32_t>(total > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : total));
            context.for_each_payload([&](const std::string& key, const std::string& value) {
                write_string_len_prefixed(buf, key);
                write_string_len_prefixed(buf, value);
            });
        }
    }  // namespace

    std::string error_context_serializer_t::to_string(const error_context_t& ctx) noexcept {
        return to_string_impl_(ctx, 0);
    }

    std::string error_context_serializer_t::to_string_impl_(const error_context_t& ctx, size_t depth) noexcept {
        if (auto formatter = formatter_config_t::get_custom_formatter()) {
            try {
                return formatter(ctx);
            } catch (...) {
                std::fprintf(stderr, "[error_context_serializer] to_string: custom formatter threw\n");
            }
        }

        constexpr size_t MAX_CAUSE_DEPTH = 32;
        const bool has_metadata = !ctx.metadata_.name.empty();
        // 使用 string_view 避免 ternary 两分支类型不一致导致的隐式 string 拷贝
        const std::string_view desc = has_metadata ? std::string_view{ctx.metadata_.description} : std::string_view{"未注册的未知错误"};
        const std::string_view name = has_metadata ? std::string_view{ctx.metadata_.name} : std::string_view{"UNKNOWN_ERR_CODE"};

        std::string subsys_module_str;
        if (feature_flags_t::is_text_output_enabled()) {
            auto& registry = error_system::core::error_registry_t::instance();
            const auto sm_info = registry.get_subsystem_module_info(ctx.code_.get_subsys(), ctx.code_.get_module());
            try {
                subsys_module_str.reserve(sm_info.subsystem_name.size() + sm_info.module_name.size() + 3);
                subsys_module_str.append(sm_info.subsystem_name).append(" / ").append(sm_info.module_name);
            } catch (...) {
                std::fprintf(stderr, "[error_context_serializer] to_string: subsys_module_str failed\n");
            }
        } else {
            try {
                subsys_module_str.reserve(32);
                subsys_module_str.append("SubSys: ");
                append_decimal(subsys_module_str, ctx.code_.get_subsys());
                subsys_module_str.append(", Module: ");
                append_decimal(subsys_module_str, ctx.code_.get_module());
            } catch (...) {
                std::fprintf(stderr, "[error_context_serializer] to_string: subsys_module_str (numeric) failed\n");
            }
        }

        std::string result;
        try {
            result.reserve(estimate_string_capacity(ctx, name.size(), desc.size(), subsys_module_str.size()));
            append_location_text(result, ctx);
            append_subheader_text(result, ctx, subsys_module_str);
            result.append(" (").append(name).append(") - ");
            if (!ctx.message.empty()) {
                result.append(ctx.message).append(": ");
            }
            result.append(desc);
            append_payload_text(result, ctx);
            append_stacktrace_text(result, ctx);

            if (ctx.cause && depth < MAX_CAUSE_DEPTH) {
                result.append("\n  ↳ Caused by: ").append(to_string_impl_(*ctx.cause, depth + 1));
            }
        } catch (...) {
            std::fprintf(stderr, "[error_context_serializer] to_string: std::bad_alloc\n");
        }
        return result;
    }

    std::string error_context_serializer_t::to_json(const error_context_t& ctx) noexcept {
        return to_json_impl_(ctx, 0);
    }

    std::string error_context_serializer_t::to_json_impl_(const error_context_t& ctx, size_t depth) noexcept {
        std::string json;
        try {
            json.reserve(estimate_json_capacity(ctx));
            json.push_back('{');

            bool first_field = true;
            append_location_json(json, ctx, first_field);

            auto append_separator = [&json, &first_field]() {
                if (!first_field) {
                    json.push_back(',');
                }
                first_field = false;
            };

            append_separator();
            json.append("\"code\":");
            append_decimal(json, ctx.code_.get_code());
            json.append(",\"message\":");
            append_escaped_json_string(json, ctx.message);
            append_payload_json(json, ctx);
            append_stacktrace_json(json, ctx);

            constexpr size_t MAX_CAUSE_DEPTH = 32;
            if (ctx.cause && depth < MAX_CAUSE_DEPTH) {
                json.append(",\"cause\":").append(to_json_impl_(*ctx.cause, depth + 1));
            }

            json.push_back('}');
        } catch (...) {
            std::fprintf(stderr, "[error_context_serializer] to_json: std::bad_alloc\n");
        }
        return json;
    }

    std::string error_context_serializer_t::to_binary(const error_context_t& ctx) noexcept {
        return to_binary_impl_(ctx, 0);
    }

    std::string error_context_serializer_t::to_binary_impl_(const error_context_t& ctx, size_t depth) noexcept {
        std::string buf;
        const size_t total_payload = ctx.payload_size();
        try {
            buf.reserve(128 + ctx.message.size() + total_payload * 24);

            write_little_endian(buf, ctx.code_.get_code());
            write_string_len_prefixed(buf, ctx.message);
            write_location_binary(buf, ctx);
            write_payload_binary(buf, ctx);

            constexpr size_t MAX_CAUSE_DEPTH = 32;
            if (ctx.cause && depth < MAX_CAUSE_DEPTH) {
                buf.push_back(1);
                std::string cause_binary = to_binary_impl_(*ctx.cause, depth + 1);
                write_string_len_prefixed(buf, cause_binary);
            } else {
                buf.push_back(0);
            }
        } catch (...) {
            std::fprintf(stderr, "[error_context_serializer] to_binary: std::bad_alloc\n");
        }
        return buf;
    }

}  // namespace error_system::core
