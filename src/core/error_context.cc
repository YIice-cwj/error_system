#include "error_system/core/error_context.h"
#include "error_system/plugin/plugin_registry.h"

using error_system::config::error_config_t;

/**
 * @file error_context.h
 * @brief 错误上下文实现
 * @details 定义错误上下文的实现，包括通知插件、检查有效性和包装错误上下文等功能
 * @author yiice
 * @version 1.0.0
 * @date 2026-04-27
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
        void append_decimal(std::string& out, T value) {
            out.append(std::to_string(value));
        }

        void append_escaped_json_string(std::string& out, std::string_view value) {
            out.push_back('"');
            out.append(utils::string_utils_t::escape_json(std::string(value)));
            out.push_back('"');
        }

        size_t estimate_string_capacity(const error_context_t& context,
                                        size_t name_size,
                                        size_t desc_size,
                                        size_t subsys_module_size) noexcept {
            size_t capacity = 96 + name_size + desc_size + subsys_module_size + context.message.size();

            for (const auto& [key, value] : context.payload) {
                capacity += key.size() + value.size() + 4;
            }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            for (const auto& frame : context.stack_frames) {
                capacity += frame.size() + 12;
            }
#endif

            if (context.cause) {
                capacity += 16 + context.cause->message.size();
            }
            return capacity;
        }

        size_t estimate_json_capacity(const error_context_t& context) noexcept {
            size_t capacity = 64 + context.message.size();

            for (const auto& [key, value] : context.payload) {
                capacity += key.size() + value.size() + 8;
            }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            for (const auto& frame : context.stack_frames) {
                capacity += frame.size() + 4;
            }
#endif

            if (context.cause) {
                capacity += 16 + context.cause->message.size();
            }
            return capacity;
        }
    }  // namespace

    void notify_plugins(const error_context_t& context) noexcept {
        plugin::plugin_registry_t::instance().notify_error(context);
    }

    void error_context_t::finalize_runtime_features() noexcept {
        const bool validation_enabled = error_config_t::is_validation_enabled();
        const bool stacktrace_enabled = error_config_t::is_stacktrace_enabled();
        const bool location_enabled = error_config_t::is_source_location_enabled();
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        // 优先使用 per-code 堆栈等级覆盖，否则使用全局配置
        auto stacktrace_level = stacktrace_enabled ? error_config_t::get_stacktrace_level() : error_level_t::warn;
        if (stacktrace_enabled) {
            const auto per_code_level = error_config_t::get_per_code_stacktrace_level(code_.get_identity_code());
            if (per_code_level.has_value()) {
                stacktrace_level = per_code_level.value();
            }
        }
#endif
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        const bool short_filename_enabled =
            location_enabled && error_config_t::is_short_filename_enabled();
#endif

        if (validation_enabled) {
            fill_validation_fields();
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        if (stacktrace_enabled && code_.get_level() >= stacktrace_level) {
            fill_stacktrace();
        }
#endif

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        if (location_enabled) {
            fill_source_location(short_filename_enabled);
        }
#endif

        // 根据通知模式选择同步或异步
        if (error_config_t::get_notify_mode() == error_config_t::notify_mode_t::async_queue) {
            plugin::plugin_registry_t::instance().enqueue_notification(*this);
        } else {
            plugin::plugin_registry_t::instance().notify_error(*this);
        }
    }

    void error_context_t::fill_validation_fields() noexcept {
        if (error_registry_t::instance().is_registered(code_)) {
            return;
        }

        payload.insert_or_assign("illegal_raw_code", std::to_string(code_.get_code()));
        message.insert(0, "[UNREGISTERED CODE] ");
        code_ = error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFF);
        metadata_ = error_registry_t::instance().get_info(code_);  // 同步更新缓存
    }

    void error_context_t::fill_stacktrace() noexcept {
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        stack_frames = utils::stack_trace_utils_t::generate(1);
#endif
    }

    void error_context_t::fill_source_location(bool short_filename_enabled) noexcept {
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        file_name = short_filename_enabled ? utils::extract_short_filename(source_location_.file_name())
                                           : source_location_.file_name();
        function_name = source_location_.function_name();
        line_number = source_location_.line();
#else
        (void)short_filename_enabled;
#endif
    }

    bool error_context_t::is_error() const noexcept {
        return code_.is_error_code(); 
    }

    bool error_context_t::is_success() const noexcept {
        return code_.is_success_code();
    }

    error_context_t error_context_t::wrap(const error_context_t& underlying) const noexcept {
        error_context_t new_code_context = *this;

        object_pool_t& object_pool = object_pool_t::instance_thread_local();
        error_context_t* context_pointer = object_pool.acquire();
        if (context_pointer) {
            *context_pointer = underlying;
            new_code_context.cause = std::shared_ptr<error_context_t>(
                context_pointer, [&object_pool](error_context_t* context_pointer) -> void {
                    *context_pointer = error_context_t{};
                    object_pool.release(context_pointer);
                });
        } else {
            new_code_context.cause = std::make_shared<error_context_t>(underlying);
        }
        return new_code_context;
    }

    error_context_t error_context_t::wrap(error_context_t&& underlying) const noexcept {
        error_context_t new_code_context = *this;

        object_pool_t& object_pool = object_pool_t::instance_thread_local();
        error_context_t* context_pointer = object_pool.acquire();
        if (context_pointer) {
            *context_pointer = std::move(underlying);
            new_code_context.cause = std::shared_ptr<error_context_t>(
                context_pointer, [&object_pool](error_context_t* context_pointer) -> void {
                    *context_pointer = error_context_t{};
                    object_pool.release(context_pointer);
                });
        } else {
            new_code_context.cause = std::make_shared<error_context_t>(std::move(underlying));
        }
        return new_code_context;
    }

    error_context_t& error_context_t::with(const std::string& key, const std::string& value) noexcept {
        payload.insert_or_assign(key, value);
        return *this;
    }

    error_context_t& error_context_t::with(const char* key, const char* value) noexcept {
        return with(std::string_view(key != nullptr ? key : ""), std::string_view(value != nullptr ? value : ""));
    }

    error_context_t& error_context_t::with(std::string_view key, std::string_view value) noexcept {
        payload.insert_or_assign(std::string(key), std::string(value));
        return *this;
    }

    error_context_t& error_context_t::with(std::string&& key, std::string&& value) noexcept {
        payload.insert_or_assign(std::move(key), std::move(value));
        return *this;
    }

    error_context_t& error_context_t::with_batch(
        std::initializer_list<std::pair<const std::string, std::string>> items) noexcept {
        for (const auto& [key, value] : items) {
            payload.insert_or_assign(key, value);
        }
        return *this;
    }

    std::string error_context_t::to_string() const noexcept {
        if (const auto& formatter = error_config_t::get_custom_formatter()) {
            return formatter(*this);
        }

        auto& registry = error_system::core::error_registry_t::instance();
        const error_metadata_t* info = metadata_;  // 使用构造时缓存的元数据，避免重复加锁查询
        const std::string& desc = info ? info->description : "未注册的未知错误";
        const std::string& name = info ? info->name : "UNKNOWN_ERR_CODE";

        std::string subsys_module_str;
        if (error_config_t::is_text_output_enabled()) {
            const auto& sm_info = registry.get_subsystem_module_info(code_.get_subsys(), code_.get_module());
            subsys_module_str.reserve(sm_info.subsystem_name.size() + sm_info.module_name.size() + 3);
            subsys_module_str.append(sm_info.subsystem_name).append(" / ").append(sm_info.module_name);
        } else {
            subsys_module_str.reserve(32);
            subsys_module_str.append("SubSys: ");
            append_decimal(subsys_module_str, code_.get_subsys());
            subsys_module_str.append(", Module: ");
            append_decimal(subsys_module_str, code_.get_module());
        }

        std::string result;
        result.reserve(estimate_string_capacity(*this, name.size(), desc.size(), subsys_module_str.size()));

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        if (error_config_t::is_source_location_enabled() && file_name != nullptr) {
            result.append(" [Location: ").append(file_name).append(":");
            append_decimal(result, line_number);
            result.append(" @ ").append(function_name != nullptr ? function_name : "unknown").append("]");
        }
#endif

        result.append("[Sign: ")
            .append(is_error() ? "Error" : "Success")
            .append(" Level: ")
            .append(core::to_string(code_.get_level()))
            .append(", System: ")
            .append(domain::to_string(code_.get_system()))
            .append(", ")
            .append(subsys_module_str)
            .append("] Code: ");
        append_decimal(result, code_.get_number());
        result.append(" (").append(name).append(") - ").append(message).append(": ").append(desc);

        if (!payload.empty()) {
            result.append(" {");
            bool first = true;
            for (const auto& [key, value] : payload) {
                if (!first) {
                    result.append(", ");
                }
                result.append(key).append("=").append(value);
                first = false;
            }
            result.push_back('}');
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        if (!stack_frames.empty()) {
            result.append("\n  [Stacktrace]:");
            for (size_t i = 0; i < stack_frames.size(); ++i) {
                result.append("\n    #");
                append_decimal(result, i);
                result.append("  ").append(stack_frames[i]);
            }
        }
#endif

        if (cause) {
            result.append("\n  ↳ Caused by: ").append(cause->to_string());
        }
        return result;
    }

    std::string error_context_t::to_json() const noexcept {
        std::string json;
        json.reserve(estimate_json_capacity(*this));
        json.push_back('{');

        bool first_field = true;
        auto append_separator = [&json, &first_field]() {
            if (!first_field) {
                json.push_back(',');
            }
            first_field = false;
        };

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        if (error_config_t::is_source_location_enabled() && file_name != nullptr && function_name != nullptr) {
            append_separator();
            json.append("\"location\":{");
            json.append("\"file\":");
            append_escaped_json_string(json, file_name);
            json.append(",\"function\":");
            append_escaped_json_string(json, function_name != nullptr ? function_name : "unknown");
            json.append(",\"line\":");
            append_decimal(json, line_number);
            json.push_back('}');
        }
#endif

        append_separator();
        json.append("\"code\":");
        append_decimal(json, code_.get_identity_code());
        json.append(",\"message\":");
        append_escaped_json_string(json, message);

        if (!payload.empty()) {
            json.append(",\"payload\":{");
            bool first_payload = true;
            for (const auto& [key, value] : payload) {
                if (!first_payload) {
                    json.push_back(',');
                }
                append_escaped_json_string(json, key);
                json.push_back(':');
                append_escaped_json_string(json, value);
                first_payload = false;
            }
            json.push_back('}');
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        if (!stack_frames.empty()) {
            json.append(",\"stack_frames\":[");
            bool first_frame = true;
            for (const auto& frame : stack_frames) {
                if (!first_frame) {
                    json.push_back(',');
                }
                append_escaped_json_string(json, frame);
                first_frame = false;
            }
            json.push_back(']');
        }
#endif

        if (cause) {
            json.append(",\"cause\":").append(cause->to_json());
        }

        json.push_back('}');
        return json;
    }

    std::string error_context_t::to_binary() const noexcept {
        std::string buf;
        buf.reserve(128 + message.size() + payload.size() * 24);

        auto write_string = [&buf](const std::string& str) {
            const uint32_t len = static_cast<uint32_t>(str.size());
            write_little_endian(buf, len);
            buf.append(str.data(), len);
        };

        write_little_endian(buf, code_.get_code());
        write_string(message);

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        uint8_t has_location = 0;
        if (error_config_t::is_source_location_enabled() && file_name != nullptr && function_name != nullptr) {
            has_location = 1;
        }
        buf.push_back(static_cast<char>(has_location));

        if (has_location) {
            write_string(file_name);
            write_string(function_name != nullptr ? function_name : "unknown");
            write_little_endian(buf, line_number);
        }
#else
        buf.push_back(0);
#endif

        write_little_endian(buf, static_cast<uint32_t>(payload.size()));
        for (const auto& [key, value] : payload) {
            write_string(key);
            write_string(value);
        }

        if (cause) {
            buf.push_back(1);
            std::string cause_binary = cause->to_binary();
            write_string(cause_binary);
        } else {
            buf.push_back(0);
        }

        return buf;
    }

}  // namespace error_system::core
