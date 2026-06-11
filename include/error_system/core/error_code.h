#pragma once
#include "error_system/core/error_level.h"
#include "error_system/domain/system_domain.h"
#include <cstdint>

/**
 * @file error_code.h
 * @brief 错误码数据类定义
 * @details 定义错误码数据结构、字段解析和访问接口，采用完全符合 C++ 标准的位移实现
 * @author yiice
 * @version 1.0.1
 * @date 2026-05-21
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
     * @details 封装64位错误码，提供字段解析和访问功能。基于位移操作实现，100% 避免严格别名规则与位域排布未定义行为。
     */
    class error_code_t {
        private:
        code_t code_{0};

        static constexpr uint32_t SIGN_SHIFT = 63;      // 符号位位移
        static constexpr uint32_t RESERVED_SHIFT = 60;  // 预留位位移
        static constexpr uint32_t LEVEL_SHIFT = 56;     // 错误等级位移
        static constexpr uint32_t SYSTEM_SHIFT = 48;    // 系统域位移
        static constexpr uint32_t SUBSYS_SHIFT = 32;    // 子系统位移
        static constexpr uint32_t MODULE_SHIFT = 16;    // 模块位移
        static constexpr uint32_t NUMBER_SHIFT = 0;     // 错误编号位移

        static constexpr uint64_t SIGN_MASK = 0x1ULL;       // 1 bit
        static constexpr uint64_t RESERVED_MASK = 0x7ULL;   // 3 bits
        static constexpr uint64_t LEVEL_MASK = 0xFULL;      // 4 bits
        static constexpr uint64_t SYSTEM_MASK = 0xFFULL;    // 8 bits
        static constexpr uint64_t SUBSYS_MASK = 0xFFFFULL;  // 16 bits
        static constexpr uint64_t MODULE_MASK = 0xFFFFULL;  // 16 bits
        static constexpr uint64_t NUMBER_MASK = 0xFFFFULL;  // 16 bits

        public:
        /**
         * @brief 默认构造函数
         */
        constexpr error_code_t() noexcept = default;

        /**
         * @brief 使用原始错误码构造
         * @param code 64位无符号整型错误码
         */
        constexpr explicit error_code_t(code_t code) noexcept : code_(code) {}

        /**
         * @brief 便捷构造函数（通过位域值构造完整错误码）
         * @details 直接传入 level、system、subsys、module、number 五个段，内部通过位运算组装。
         *          替代 error_builder_t::make_error_code()，减少中间层调用。
         * @param level 错误等级 (bits 59-56)
         * @param system 系统域 (bits 55-48)
         * @param subsys 子系统 ID (bits 47-32)
         * @param module 模块 ID (bits 31-16)
         * @param number 错误编号 (bits 15-0)
         *
         * @example
         * // 替代 error_builder_t::make_error_code(...)
         * error_code_t code(error_level_t::error, system_domain_t::database, 1, 2, 0x0010);
         */
        constexpr error_code_t(error_level_t level, domain::system_domain_t system,
                               uint16_t subsys, uint16_t module, uint16_t number) noexcept
            : code_((1ULL << SIGN_SHIFT)  // sign = 1 (错误)
                    | (static_cast<code_t>(level) << LEVEL_SHIFT)
                    | (static_cast<code_t>(system) << SYSTEM_SHIFT)
                    | (static_cast<code_t>(subsys) << SUBSYS_SHIFT)
                    | (static_cast<code_t>(module) << MODULE_SHIFT)
                    | (static_cast<code_t>(number) << NUMBER_SHIFT)) {}

        /**
         * @brief 获取原始错误码
         * @return code_t 64位原始错误码值
         */
        constexpr code_t get_code() const noexcept { return code_; }

        /**
         * @brief 获取符号位
         * @return uint8_t 符号位 (bit 63)
         */
        constexpr uint8_t get_sign() const noexcept { return static_cast<uint8_t>((code_ >> SIGN_SHIFT) & SIGN_MASK); }

        /**
         * @brief 设置符号位
         * @param sign 符号位值 (0 表示成功，1 表示错误)
         */
        constexpr void set_sign(uint8_t sign) noexcept {
            code_ = (code_ & ~SIGN_MASK) | (static_cast<code_t>(sign) << SIGN_SHIFT);
        }

        /**
         * @brief 获取预留位
         * @return uint8_t 预留位 (bits 62-60)
         */
        constexpr uint8_t get_reserved() const noexcept {
            return static_cast<uint8_t>((code_ >> RESERVED_SHIFT) & RESERVED_MASK);
        }

        /**
         * @brief 设置预留位
         * @param reserved 预留位值 (0-7)
         */
        constexpr void set_reserved(uint8_t reserved) noexcept {
            code_ = (code_ & ~RESERVED_MASK) | (static_cast<code_t>(reserved) << RESERVED_SHIFT);
        }

        /**
         * @brief 获取错误等级
         * @return error_level_t 错误等级 (bits 59-56)
         */
        constexpr error_level_t get_level() const noexcept {
            return static_cast<error_level_t>((code_ >> LEVEL_SHIFT) & LEVEL_MASK);
        }

        /**
         * @brief 获取系统域
         * @return domain::system_domain_t 系统域 (bits 55-48)
         */
        constexpr domain::system_domain_t get_system() const noexcept {
            return static_cast<domain::system_domain_t>((code_ >> SYSTEM_SHIFT) & SYSTEM_MASK);
        }

        /**
         * @brief 获取子系统值
         * @return uint16_t 子系统值 (bits 47-32)
         */
        constexpr uint16_t get_subsys() const noexcept {
            return static_cast<uint16_t>((code_ >> SUBSYS_SHIFT) & SUBSYS_MASK);
        }

        /**
         * @brief 获取模块值
         * @return uint16_t 模块值 (bits 31-16)
         */
        constexpr uint16_t get_module() const noexcept {
            return static_cast<uint16_t>((code_ >> MODULE_SHIFT) & MODULE_MASK);
        }

        /**
         * @brief 获取错误编号
         * @return uint16_t 错误编号 (bits 15-0)
         */
        constexpr uint16_t get_number() const noexcept {
            return static_cast<uint16_t>((code_ >> NUMBER_SHIFT) & NUMBER_MASK);
        }

        /**
         * @brief 获取模块的聚合隔离 ID
         * @details 直接通过位掩码，高8位(Sign+Reserved+Level)和低16位(Number)置零，只保留系统与模块信息
         * @return uint64_t 模块的聚合隔离 ID
         */
        constexpr module_group_id_t get_module_group_id() const noexcept { return code_ & 0x0000FFFFFFFF0000ULL; }

        /**
         * @brief 获取清除符号位和预留位后的错误码
         * @details 返回符号位(bit63)和预留位(bits62-60)被置0后的错误码值，用于注册和查询时统一忽略这些差异
         * @return code_t 清除符号位和预留位后的64位错误码值
         */
        constexpr code_t get_identity_code() const noexcept {
            return code_ & ~((SIGN_MASK << SIGN_SHIFT) | (RESERVED_MASK << RESERVED_SHIFT));
        }

        /**
         * @brief 类型转换运算符
         * @details 支持将隐式或显式地将 error_code_t 转换为 64位整型 code_t
         */
        constexpr operator code_t() const noexcept { return code_; }

        /**
         * @brief 相等比较运算符
         * @param other 另一个错误码
         * @return bool 相等返回 true
         */
        constexpr bool operator==(const error_code_t& other) const noexcept { return code_ == other.code_; }

        /**
         * @brief 不等比较运算符
         * @param other 另一个错误码
         * @return bool 不等返回 true
         */
        constexpr bool operator!=(const error_code_t& other) const noexcept { return code_ != other.code_; }

        /**
         * @brief 小于比较运算符
         * @param other 另一个错误码
         * @return bool 小于返回 true
         */
        constexpr bool operator<(const error_code_t& other) const noexcept { return code_ < other.code_; }
    };

}  // namespace error_system::core