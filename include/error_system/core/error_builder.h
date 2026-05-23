#pragma once
#include "error_system/core/error_code.h"
#include <cstdint>

/**
 * @file error_builder.h
 * @brief 错误码构建器
 * @details 提供构建错误码的工厂方法，支持通过位移操作安全拼接各个字段
 * @author yiice
 * @version 1.0.1
 * @date 2026-05-21
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误码构建器
     * @details 提供静态工厂方法构建错误码，保证编译期安全与跨平台确定性
     */
    class error_builder_t {
        public:
        /**
         * @brief 构建错误码（默认 sign=1，表示错误，保留位固定为 0 (bits 62-63)）
         * @details 兼容旧接口，通过枚举类型构建错误码，内部自动安全转换为 uint16_t
         * @tparam SubSystemEnum 子系统枚举类型
         * @tparam ModuleEnum 模块枚举类型
         * @param level 错误等级
         * @param system 系统域
         * @param subsys 子系统值（枚举）
         * @param module 模块值（枚举）
         * @param number 错误编号
         * @return error_code_t 错误码对象
         */
        template <typename SubSystemEnum, typename ModuleEnum>
        static constexpr error_code_t make_error_code(error_level_t level,
                                                      domain::system_domain_t system,
                                                      SubSystemEnum subsys,
                                                      ModuleEnum module,
                                                      uint16_t number) noexcept {
            return make_error_code(
                1, 0, level, system, static_cast<uint16_t>(subsys), static_cast<uint16_t>(module), number);
        }

        /**
         * @brief 构建错误码（默认 sign=1，表示错误，保留位固定为 0 (bits 62-63)）
         * @details 兼容旧接口，通过原始值构建错误码
         * @param level 错误等级
         * @param system 系统域
         * @param subsys 子系统值
         * @param module 模块值
         * @param number 错误编号
         * @return error_code_t 错误码对象
         */
        static constexpr error_code_t make_error_code(error_level_t level,
                                                      domain::system_domain_t system,
                                                      uint16_t subsys,
                                                      uint16_t module,
                                                      uint16_t number) noexcept {
            return make_error_code(1, 0, level, system, subsys, module, number);
        }

        /**
         * @brief 构建错误码（可自定义符号位和保留位，固定为 0 (bits 62-63)）
         * @details 通过枚举类型构建错误码，内部自动安全转换为 uint16_t
         * @tparam SubSystemEnum 子系统枚举类型
         * @tparam ModuleEnum 模块枚举类型
         * @param sign 符号位，0 代表无错，1 代表错误
         * @param reserved 保留位，固定为 0 (bits 62-63)
         * @param level 错误等级
         * @param system 系统域
         * @param subsys 子系统值（枚举）
         * @param module 模块值（枚举）
         * @param number 错误编号
         * @return error_code_t 错误码对象
         */
        template <typename SubSystemEnum, typename ModuleEnum>
        static constexpr error_code_t make_error_code(uint8_t sign,
                                                      uint8_t reserved,
                                                      error_level_t level,
                                                      domain::system_domain_t system,
                                                      SubSystemEnum subsys,
                                                      ModuleEnum module,
                                                      uint16_t number) noexcept {
            return make_error_code(
                sign, reserved, level, system, static_cast<uint16_t>(subsys), static_cast<uint16_t>(module), number);
        }

        /**
         * @brief 构建错误码（可自定义符号位和保留位，固定为 0 (bits 62-63)）
         * @details 通过原始值构建错误码。利用明确的左移和位或操作，按预定比特位安全拼接 64 位错误码。
         * 拼接规则：
         * - Sign: bit 63 (0 代表无错，1 代表错误)
         * - Reserved: bits 62-60 (固定为 0)
         * - Level: bits 59-56
         * - System: bits 55-48
         * - Subsystem: bits 47-32
         * - Module: bits 31-16
         * - Number: bits 15-0
         * @param sign 符号位，0 代表无错，1 代表错误
         * @param reserved 保留位，固定为 0 (bits 62-63)
         * @param level 错误等级
         * @param system 系统域
         * @param subsys 子系统值
         * @param module 模块值
         * @param number 错误编号
         * @return error_code_t 错误码对象
         */
        static constexpr error_code_t make_error_code(uint8_t sign,
                                                      uint8_t reserved,
                                                      error_level_t level,
                                                      domain::system_domain_t system,
                                                      uint16_t subsys,
                                                      uint16_t module,
                                                      uint16_t number) noexcept {

            return error_code_t((static_cast<code_t>(sign) << 63) | (static_cast<code_t>(reserved) << 60) |
                                (static_cast<code_t>(level) << 56) | (static_cast<code_t>(system) << 48) |
                                (static_cast<code_t>(subsys) << 32) | (static_cast<code_t>(module) << 16) |
                                (static_cast<code_t>(number) << 0));
        }

        /**
         * @brief 从原始码构建错误码
         * @param code 64位错误码
         * @return error_code_t 错误码对象
         */
        static constexpr error_code_t make_error_code(code_t code) noexcept { return error_code_t(code); }
    };

}  // namespace error_system::core