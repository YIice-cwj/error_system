#include "error_system/core/error_context_serializer.h"
#include "error_context_serializer_internal.h"

#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"

using error_system::config::feature_flags_t;
using error_system::config::formatter_config_t;
using error_system::config::i18n_config_t;
using error_system::core::detail::append_decimal;

/**
 * @file error_context_serializer_text.cc
 * @brief 错误上下文序列化器 - 文本格式实现
 * @details 实现 error_context_serializer_t::to_string 及其递归实现 to_string_impl_。
 *          从 error_context_serializer.cc 拆分而来，仅包含文本格式相关的辅助函数与逻辑。
 *          子系统/模块名称从 i18n_config_t 解析输出 locale，保证与错误码消息使用同一语言。
 * @author yiice
 * @version 1.1.0
 * @date 2026-06-29
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {
    namespace {

        /**
         * @brief 估算文本序列化结果字符串容量，避免多次重分配
         */
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

        /**
         * @brief 追加源位置信息文本（[Location: file:line @ function]）
         */
        void append_location_text(std::string& result, const error_context_t& context) noexcept {
            if constexpr (feature_flags_t::LOCATION_ENABLED) {
                if (feature_flags_t::is_source_location_enabled() && context.file_name != nullptr) {
                    result.append(" [Location: ").append(context.file_name).append(":");
                    append_decimal(result, context.source_location.line());
                    result.append(" @ ").append(context.source_location.function_name()).append("]");
                }
            }
        }

        /**
         * @brief 追加签名/等级/系统域/子系统/模块/错误编号子头部
         */
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

        /**
         * @brief 追加 payload 文本段（{key=value, ...}）
         */
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

        /**
         * @brief 追加堆栈跟踪文本段（\n  [Stacktrace]:\n    #0  frame...）
         */
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

        /**
         * @brief 构建子系统/模块名称字符串
         * @details i18n 启用且文本输出模式开启时，从 i18n_config_t 解析输出 locale，
         *          查询本地化子系统/模块名称；
         *          否则回退为 "SubSys: X, Module: Y" 数字形式。
         *          分配失败时返回空字符串并记录日志。
         */
        std::string build_subsystem_module_string(const error_context_t& context) noexcept {
            std::string subsys_module_str;
            const auto code = context.get_code();
            const bool use_text = feature_flags_t::is_text_output_enabled()
                                  && i18n_config_t::is_i18n_enabled();
            if (use_text) {
                auto& registry = error_registry_t::instance();
                const auto output_locale = i18n_config_t::resolve_output_locale();
                const auto default_locale = i18n_config_t::get_default_locale();
                const auto sm_info = registry.get_subsystem_module_info(
                    output_locale, default_locale, code.get_subsys(), code.get_module());
                try {
                    subsys_module_str.reserve(sm_info.subsystem_name.size() + sm_info.module_name.size() + 3);
                    subsys_module_str.append(sm_info.subsystem_name).append(" / ").append(sm_info.module_name);
                } catch (...) {
                    std::fprintf(stderr, "[error_context_serializer] build_subsystem_module_string: text mode failed\n");
                }
            } else {
                try {
                    subsys_module_str.reserve(32);
                    subsys_module_str.append("SubSys: ");
                    append_decimal(subsys_module_str, code.get_subsys());
                    subsys_module_str.append(", Module: ");
                    append_decimal(subsys_module_str, code.get_module());
                } catch (...) {
                    std::fprintf(stderr, "[error_context_serializer] build_subsystem_module_string: numeric mode failed\n");
                }
            }
            return subsys_module_str;
        }

    }  // namespace

    std::string error_context_serializer_t::to_string(const error_context_t& context) noexcept {
        return to_string_impl_(context, 0);
    }

    std::string error_context_serializer_t::to_string_impl_(const error_context_t& context, size_t depth) noexcept {
        if (auto formatter = formatter_config_t::get_custom_formatter()) {
            try {
                return formatter(context);
            } catch (...) {
                std::fprintf(stderr, "[error_context_serializer] to_string: custom formatter threw\n");
            }
        }

        constexpr size_t MAX_CAUSE_DEPTH = 32;
        const bool has_metadata = !context.metadata_.name.empty();
        const std::string_view desc = has_metadata ? std::string_view{context.metadata_.description} : std::string_view{"未注册的未知错误"};
        const std::string_view name = has_metadata ? std::string_view{context.metadata_.name} : std::string_view{"UNKNOWN_ERR_CODE"};
        const std::string subsys_module_str = build_subsystem_module_string(context);

        std::string result;
        try {
            result.reserve(estimate_string_capacity(context, name.size(), desc.size(), subsys_module_str.size()));
            append_location_text(result, context);
            append_subheader_text(result, context, subsys_module_str);
            result.append(" (").append(name).append(") - ");
            if (!context.message.empty()) {
                result.append(context.message).append(": ");
            }
            result.append(desc);
            append_payload_text(result, context);
            append_stacktrace_text(result, context);

            if (context.cause && depth < MAX_CAUSE_DEPTH) {
                result.append("\n  ↳ Caused by: ").append(to_string_impl_(*context.cause, depth + 1));
            }
        } catch (...) {
            std::fprintf(stderr, "[error_context_serializer] to_string: std::bad_alloc\n");
        }
        return result;
    }

}  // namespace error_system::core
