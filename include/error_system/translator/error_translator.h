#pragma once
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

/**
 * @file error_translator.h
 * @brief 错误翻译器
 * @details 定义错误翻译器，用于将子系统和模块翻译为可读的字符串
 * @author yiice
 * @version 1.0.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::translator {

    class error_translator_t {
        private:
        std::unordered_map<uint16_t, std::string> dynamic_subsystems_;

        std::unordered_map<uint32_t, std::string> dynamic_modules_;

        mutable std::shared_mutex rw_mutex_;

        private:
        error_translator_t() = default;

        error_translator_t& operator=(const error_translator_t&) = delete;

        error_translator_t& operator=(error_translator_t&&) = delete;

        error_translator_t(const error_translator_t&) = delete;

        error_translator_t(error_translator_t&&) = delete;

        public:
        ~error_translator_t() = default;

        /**
         * @brief 注册子系统
         * @details 注册子系统，用于将子系统ID转换为可读的字符串
         * @param subsys_id 子系统ID
         * @param name 子系统名称
         */
        void register_subsystem(uint16_t subsys_id, std::string name) noexcept {
            std::unique_lock lock(rw_mutex_);
            dynamic_subsystems_[subsys_id] = std::move(name);
        }

        /**
         * @brief 注册模块
         * @details 注册模块，用于将模块ID转换为可读的字符串
         * @param subsys_id 子系统ID
         * @param module_id 模块ID
         * @param name 模块名称
         */
        void register_module(uint16_t subsys_id, uint16_t module_id, std::string name) noexcept {
            uint32_t key = (static_cast<uint32_t>(subsys_id) << 16) | module_id;
            std::unique_lock lock(rw_mutex_);
            dynamic_modules_[key] = std::move(name);
        }

        /**
         * @brief 翻译错误系统ID和模块ID为可读的字符串
         * @details 翻译错误系统ID和模块ID为可读的字符串，用于显示错误信息
         * @param subsys_id 子系统ID
         * @param module_id 模块ID
         * @return std::string 可读的错误信息
         */
        std::string translate(uint16_t subsys_id, uint16_t module_id) const noexcept;

        public:
        /**
         * @brief 获取错误翻译器实例
         * @details 获取错误翻译器实例，用于在其他地方调用翻译器
         * @return error_translator_t& 错误翻译器实例
         */
        static error_translator_t& instance() noexcept {
            static error_translator_t instance;
            return instance;
        }
    };

    /**
     * @brief 获取静态子系统名称
     * @details 获取静态子系统名称，用于显示错误信息
     * @param subsys_id 子系统ID
     * @return std::string名称
     */
    std::string_view get_static_subsys_name(uint16_t subsys_id) noexcept;

    /**
     * @brief 获取静态模块名称
     * @details 获取静态模块名称，用于显示错误信息
     * @param subsys_id 子系统ID
     * @param module_id 模块ID
     * @return std::string名称
     */
    std::string_view get_static_module_name(uint16_t subsys_id, uint16_t module_id) noexcept;
}  // namespace error_system::translator

/**
 * @brief 注册子系统和模块描述
 * @param SUBSYS 子系统 ID
 * @param MODULE 模块 ID
 * @param SUBSYS_NAME 子系统名称
 * @param MODULE_NAME 模块名称
 * @details 该宏会：
 *          1. 注册子系统和模块描述
 *          2. 在静态初始化阶段自动将子系统和模块描述注册到 error_translator_t
 * @example
 * REGISTER_LEGACY_MODULE(
 *     1, 1, "支付服务", "支付网关")
 */
#define REGISTER_LEGACY_MODULE(SUBSYS_ID, MODULE_ID, SUBSYS_NAME, MODULE_NAME)                                         \
    do {                                                                                                               \
        error_system::translator::error_translator_t::instance().register_subsystem(SUBSYS_ID, SUBSYS_NAME);           \
        error_system::translator::error_translator_t::instance().register_module(SUBSYS_ID, MODULE_ID, MODULE_NAME);   \
    } while (0)

/**
 * @brief 注册错误码
 * @param code_obj 错误码对象
 * @param err_desc 错误描述
 * @details 注册错误码，用于将错误码转换为可读的字符串，用于错误信息显示
 * @example
 * REGISTER_LEGACY_ERROR(
 *     code, "支付失败")
 */
#define REGISTER_LEGACY_ERROR(code_obj, err_desc)                                                                      \
    do {                                                                                                               \
        /* 2. 注册到你原有的错误码中心（具体错误描述） */                                                              \
        error_system::core::error_registry_t::instance().register_error((code_obj), err_desc);                         \
    } while (0)