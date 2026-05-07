#pragma once
#include "error_system/core/error_builder.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_config.h"
#include "error_system/core/error_registry.h"
#include "error_system/i18n/translator_registry.h"
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

    struct error_context_t;
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
        using object_pool_t = memory::object_pool_t<error_context_t, 128>;

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
         * @param code 错误码
         * @param message 错误信息
         */
        template <typename... Args>
        error_context_t(code_with_location_t code_with, std::string format = "", Args&&... args) noexcept
            : code(code_with.code), message(utils::string_utils_t::format(format, std::forward<Args>(args)...)) {
            if (code.get_code() != 0) {
                if (error_config_t::is_validation_enabled()) {
                    if (!error_registry_t::instance().is_registered(code)) {
                        payload["illegal_raw_code"] = std::to_string(code.get_code());
                        message = "[UNREGISTERED CODE] " + message;
                        this->code = error_builder_t::make_error_code(
                            error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFF);
                    }
                }

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
                if (error_config_t::is_stacktrace_enabled() &&
                    code.get_level() >= error_config_t::get_stacktrace_level()) {
                    stack_frames = utils::stack_trace_utils_t::generate(1);
                }
#endif

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
                if (error_config_t::is_source_location_enabled()) {
                    if (error_config_t::is_short_filename_enabled()) {
                        file_name = utils::extract_short_filename(code_with.source_location.file_name());
                    } else {
                        file_name = code_with.source_location.file_name();
                    }
                    function_name = code_with.source_location.function_name();
                    line_number = code_with.source_location.line();
                }
#endif

                if (code.get_code() != 0) {
                    notify_plugins(*this);
                }
            }
        }

        /**
         * @brief 检查错误上下文是否有效
         * @return bool 有效则返回true
         */
        explicit operator bool() const noexcept;

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
         * @details 优先使用传入的翻译器；未传入时自动尝试全局注册的翻译器；
         *          均无则降级输出可读英文名
         * @param translator 可选的翻译器接口指针，默认为 nullptr
         * @return std::string 错误上下文的字符串表示
         */
        std::string to_string(const i18n::i_translator_t* translator = nullptr) const noexcept;

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
    };
}  // namespace error_system::core