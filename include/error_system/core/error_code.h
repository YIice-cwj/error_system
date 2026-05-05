#pragma once
#include "error_system/core/error_level.h"
#include "error_system/domain/system_domain.h"
#include <cstdint>

/**
 * @file error_code.h
 * @brief 错误码数据类定义
 * @details 定义错误码数据结构、字段解析和访问接口
 * @author yiice
 * @version 1.0.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误码类型
     * @details 使用64位无符号整型表示错误码
     */
    using code_t = uint64_t;

    /**
     * @brief 模块组 ID 类型
     * @details 用于标识一个模块组（系统 + 子系统 + 模块）
     */
    using module_group_id_t = uint64_t;

    /**
     * @brief 错误码数据类
     * @details 封装64位错误码，提供字段解析和访问功能
     */
    class error_code_t {
        public:
        /**
         * @brief 错误码字段结构体
         * @details 按位域分解64位错误码的各个组成部分
         */
        struct fields_t {
            uint8_t sign : 1;                    // 符号位 (bit 63)
            uint8_t reserved : 3;                // 预留位 (bits 62-60)
            error_level_t level : 4;             // 错误等级 (bits 59-56)
            domain::system_domain_t system : 8;  // 系统域 (bits 55-48)
            uint16_t subsys : 16;                // 子系统 (bits 47-32)
            uint16_t module : 16;                // 模块 (bits 31-16)
            uint16_t number : 16;                // 错误编号 (bits 15-0)
        };

        /**
         * @brief 错误码联合体
         * @details 支持以整体或字段方式访问错误码
         */
        union error_code_union_t {
            code_t code;
            fields_t fields;
        };

        private:
        error_code_union_t union_;

        public:
        /**
         * @brief 默认构造函数
         * @details 构造一个成功码（code = 0）
         */
        constexpr error_code_t() noexcept : union_{0} {}

        /**
         * @brief 从原始码构造
         * @param code 64位错误码
         */
        constexpr explicit error_code_t(code_t code) noexcept : union_{code} {}

        /**
         * @brief 获取原始错误码
         * @return code_t 64位错误码
         */
        constexpr code_t get_code() const noexcept { return union_.code; }

        /**
         * @brief 获取符号位
         * @return uint8_t 符号位 (0=成功, 1=错误)
         */
        constexpr uint8_t get_sign() const noexcept { return union_.fields.sign; }

        /**
         * @brief 获取预留位
         * @return uint8_t 预留位
         */
        constexpr uint8_t get_reserved() const noexcept { return union_.fields.reserved; }

        /**
         * @brief 获取错误等级
         * @return error_level_t 错误等级
         */
        constexpr error_level_t get_level() const noexcept { return union_.fields.level; }

        /**
         * @brief 获取系统域
         * @return domain::system_domain_t 系统域
         */
        constexpr domain::system_domain_t get_system() const noexcept { return union_.fields.system; }

        /**
         * @brief 获取子系统值
         * @return uint16_t 子系统值
         */
        constexpr uint16_t get_subsys() const noexcept { return union_.fields.subsys; }

        /**
         * @brief 获取模块值
         * @return uint16_t 模块值
         */
        constexpr uint16_t get_module() const noexcept { return union_.fields.module; }

        /**
         * @brief 获取错误编号
         * @return uint16_t 错误编号
         */
        constexpr uint16_t get_number() const noexcept { return union_.fields.number; }

        /**
         * @brief 获取模块的聚合隔离 ID
         * @details 直接通过位掩码，高8位(Level)和低16位(Number)置零，只保留中间的系统与模块信息
         * @return uint64_t 模块的聚合隔离 ID
         */
        constexpr module_group_id_t get_module_group_id() const noexcept {
            return union_.code & 0x00FFFFFFFFFF0000ULL;
        }

        /**
         * @brief 隐式转换为原始码
         * @return code_t 64位错误码
         */
        constexpr operator code_t() const noexcept { return union_.code; }
    };

}  // namespace error_system::core
