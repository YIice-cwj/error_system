#include "error_system/core/error_context.h"
#include "error_system/plugin/plugin_registry.h"

namespace error_system::core {
    /**
     * @brief 通知所有已注册插件
     * @details 实现在 plugin_registry_t::notify_error()
     */
    void notify_plugins(const error_context_t& context) noexcept {
        plugin::plugin_registry_t::instance().notify_error(context);
    }

    /**
     * @brief 检查错误上下文是否有效
     * @return bool 有效则返回true
     */
    error_context_t::operator bool() const noexcept {
        return code.get_code() != 0;
    }

    /**
     * @brief 包装底层错误上下文
     * @param underlying 底层错误上下文
     * @return error_context_t 包装后的错误上下文
     */
    error_context_t error_context_t::wrap(const error_context_t& underlying) const noexcept {
        error_context_t new_code_context = *this;

        object_pool_t& object_pool = object_pool_t::instance_thread_local();
        error_context_t* context_pointer = object_pool.acquire();
        if (context_pointer) {
            *context_pointer = underlying;
            new_code_context.cause = std::shared_ptr<error_context_t>(
                context_pointer,
                [](error_context_t* context_pointer) -> void {
                    *context_pointer = error_context_t{};
                    object_pool_t::instance_thread_local().release(context_pointer);
                });
        } else {
            new_code_context.cause = std::make_shared<error_context_t>(underlying);
        }
        return new_code_context;
    }

    /**
     * @brief 包装底层错误上下文
     * @param underlying 底层错误上下文
     * @return error_context_t 包装后的错误上下文
     */
    error_context_t error_context_t::wrap(error_context_t&& underlying) const noexcept {
        error_context_t new_code_context = *this;

        object_pool_t& object_pool = object_pool_t::instance_thread_local();
        error_context_t* context_pointer = object_pool.acquire();
        if (context_pointer) {
            *context_pointer = std::move(underlying);
            new_code_context.cause = std::shared_ptr<error_context_t>(
                context_pointer,
                [](error_context_t* context_pointer) -> void {
                    *context_pointer = error_context_t{};
                    object_pool_t::instance_thread_local().release(context_pointer);
                });
        } else {
            new_code_context.cause = std::make_shared<error_context_t>(std::move(underlying));
        }
        return new_code_context;
    }

    /**
     * @brief 添加字段
     * @param key 字段名
     * @param val 字段值
     * @return error_context_t& 当前错误上下文的引用
     */
    error_context_t& error_context_t::with(const std::string& key, const std::string& value) noexcept {
        payload[key] = value;
        return *this;
    }

    /**
     * @brief 添加字段
     * @param key 字段名
     * @param val 字段值
     * @return error_context_t& 当前错误上下文的引用
     */
    error_context_t& error_context_t::with(std::string&& key, std::string&& value) noexcept {
        payload[std::move(key)] = std::move(value);
        return *this;
    }

    /**
     * @brief 转换为字符串
     * @details 优先使用传入的翻译器；未传入时自动尝试全局注册的翻译器；
     *          均无则降级输出可读英文名
     * @param translator 可选的翻译器接口指针，默认为 nullptr
     * @return std::string 错误上下文的字符串表示
     */
    std::string error_context_t::to_string(const i18n::i_translator_t* translator) const noexcept {
        if (!translator) {
            translator = i18n::translator_registry_t::instance().get();
        }

        std::string result{};
        result.reserve(256);
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        if (error_config_t::is_source_location_enabled() && file_name != nullptr) {
            result += utils::string_utils_t::format(" [Location: {}:{} @ {}]", file_name, line_number, function_name);
        }
#endif
        result += utils::string_utils_t::format("{} - {}", translator->translate(code), message);

        if (!payload.empty()) {
            result += " {";
            bool first = true;
            for (const auto& [key, value] : payload) {
                if (!first) {
                    result += ", ";
                }
                result += key + "=" + value;
                first = false;
            }
            result += "}";
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        if (!stack_frames.empty()) {
            result += "\n  [Stacktrace]:";
            for (size_t i = 0; i < stack_frames.size(); ++i) {
                result += "\n    #" + std::to_string(i) + "  " + stack_frames[i];
            }
        }
#endif

        if (cause) {
            result += "\n  ↳ Caused by: " + cause->to_string(translator);
        }
        return result;
    }

    /**
     * @brief 转换为 JSON 字符串
     * @return std::string 错误上下文的 JSON 字符串表示
     */
    std::string error_context_t::to_json() const noexcept {
        std::string json{};
        json.reserve(256);
        json += "{";

        bool first_field = true;
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        if (error_config_t::is_source_location_enabled() && file_name != nullptr && function_name != nullptr) {
            json += "\"location\":{";
            json += "\"file\":\"" + utils::string_utils_t::escape_json(file_name) + "\",";
            json += "\"function\":\"" + utils::string_utils_t::escape_json(function_name ? function_name : "unknown") +
                    "\",";
            json += "\"line\":" + std::to_string(line_number);
            json += "}";
            first_field = false;
        }
#endif
        if (!first_field)
            json += ",";
        json += "\"code\":" + std::to_string(code.get_code()) + ",";

        json += "\"message\":\"" + utils::string_utils_t::escape_json(message) + "\"";

        if (!payload.empty()) {
            json += ",\"payload\":{";
            bool first_p = true;
            for (const auto& [key, value] : payload) {
                if (!first_p)
                    json += ",";
                json += "\"" + utils::string_utils_t::escape_json(key) + "\":\"" +
                        utils::string_utils_t::escape_json(value) + "\"";
                first_p = false;
            }
            json += "}";
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        if (!stack_frames.empty()) {
            json += ",\"stack_frames\":[";
            bool first_s = true;
            for (const auto& frame : stack_frames) {
                if (!first_s)
                    json += ",";
                json += "\"" + utils::string_utils_t::escape_json(frame) + "\"";
                first_s = false;
            }
            json += "]";
        }
#endif
        if (cause) {
            json += ",\"cause\":" + cause->to_json();
        }

        json += "}";
        return json;
    }

    /**
     * @brief 转换为二进制字符串
     * @return std::string 错误上下文的二进制字符串表示
     */
    std::string error_context_t::to_binary() const noexcept {
        std::string buf;
        buf.reserve(128);

        auto write_data = [&buf](const void* data, size_t size) { buf.append(static_cast<const char*>(data), size); };

        auto write_string = [&write_data](const std::string& str) {
            uint32_t len = static_cast<uint32_t>(str.size());
            write_data(&len, sizeof(len));
            write_data(str.data(), len);
        };

        uint64_t raw_code = code.get_code();
        write_data(&raw_code, sizeof(raw_code));
        write_string(message);

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        uint8_t has_location = 0;
        if (error_config_t::is_source_location_enabled() && file_name != nullptr && function_name != nullptr) {
            has_location = 1;
        }
        write_data(&has_location, sizeof(has_location));

        if (has_location) {
            write_string(file_name);
            write_string(function_name ? function_name : "unknown");
            write_data(&line_number, sizeof(line_number));
        }
#else
        uint8_t has_location = 0;
        write_data(&has_location, sizeof(has_location));
#endif

        uint32_t payload_size = static_cast<uint32_t>(payload.size());
        write_data(&payload_size, sizeof(payload_size));
        for (const auto& [key, value] : payload) {
            write_string(key);
            write_string(value);
        }

        return buf;
    }

}  // namespace error_system::core