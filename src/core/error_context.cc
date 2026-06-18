#include "error_system/core/error_context.h"
#include "error_system/plugin/plugin_registry.h"

using error_system::config::error_config_t;

/**
 * @file error_context.cc
 * @brief 错误上下文实现
 * @details 定义错误上下文的实现，包括通知插件、检查有效性和包装错误上下文等功能。
 *          payload 采用 SSO（Small Size Optimization），≤4 项时栈上存储零堆分配。
 * @author yiice
 * @version 2.3.0
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
        void append_decimal(std::string& out, T value) noexcept {
            try {
                out.append(std::to_string(value));
            } catch (...) {
                std::fprintf(stderr, "[error_context] append_decimal: std::bad_alloc\n");
            }
        }

        void append_escaped_json_string(std::string& out, std::string_view value) noexcept {
            try {
                out.push_back('"');
                out.append(utils::string_utils_t::escape_json(std::string(value)));
                out.push_back('"');
            } catch (...) {
                std::fprintf(stderr, "[error_context] append_escaped_json_string: std::bad_alloc\n");
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

            if constexpr (error_config_t::STACKTRACE_ENABLED) {
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

            if constexpr (error_config_t::STACKTRACE_ENABLED) {
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
            if constexpr (error_config_t::LOCATION_ENABLED) {
                if (error_config_t::is_source_location_enabled() && context.file_name != nullptr) {
                    result.append(" [Location: ").append(context.file_name).append(":");
                    append_decimal(result, context.line_number);
                    result.append(" @ ").append(
                        context.function_name != nullptr ? context.function_name : "unknown").append("]");
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
            if constexpr (error_config_t::STACKTRACE_ENABLED) {
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
            if constexpr (error_config_t::LOCATION_ENABLED) {
                if (error_config_t::is_source_location_enabled()
                    && context.file_name != nullptr && context.function_name != nullptr) {
                    if (!first_field) {
                        json.push_back(',');
                    }
                    first_field = false;
                    json.append("\"location\":{");
                    json.append("\"file\":");
                    append_escaped_json_string(json, context.file_name);
                    json.append(",\"function\":");
                    append_escaped_json_string(json, context.function_name);
                    json.append(",\"line\":");
                    append_decimal(json, context.line_number);
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
            if constexpr (error_config_t::STACKTRACE_ENABLED) {
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
            const uint32_t len = static_cast<uint32_t>(str.size());
            write_little_endian(buf, len);
            try {
                buf.append(str.data(), len);
            } catch (...) {
                std::fprintf(stderr, "[error_context] to_binary: write_string append failed\n");
            }
        }

        void write_location_binary(std::string& buf, const error_context_t& context) noexcept {
            uint8_t has_location = 0;
            if constexpr (error_config_t::LOCATION_ENABLED) {
                if (error_config_t::is_source_location_enabled()
                    && context.file_name != nullptr && context.function_name != nullptr) {
                    has_location = 1;
                }
            }
            buf.push_back(static_cast<char>(has_location));
            if (has_location) {
                write_string_len_prefixed(buf, context.file_name);
                write_string_len_prefixed(buf,
                    context.function_name != nullptr ? context.function_name : "unknown");
                write_little_endian(buf, context.line_number);
            }
        }

        void write_payload_binary(std::string& buf, const error_context_t& context) noexcept {
            const size_t total = context.payload_size();
            write_little_endian(buf, static_cast<uint32_t>(total));
            context.for_each_payload([&](const std::string& key, const std::string& value) {
                write_string_len_prefixed(buf, key);
                write_string_len_prefixed(buf, value);
            });
        }

        void write_cause_binary(std::string& buf, const error_context_t& context) noexcept {
            if (context.cause) {
                buf.push_back(1);
                std::string cause_binary = context.cause->to_binary();
                write_string_len_prefixed(buf, cause_binary);
            } else {
                buf.push_back(0);
            }
        }
    }  // namespace

    void notify_plugins(const error_context_t& context) noexcept {
        plugin::plugin_registry_t::instance().notify_error(context);
    }

    void error_context_t::__finalize_runtime_features() noexcept {
        const bool validation_enabled = error_config_t::is_validation_enabled();
        const bool stacktrace_enabled = error_config_t::is_stacktrace_enabled();
        const bool location_enabled = error_config_t::is_source_location_enabled();

        auto stacktrace_level = core::error_level_t::warn;
        if constexpr (error_config_t::STACKTRACE_ENABLED) {
            stacktrace_level = stacktrace_enabled ? error_config_t::get_stacktrace_level() : core::error_level_t::warn;
            if (stacktrace_enabled) {
                const auto per_code_level = error_config_t::get_per_code_stacktrace_level(code_.get_identity_code());
                if (per_code_level.has_value()) {
                    stacktrace_level = per_code_level.value();
                }
            }
        }

        const bool short_filename_enabled = [&]() {
            if constexpr (error_config_t::LOCATION_ENABLED) {
                return location_enabled && error_config_t::is_short_filename_enabled();
            } else {
                return false;
            }
        }();

        if (validation_enabled) {
            __fill_validation_fields();
        }

        if constexpr (error_config_t::STACKTRACE_ENABLED) {
            if (stacktrace_enabled && code_.get_level() >= stacktrace_level) {
                __fill_stacktrace();
            }
        }

        if constexpr (error_config_t::LOCATION_ENABLED) {
            if (location_enabled) {
                __fill_source_location(short_filename_enabled);
            }
        }

        if (error_config_t::get_notify_mode() == error_config_t::notify_mode_t::async_queue) {
            plugin::plugin_registry_t::instance().enqueue_notification(*this);
        } else {
            plugin::plugin_registry_t::instance().notify_error(*this);
        }
    }

    void error_context_t::__fill_validation_fields() noexcept {
        if (error_registry_t::instance().is_registered(code_)) {
            return;
        }

        with("illegal_raw_code", std::to_string(code_.get_code()));
        try {
            message.insert(0, "[UNREGISTERED CODE] ");
        } catch (...) {
            std::fprintf(stderr, "[error_context] __fill_validation_fields: std::bad_alloc\n");
        }
        code_ = error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFF);
        metadata_ = error_registry_t::instance().get_info(code_);
    }

    void error_context_t::__fill_stacktrace() noexcept {
        if constexpr (error_config_t::STACKTRACE_ENABLED) {
            stack_frames = utils::stack_trace_utils_t::generate(1);
        }
    }

    void error_context_t::__fill_source_location(bool short_filename_enabled) noexcept {
        if constexpr (error_config_t::LOCATION_ENABLED) {
            file_name = short_filename_enabled ? utils::extract_short_filename(source_location.file_name())
                                               : source_location.file_name();
            function_name = source_location.function_name();
            line_number = source_location.line();
        }
    }

    bool error_context_t::is_error() const noexcept {
        return code_.is_error_code(); 
    }

    bool error_context_t::is_success() const noexcept {
        return code_.is_success_code();
    }

    error_context_t error_context_t::wrap(const error_context_t& underlying) const noexcept {
        error_context_t new_code_context = *this;
        try {
            new_code_context.cause = std::make_shared<error_context_t>(underlying);
        } catch (...) {
            std::fprintf(stderr, "[error_context] wrap: std::bad_alloc\n");
        }
        return new_code_context;
    }

    error_context_t error_context_t::wrap(error_context_t&& underlying) const noexcept {
        error_context_t new_code_context = *this;
        try {
            new_code_context.cause = std::make_shared<error_context_t>(std::move(underlying));
        } catch (...) {
            std::fprintf(stderr, "[error_context] wrap(&&): std::bad_alloc\n");
        }
        return new_code_context;
    }

    error_context_t& error_context_t::with(const std::string& key, const std::string& value) noexcept {
        const size_t sso_end = std::min<size_t>(payload_count_, PAYLOAD_SSO_CAPACITY);
        for (size_t i = 0; i < sso_end; ++i) {
            if (payload_small_[i].first == key) {
                payload_small_[i].second = value;
                return *this;
            }
        }

        if (payload_overflow_) {
            payload_overflow_->insert_or_assign(key, value);
            return *this;
        }

        if (payload_count_ < PAYLOAD_SSO_CAPACITY) {
            payload_small_[payload_count_].first = key;
            payload_small_[payload_count_].second = value;
            ++payload_count_;
            return *this;
        }

        try {
            auto overflow = std::make_unique<std::unordered_map<std::string, std::string>>();
            for (size_t i = 0; i < PAYLOAD_SSO_CAPACITY; ++i) {
                overflow->emplace(payload_small_[i].first, payload_small_[i].second);
            }
            overflow->insert_or_assign(key, value);
            payload_overflow_ = std::move(overflow);
            for (size_t i = 0; i < PAYLOAD_SSO_CAPACITY; ++i) {
                payload_small_[i] = {};
            }
            payload_count_ = 0;
        } catch (...) {
            std::fprintf(stderr, "[error_context] with: std::bad_alloc\n");
        }
        return *this;
    }

    error_context_t& error_context_t::with(const char* key, const char* value) noexcept {
        return with(std::string_view(key != nullptr ? key : ""), std::string_view(value != nullptr ? value : ""));
    }

    error_context_t& error_context_t::with(std::string_view key, std::string_view value) noexcept {
        return with(std::string(key), std::string(value));
    }

    error_context_t& error_context_t::with(std::string&& key, std::string&& value) noexcept {
        const size_t sso_end = std::min<size_t>(payload_count_, PAYLOAD_SSO_CAPACITY);
        for (size_t i = 0; i < sso_end; ++i) {
            if (payload_small_[i].first == key) {
                payload_small_[i].second = std::move(value);
                return *this;
            }
        }

        if (payload_overflow_) {
            payload_overflow_->insert_or_assign(std::move(key), std::move(value));
            return *this;
        }

        if (payload_count_ < PAYLOAD_SSO_CAPACITY) {
            payload_small_[payload_count_].first = std::move(key);
            payload_small_[payload_count_].second = std::move(value);
            ++payload_count_;
            return *this;
        }

        try {
            auto overflow = std::make_unique<std::unordered_map<std::string, std::string>>();
            for (size_t i = 0; i < PAYLOAD_SSO_CAPACITY; ++i) {
                overflow->emplace(payload_small_[i].first, payload_small_[i].second);
            }
            overflow->insert_or_assign(std::move(key), std::move(value));
            payload_overflow_ = std::move(overflow);
            for (size_t i = 0; i < PAYLOAD_SSO_CAPACITY; ++i) {
                payload_small_[i] = {};
            }
            payload_count_ = 0;
        } catch (...) {
            std::fprintf(stderr, "[error_context] with(string_view): std::bad_alloc\n");
        }
        return *this;
    }

    error_context_t& error_context_t::with_batch(
        std::initializer_list<std::pair<const std::string, std::string>> items) noexcept {
        for (const auto& [key, value] : items) {
            with(key, value);
        }
        return *this;
    }

    bool error_context_t::operator==(const error_context_t& other) const noexcept {
        if (code_.get_code() != other.code_.get_code() || message != other.message) {
            return false;
        }
        if (payload_count_ != other.payload_count_) {
            return false;
        }
        // 构建临时 map 比较（payload 数量通常 ≤4，copy 开销极小）
        auto this_payload = get_payload();
        auto other_payload = other.get_payload();
        for (const auto& this_pair : this_payload) {
            const auto& key = this_pair.first;
            const auto& value = this_pair.second;
            auto it = std::find_if(other_payload.begin(), other_payload.end(),
                                   [&key](const auto& pair) { return pair.first == key; });
            if (it == other_payload.end() || it->second != value) {
                return false;
            }
        }
        return true;
    }

    std::vector<std::pair<std::string, std::string>> error_context_t::get_payload() const noexcept {
        std::vector<std::pair<std::string, std::string>> result;
        try {
            result.reserve(payload_count_ + (payload_overflow_ ? payload_overflow_->size() : 0));
            for_each_payload([&](const std::string& key, const std::string& value) {
                result.emplace_back(key, value);
            });
        } catch (...) {
            std::fprintf(stderr, "[error_context] get_payload: std::bad_alloc\n");
        }
        return result;
    }

    std::optional<std::string> error_context_t::get_payload_value(const std::string& key) const noexcept {
        try {
            for (size_t i = 0; i < payload_count_ && i < PAYLOAD_SSO_CAPACITY; ++i) {
                if (payload_small_[i].first == key) {
                    return payload_small_[i].second;
                }
            }
            if (payload_overflow_) {
                auto it = payload_overflow_->find(key);
                if (it != payload_overflow_->end()) {
                    return it->second;
                }
            }
        } catch (...) {
            std::fprintf(stderr, "[error_context] get_payload_value: unexpected exception\n");
        }
        return std::nullopt;
    }

    std::string error_context_t::to_string() const noexcept {
        if (auto formatter = error_config_t::get_custom_formatter()) {
            return formatter(*this);
        }

        auto& registry = error_system::core::error_registry_t::instance();
        const error_metadata_t* info = metadata_;
        const std::string& desc = info ? info->description : "未注册的未知错误";
        const std::string& name = info ? info->name : "UNKNOWN_ERR_CODE";

        std::string subsys_module_str;
        if (error_config_t::is_text_output_enabled()) {
            const auto& sm_info = registry.get_subsystem_module_info(code_.get_subsys(), code_.get_module());
            try {
                subsys_module_str.reserve(sm_info.subsystem_name.size() + sm_info.module_name.size() + 3);
                subsys_module_str.append(sm_info.subsystem_name).append(" / ").append(sm_info.module_name);
            } catch (...) {
                std::fprintf(stderr, "[error_context] to_string: subsys_module_str failed\n");
            }
        } else {
            try {
                subsys_module_str.reserve(32);
                subsys_module_str.append("SubSys: ");
                append_decimal(subsys_module_str, code_.get_subsys());
                subsys_module_str.append(", Module: ");
                append_decimal(subsys_module_str, code_.get_module());
            } catch (...) {
                std::fprintf(stderr, "[error_context] to_string: subsys_module_str (numeric) failed\n");
            }
        }

        std::string result;
        try {
            result.reserve(estimate_string_capacity(*this, name.size(), desc.size(), subsys_module_str.size()));
        } catch (...) {
            std::fprintf(stderr, "[error_context] to_string: reserve failed\n");
        }

        append_location_text(result, *this);
        append_subheader_text(result, *this, subsys_module_str);
        result.append(" (").append(name).append(") - ");
        if (!message.empty()) {
            result.append(message).append(": ");
        }
        result.append(desc);
        append_payload_text(result, *this);
        append_stacktrace_text(result, *this);

        if (cause) {
            result.append("\n  ↳ Caused by: ").append(cause->to_string());
        }
        return result;
    }

    std::string error_context_t::to_json() const noexcept {
        std::string json;
        try {
            json.reserve(estimate_json_capacity(*this));
        } catch (...) {
            std::fprintf(stderr, "[error_context] to_json: reserve failed\n");
        }
        json.push_back('{');

        bool first_field = true;
        append_location_json(json, *this, first_field);

        auto append_separator = [&json, &first_field]() {
            if (!first_field) {
                json.push_back(',');
            }
            first_field = false;
        };

        append_separator();
        json.append("\"code\":");
        append_decimal(json, code_.get_identity_code());
        json.append(",\"message\":");
        append_escaped_json_string(json, message);
        append_payload_json(json, *this);
        append_stacktrace_json(json, *this);

        if (cause) {
            json.append(",\"cause\":").append(cause->to_json());
        }

        json.push_back('}');
        return json;
    }

    std::string error_context_t::to_binary() const noexcept {
        std::string buf;
        const size_t total_payload = payload_count_ + (payload_overflow_ ? payload_overflow_->size() : 0);
        try {
            buf.reserve(128 + message.size() + total_payload * 24);
        } catch (...) {
            std::fprintf(stderr, "[error_context] to_binary: reserve failed\n");
        }

        write_little_endian(buf, code_.get_code());
        write_string_len_prefixed(buf, message);
        write_location_binary(buf, *this);
        write_payload_binary(buf, *this);
        write_cause_binary(buf, *this);

        return buf;
    }

}  // namespace error_system::core