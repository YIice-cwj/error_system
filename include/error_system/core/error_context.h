#pragma once
#include "error_system/core/error_code.h"
#include "error_system/core/error_builder.h"
#include "error_system/core/error_registry.h"
#include "error_system/i18n/translator_registry.h"
#include "error_system/utils/string_utils.h"
#include "error_system/memory/object_pool.h"
#include "error_system/utils/stack_trace_utils.h"
#include "error_system/core/error_config.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace error_system::core {
    class plugin_registry_t;
}  // forward declaration

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
    void __notify_plugins(const error_context_t& context) noexcept;

    /**
     * @brief 错误上下文数据类
     * @details 封装错误上下文信息，提供字段解析和访问功能
     */
    struct error_context_t {
        using object_pool_t = memory::object_pool_t<error_context_t, 128>;
        
        error_code_t code{};
        std::string message{};
        std::unordered_map<std::string, std::string> payload{};
        std::vector<std::string> stack_frames{};
        std::shared_ptr<error_context_t> cause{nullptr};

        constexpr error_context_t() noexcept = default;

        /**
         * @brief 构造函数
         * @param code 错误码
         * @param message 错误信息
         */
        template<typename... Args>
        error_context_t(error_code_t code, std::string format = "", Args&&... args) noexcept
            : code(code), message(utils::string_utils_t::format(format, std::forward<Args>(args)...)) {
            if (code.get_code() != 0) {
                if (error_config::is_validation_enabled()) {
                    if (!error_registry_t::instance().is_registered(code)) {
                        payload["illegal_raw_code"] = std::to_string(code.get_code());
                        message = "[UNREGISTERED CODE] " + message;
                        this->code = error_builder_t::make_error_code(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFF);
                    }
                }

                if (error_config::is_stacktrace_enabled() && code.get_level() >= error_config::get_stacktrace_level()) {
                    stack_frames = utils::stack_trace_utils_t::generate(1);
                }
            
                if (code.get_code() != 0) {
                    __notify_plugins(*this);
                }
            }
        }

        /**
         * @brief 检查错误上下文是否有效
         * @return bool 有效则返回true
         */
        explicit operator bool() const noexcept { return code.get_code() != 0; }

        /**
         * @brief 包装底层错误上下文
         * @param underlying 底层错误上下文
         * @return error_context_t 包装后的错误上下文
         */
        error_context_t wrap(const error_context_t& underlying) const {
            error_context_t new_code_context = *this;

            object_pool_t& object_pool = object_pool_t::instance_thread_local();
            error_context_t* context_pointer = object_pool.acquire();
            if (context_pointer) {
                *context_pointer = underlying;
                new_code_context.cause = std::shared_ptr<error_context_t>(
                    context_pointer, 
                    [&object_pool](error_context_t* context_pointer) -> void { 
                        object_pool.release(context_pointer);
                    }
                );
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
        error_context_t wrap(error_context_t&& underlying) const {
            error_context_t new_code_context = *this;

            object_pool_t& object_pool = object_pool_t::instance_thread_local();
            error_context_t* context_pointer = object_pool.acquire();
            if (context_pointer) {
                *context_pointer = std::move(underlying);
                new_code_context.cause = std::shared_ptr<error_context_t>(
                    context_pointer, 
                    [&object_pool](error_context_t* context_pointer) -> void { 
                        object_pool.release(context_pointer);
                    }
                );
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
        error_context_t& with(const std::string& key, const std::string& value) {
            payload[key] = value;
            return *this;
        }

        /**
         * @brief 添加字段
         * @param key 字段名
         * @param val 字段值
         * @return error_context_t& 当前错误上下文的引用
         */
        error_context_t& with(std::string&& key, std::string&& value) {
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
        std::string to_string(const i18n::i_translator_t* translator = nullptr) const {
            if (!translator) {
                translator = i18n::translator_registry_t::instance().get();
            }

            std::string result = utils::string_utils_t::format("{} - {}", translator->translate(code), message);
            
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

            if (!stack_frames.empty()) {
                result += "\n  [Stacktrace]:";
                for (size_t i = 0; i < stack_frames.size(); ++i) {
                    result += "\n    #" + std::to_string(i) + "  " + stack_frames[i];
                }
            }

            if (cause) {
                result += "\n  ↳ Caused by: " + cause->to_string(translator);
            }
            return result;
        }
    };

}  // namespace error_system::core