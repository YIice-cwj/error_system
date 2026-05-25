#include "error_system/core/error_context.h"
#include "error_system/plugin/plugin_registry.h"
#include <sstream>
using namespace error_system::config;

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
    /**
     * @brief 转换为二进制字符串
     * @return std::string 错误上下文的二进制字符串表示
     */
    namespace {
        template <typename T>
        void write_little_endian(std::string& buf, T value) noexcept {
            static_assert(std::is_integral_v<T>, "T must be an integral type");
            for (size_t i = 0; i < sizeof(T); ++i) {
                buf.push_back(static_cast<char>((value >> (i * 8)) & 0xFF));
            }
        }
    }  // namespace

    /**
     * @brief 通知所有已注册插件
     * @details 实现在 plugin_registry_t::notify_error()
     */
    void notify_plugins(const error_context_t& context) noexcept {
        plugin::plugin_registry_t::instance().notify_error(context);
    }

    /**
     * @brief 检查错误上下文是否包含错误
     * @details sign位为1表示有错误，0表示成功
     * @return bool 包含错误则返回true
     */
    bool error_context_t::is_error() const noexcept {
        return code.get_sign();
    }

    /**
     * @brief 检查错误上下文是否成功（无错误）
     * @details sign位为0表示成功，1表示错误
     * @return bool 成功则返回true
     */
    bool error_context_t::is_success() const noexcept {
        return !code.get_sign();
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
                context_pointer, [&object_pool](error_context_t* context_pointer) -> void {
                    *context_pointer = error_context_t{};
                    object_pool.release(context_pointer);
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
                context_pointer, [&object_pool](error_context_t* context_pointer) -> void {
                    *context_pointer = error_context_t{};
                    object_pool.release(context_pointer);
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
     * @return std::string 错误上下文的字符串表示
     */
    std::string error_context_t::to_string() const noexcept {
        if (const auto& formatter = error_config_t::get_custom_formatter()) {
            return formatter(*this);
        }

        std::string result{};
        std::string subsys_module_str{};
        result.reserve(512);
        if (const auto& translator = error_config_t::get_translator()) {
            subsys_module_str = translator(code.get_subsys(), code.get_module());
        } else {
            subsys_module_str =
                utils::string_utils_t::format("SubSys: {}, Module: {}", code.get_subsys(), code.get_module());
        }
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        if (error_config_t::is_source_location_enabled() && file_name != nullptr) {
            result.append(" [Location: ")
                .append(file_name)
                .append(":")
                .append(std::to_string(line_number))
                .append(" @ ")
                .append(function_name)
                .append("]");
        }
#endif

        auto info = error_system::core::error_registry_t::instance().get_info(this->code);
        const std::string& desc = info.has_value() ? info.value().get().description : "未注册的未知错误";
        const std::string& name = info.has_value() ? info.value().get().name : "UNKNOWN_ERR_CODE";

        result.append(utils::string_utils_t::format("[Sign: {} Level: {}, System: {}, {}] Code: {} ({}) - {}: {}",
                                                    is_error() ? "Error" : "Success",
                                                    core::to_string(code.get_level()),
                                                    domain::to_string(code.get_system()),
                                                    subsys_module_str,
                                                    code.get_number(),
                                                    name,
                                                    message,
                                                    desc));

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
            result.append("}");
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        if (!stack_frames.empty()) {
            result.append("\n  [Stacktrace]:");
            for (size_t i = 0; i < stack_frames.size(); ++i) {
                result.append("\n    #").append(std::to_string(i)).append("  ").append(stack_frames[i]);
            }
        }
#endif

        if (cause) {
            result.append("\n  ↳ Caused by: ").append(cause->to_string());
        }
        return result;
    }

    /**
     * @brief 转换为 JSON 字符串
     * @return std::string 错误上下文的 JSON 字符串表示
     */
    std::string error_context_t::to_json() const noexcept {
        std::ostringstream json;

        json << "{";

        bool first_field = true;
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        if (error_config_t::is_source_location_enabled() && file_name != nullptr && function_name != nullptr) {
            json << "\"location\":{";
            json << "\"file\":\"" << utils::string_utils_t::escape_json(file_name) << "\",";
            json << "\"function\":\"" << utils::string_utils_t::escape_json(function_name ? function_name : "unknown")
                 << "\",";
            json << "\"line\":" << line_number;
            json << "}";
            first_field = false;
        }
#endif
        if (!first_field)
            json << ",";
        json << "\"code\":" << code.get_code() << ",";

        json << "\"message\":\"" << utils::string_utils_t::escape_json(message) << "\"";

        if (!payload.empty()) {
            json << ",\"payload\":{";
            bool first_p = true;
            for (const auto& [key, value] : payload) {
                if (!first_p)
                    json << ",";
                json << "\"" << utils::string_utils_t::escape_json(key) << "\":\""
                     << utils::string_utils_t::escape_json(value) << "\"";
                first_p = false;
            }
            json << "}";
        }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        if (!stack_frames.empty()) {
            json << ",\"stack_frames\":[";
            bool first_s = true;
            for (const auto& frame : stack_frames) {
                if (!first_s)
                    json << ",";
                json << "\"" << utils::string_utils_t::escape_json(frame) << "\"";
                first_s = false;
            }
            json << "]";
        }
#endif
        if (cause) {
            json << ",\"cause\":" << cause->to_json();
        }

        json << "}";
        return json.str();
    }

    std::string error_context_t::to_binary() const noexcept {
        std::string buf;
        buf.reserve(128);

        auto write_string = [&buf](const std::string& str) {
            uint32_t len = static_cast<uint32_t>(str.size());
            write_little_endian(buf, len);
            buf.append(str.data(), len);
        };

        write_little_endian(buf, code.get_code());
        write_string(message);

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        uint8_t has_location = 0;
        if (error_config_t::is_source_location_enabled() && file_name != nullptr && function_name != nullptr) {
            has_location = 1;
        }
        buf.push_back(static_cast<char>(has_location));

        if (has_location) {
            write_string(file_name);
            write_string(function_name ? function_name : "unknown");
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

        return buf;
    }

}  // namespace error_system::core