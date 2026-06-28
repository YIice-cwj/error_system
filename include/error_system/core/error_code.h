#pragma once
#include <cstdint>

#include "error_system/core/error_level.h"
#include "error_system/domain/system_domain.h"

/**
 * @file error_code.h
 * @brief 错误码数据类定义
 * @details 定义错误码数据结构、字段解析和访问接口，采用完全符合 C++ 标准的位移实现
 * @author yiice
 * @version 2.3.0
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
        static constexpr uint32_t RESERVED_SHIFT = 60;  // 预留位位移（含 retryable/transient 语义）
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

        /**
         * @brief Reserved 字段内 retryable 标志位位移（相对 RESERVED_SHIFT 的 bit 偏移）
         * @details Reserved 3 bits 布局：bit0 = retryable, bit1 = transient, bit2 = reserved
         *          - retryable=1 表示该错误可重试（如网络抖动、临时限流）
         *          - transient=1 表示该错误是瞬态的（可能自动恢复）
         *          默认均为 0（不可重试 / 非瞬态），与历史行为兼容。
         */
        static constexpr uint32_t RETRYABLE_BIT = 0;
        static constexpr uint32_t TRANSIENT_BIT = 1;
        static constexpr uint64_t RETRYABLE_MASK = 0x1ULL;
        static constexpr uint64_t TRANSIENT_MASK = 0x2ULL;

    public:
        /**
         * @brief 默认构造函数
         * @details 默认构造为成功码（sign=1），所有字段为 0
         */
        constexpr error_code_t() noexcept : code_(1ULL << SIGN_SHIFT) {}
        constexpr error_code_t(const error_code_t&) noexcept = default;
        constexpr error_code_t(error_code_t&&) noexcept = default;
        constexpr error_code_t& operator=(const error_code_t&) noexcept = default;
        constexpr error_code_t& operator=(error_code_t&&) noexcept = default;

        ~error_code_t() noexcept = default;

        /**
         * @brief 创建成功码的工厂方法
         * @details 语义清晰的构造成功码方式，等价于默认构造函数。
         *          适用于返回默认成功状态的场景。
         * @return error_code_t sign=1 的成功码
         *
         * @example
         * return error_code_t::make_success();
         */
        static constexpr error_code_t make_success() noexcept { return error_code_t{}; }

        /**
         * @brief 使用原始错误码构造
         * @param code 64位无符号整型错误码
         */
        constexpr explicit error_code_t(code_t code) noexcept : code_(code) {}

        /**
         * @brief 便捷构造函数（通过位域值构造错误码，sign=0 表示错误）
         * @details 直接传入 level、system、subsys、module、number 五个段，内部通过位运算组装。
         *          sign 位默认为 0（false = 错误），符合计算机 0=false/1=true 语义。
         * @param level 错误等级 (bits 59-56)
         * @param system 系统域 (bits 55-48)
         * @param subsystem 子系统 ID (bits 47-32)
         * @param module 模块 ID (bits 31-16)
         * @param number 错误编号 (bits 15-0)
         *
         * @example
         * error_code_t code(error_level_t::error, system_domain_t::database, 1, 2, 0x0010);
         */
        constexpr error_code_t(error_level_t level, domain::system_domain_t system,
                               uint16_t subsystem, uint16_t module, uint16_t number) noexcept
            : code_((static_cast<code_t>(level) << LEVEL_SHIFT)
                    | (static_cast<code_t>(system) << SYSTEM_SHIFT)
                    | (static_cast<code_t>(subsystem) << SUBSYS_SHIFT)
                    | (static_cast<code_t>(module) << MODULE_SHIFT)
                    | (static_cast<code_t>(number) << NUMBER_SHIFT)) {}

        /**
         * @brief 获取原始错误码
         * @return code_t 64位原始错误码值
         */
        [[nodiscard]] constexpr code_t get_code() const noexcept { return code_; }

        /**
         * @brief 判断错误码是否表示错误
         * @return bool sign=0（false）时为错误，sign=1（true）时为成功
         */
        [[nodiscard]] constexpr bool is_error_code() const noexcept { return get_sign() == 0; }

        /**
         * @brief 判断错误码是否表示成功
         * @return bool sign=1（true）时为成功
         */
        [[nodiscard]] constexpr bool is_success_code() const noexcept { return get_sign() == 1; }

        /**
         * @brief 获取符号位
         * @return uint8_t 符号位 (bit 63)，0 = 错误(false)，1 = 成功(true)
         */
        [[nodiscard]] constexpr uint8_t get_sign() const noexcept { return static_cast<uint8_t>((code_ >> SIGN_SHIFT) & SIGN_MASK); }

        /**
         * @brief 获取预留位
         * @return uint8_t 预留位 (bits 62-60)
         */
        [[nodiscard]] constexpr uint8_t get_reserved() const noexcept {
            return static_cast<uint8_t>((code_ >> RESERVED_SHIFT) & RESERVED_MASK);
        }

        /**
         * @brief 设置符号位
         * @details 仅接受 0/1，超范围值视为 0（错误），避免污染其他位
         * @param sign 符号位值 (0 = 错误，1 = 成功)
         */
        constexpr void set_sign(uint8_t sign) noexcept {
            const code_t sign_value = (sign <= 1) ? static_cast<code_t>(sign) : 0ULL;
            code_ = (code_ & ~(SIGN_MASK << SIGN_SHIFT)) | (sign_value << SIGN_SHIFT);
        }

        /**
         * @brief 设置预留位
         * @details 仅接受 0-7，超范围值视为 0，避免污染其他位
         * @param reserved 预留位值 (0-7)
         */
        constexpr void set_reserved(uint8_t reserved) noexcept {
            const code_t reserved_value = (reserved <= 7) ? static_cast<code_t>(reserved) : 0ULL;
            code_ = (code_ & ~(RESERVED_MASK << RESERVED_SHIFT)) | (reserved_value << RESERVED_SHIFT);
        }

        /**
         * @brief 查询错误是否可重试
         * @details 读取 Reserved.bit0。retryable=1 表示业务可对该错误执行重试逻辑
         *          （如网络抖动、临时限流、leader 切换等）。
         *          默认 0（不可重试），与历史行为兼容。
         * @return bool 是否可重试
         */
        [[nodiscard]] constexpr bool is_retryable() const noexcept {
            return ((code_ >> RESERVED_SHIFT) & RETRYABLE_MASK) != 0;
        }

        /**
         * @brief 设置错误是否可重试
         * @param retryable true=可重试，false=不可重试
         */
        constexpr void set_retryable(bool retryable) noexcept {
            const code_t bit = retryable ? RETRYABLE_MASK : 0ULL;
            code_ = (code_ & ~(RETRYABLE_MASK << RESERVED_SHIFT)) | (bit << RESERVED_SHIFT);
        }

        /**
         * @brief 查询错误是否为瞬态
         * @details 读取 Reserved.bit1。transient=1 表示该错误是瞬态的，
         *          可能在短时间内自动恢复（与 retryable 通常同时为 true）。
         *          默认 0（非瞬态）。
         * @return bool 是否瞬态
         */
        [[nodiscard]] constexpr bool is_transient() const noexcept {
            return ((code_ >> RESERVED_SHIFT) & TRANSIENT_MASK) != 0;
        }

        /**
         * @brief 设置错误是否为瞬态
         * @param transient true=瞬态，false=非瞬态
         */
        constexpr void set_transient(bool transient) noexcept {
            const code_t bit = transient ? TRANSIENT_MASK : 0ULL;
            code_ = (code_ & ~(TRANSIENT_MASK << RESERVED_SHIFT)) | (bit << RESERVED_SHIFT);
        }

        /**
         * @brief 获取错误等级
         * @details 通过 from_int 校验，非法值（5-15）回退为 fatal，避免下游越界
         * @return error_level_t 错误等级 (bits 59-56)
         */
        [[nodiscard]] constexpr error_level_t get_level() const noexcept {
            return from_int(static_cast<uint8_t>((code_ >> LEVEL_SHIFT) & LEVEL_MASK));
        }

        /**
         * @brief 获取系统域
         * @details 通过 from_int 校验，非法值回退为 none，避免下游越界
         * @return domain::system_domain_t 系统域 (bits 55-48)
         */
        [[nodiscard]] constexpr domain::system_domain_t get_system() const noexcept {
            return domain::from_int(static_cast<uint8_t>((code_ >> SYSTEM_SHIFT) & SYSTEM_MASK));
        }

        /**
         * @brief 获取子系统值
         * @return uint16_t 子系统值 (bits 47-32)
         */
        [[nodiscard]] constexpr uint16_t get_subsys() const noexcept {
            return static_cast<uint16_t>((code_ >> SUBSYS_SHIFT) & SUBSYS_MASK);
        }

        /**
         * @brief 获取模块值
         * @return uint16_t 模块值 (bits 31-16)
         */
        [[nodiscard]] constexpr uint16_t get_module() const noexcept {
            return static_cast<uint16_t>((code_ >> MODULE_SHIFT) & MODULE_MASK);
        }

        /**
         * @brief 获取错误编号
         * @return uint16_t 错误编号 (bits 15-0)
         */
        [[nodiscard]] constexpr uint16_t get_number() const noexcept {
            return static_cast<uint16_t>((code_ >> NUMBER_SHIFT) & NUMBER_MASK);
        }

        /**
         * @brief 获取模块组ID（系统域+子系统+模块）
         * @details 高8位(Sign+Reserved+Level)和低16位(Number)置零，保留系统域(8位)、子系统(16位)和模块(16位)
         * @return uint64_t 模块的聚合隔离 ID
         */
        [[nodiscard]] constexpr module_group_id_t get_module_group_id() const noexcept { return code_ & 0x00FFFFFFFFFF0000ULL; }

        /**
         * @brief 获取清除符号位和预留位后的错误码
         * @details 返回符号位(bit63)和预留位(bits62-60)被置0后的错误码值，用于注册和查询时统一忽略这些差异
         * @return code_t 清除符号位和预留位后的64位错误码值
         */
        [[nodiscard]] constexpr code_t get_identity_code() const noexcept {
            return code_ & ~((SIGN_MASK << SIGN_SHIFT) | (RESERVED_MASK << RESERVED_SHIFT));
        }

        /**
         * @brief 类型转换运算符
         * @details 支持显式地将 error_code_t 转换为 64位整型 code_t。
         *          标记 explicit 防止意外隐式转换(如 if(error_code) 的布尔上下文误用)。
         */
        explicit constexpr operator code_t() const noexcept { return code_; }

        /**
         * @brief 相等比较运算符
         * @param other 另一个错误码
         * @return bool 相等返回 true
         */
        [[nodiscard]] constexpr bool operator==(const error_code_t& other) const noexcept { return code_ == other.code_; }

        /**
         * @brief 不等比较运算符
         * @param other 另一个错误码
         * @return bool 不等返回 true
         */
        [[nodiscard]] constexpr bool operator!=(const error_code_t& other) const noexcept { return code_ != other.code_; }

        /**
         * @brief 小于比较运算符
         * @param other 另一个错误码
         * @return bool 小于返回 true
         */
        [[nodiscard]] constexpr bool operator<(const error_code_t& other) const noexcept { return code_ < other.code_; }
    };

}  // namespace error_system::core