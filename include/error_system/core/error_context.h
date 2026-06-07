#pragma once
#include "error_system/config/error_config.h"
#include "error_system/core/error_builder.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_level.h"
#include "error_system/core/error_registry.h"
#include "error_system/memory/object_pool.h"
#include "error_system/utils/source_location.h"
#include "error_system/utils/stack_trace_utils.h"
#include "error_system/utils/string_utils.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace error_system::plugin {
    class plugin_registry_t;
}  // namespace error_system::plugin

/**
 * @file error_context.h
 * @brief 错误上下文数据类定义
 * @details 定义错误上下文数据结构、字段解析和访问接口
 * @author yiice
 * @version 1.0.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {
    /**
     * @brief 错误上下文对象池大小
     * @details 用于优化内存分配和释放，避免频繁的内存分配和释放
     */
    constexpr size_t ERROR_CONTEXT_POOL_SIZE = 128;

    /**
     * @brief 通知所有已注册插件
     * @details 实现在 plugin_registry_t::notify_error()
     */
    void notify_plugins(const error_context_t& context) noexcept;

    struct code_with_location_t {
        error_code_t code;
        utils::source_location_t source_location;

        code_with_location_t(error_code_t code,
                             utils::source_location_t source_location = utils::source_location_t::current()) noexcept
            : code(code), source_location(source_location) {}
    };

    /**
     * @brief 错误上下文数据类
     * @details 封装错误上下文信息，提供字段解析和访问功能
     */
    struct error_context_t {
        using object_pool_t = memory::object_pool_t<error_context_t, ERROR_CONTEXT_POOL_SIZE>;

        error_code_t code{};
        std::string message{};
        std::unordered_map<std::string, std::string> payload{};
        std::shared_ptr<error_context_t> cause{nullptr};
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        std::vector<std::string> stack_frames{};
#endif

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        const char* file_name{nullptr};
        const char* function_name{nullptr};
        uint32_t line_number{0};
#endif

        constexpr error_context_t() noexcept = default;

        /**
         * @brief 构造函数
         * @param code_with 错误码和位置信息
         * @param format 错误信息格式化字符串
         * @param args 格式化字符串参数列表
         */
        template <typename... Args>
        error_context_t(code_with_location_t code_with, std::string message_format = "", Args&&... args) noexcept
            : code(code_with.code),
              message(utils::string_utils_t::format(message_format, std::forward<Args>(args)...)) {
            if (is_success()) {
                return;
            }

            if (config::error_config_t::is_validation_enabled()) {
                if (!error_registry_t::instance().is_registered(code)) {
                    payload["illegal_raw_code"] = std::to_string(code.get_code());
                    message = "[UNREGISTERED CODE] " + message;
                    this->code = error_builder_t::make_error_code(
                        error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFF);
                }
            }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            if (config::error_config_t::is_stacktrace_enabled() &&
                code.get_level() >= config::error_config_t::get_stacktrace_level()) {
                stack_frames = utils::stack_trace_utils_t::generate(1);
            }
#endif

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
            if (config::error_config_t::is_source_location_enabled()) {
                if (config::error_config_t::is_short_filename_enabled()) {
                    file_name = utils::extract_short_filename(code_with.source_location.file_name());
                } else {
                    file_name = code_with.source_location.file_name();
                }
                function_name = code_with.source_location.function_name();
                line_number = code_with.source_location.line();
            }
#endif
            notify_plugins(*this);
        }

        /**
         * @brief 检查错误上下文是否成功
         * @return bool 成功则返回true
         */
        bool is_success() const noexcept;

        /**
         * @brief 检查错误上下文是否包含错误
         * @return bool 包含错误则返回true
         */
        bool is_error() const noexcept;

        /**
         * @brief 包装底层错误上下文
         * @param underlying 底层错误上下文
         * @return error_context_t 包装后的错误上下文
         */
        error_context_t wrap(const error_context_t& underlying) const noexcept;

        /**
         * @brief 包装底层错误上下文
         * @param underlying 底层错误上下文
         * @return error_context_t 包装后的错误上下文
         */
        error_context_t wrap(error_context_t&& underlying) const noexcept;

        /**
         * @brief 添加字段
         * @param key 字段名
         * @param val 字段值
         * @return error_context_t& 当前错误上下文的引用
         */
        error_context_t& with(const std::string& key, const std::string& value) noexcept;

        /**
         * @brief 添加字段
         * @param key 字段名
         * @param val 字段值
         * @return error_context_t& 当前错误上下文的引用
         */
        error_context_t& with(std::string&& key, std::string&& value) noexcept;

        /**
         * @brief 转换为字符串
         * @details 从 error_registry_t 获取元数据，根据 enable_text_output 配置决定输出名称或 ID
         * @return std::string 错误上下文的字符串表示
         */
        std::string to_string() const noexcept;

        /**
         * @brief 转换为 JSON 字符串
         * @return std::string 错误上下文的 JSON 字符串表示
         */
        std::string to_json() const noexcept;

        /**
         * @brief 转换为二进制字符串
         * @return std::string 错误上下文的二进制字符串表示
         */
        std::string to_binary() const noexcept;

        /**
         * @brief 获取 payload 的只读引用
         * @return const std::unordered_map<std::string, std::string>& payload 的只读引用
         */
        const std::unordered_map<std::string, std::string>& get_payload() const noexcept { return payload; }

        /**
         * @brief 获取 C 风格错误描述字符串
         * @return const char* 错误描述字符串
         * @details 返回 message 的 C 风格字符串，生命周期与 error_context_t 对象绑定
         */
        const char* what() const noexcept { return message.c_str(); }
    };
}  // namespace error_system::core