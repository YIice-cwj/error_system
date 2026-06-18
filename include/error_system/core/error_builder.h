#pragma once
#include <cstdint>

#include "error_system/core/error_code.h"

/**
 * @file error_builder.h
 * @brief 错误码构建器
 * @details 提供编译期类型安全的错误码构造，支持枚举类型参数防止 ID 传反
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-11
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误码构建器
     * @details 保留两个有独特价值的工厂方法：
     *          - 枚举模板版本：编译期类型安全，防止 subsystem/module ID 传反
     *          - from_raw：从持久化/网络恢复原始码时语义更清晰
     *          其他场景使用 error_code_t 便捷构造函数
     */
    class error_builder_t {
        public:
        error_builder_t() = delete;

        /**
         * @brief 通过枚举类型构造错误码（编译期类型安全）
         * @details 相比 error_code_t 构造函数直接传 uint16_t，使用强类型枚举可防止
         *          subsystem_id 和 module_id 传反。内部通过 static_cast 转换为 uint16_t
         *          后委托 error_code_t 构造函数。
         * @tparam SubSystemEnum 子系统枚举类型
         * @tparam ModuleEnum 模块枚举类型
         * @param level 错误等级
         * @param system 系统域
         * @param subsys 子系统值（枚举）
         * @param module 模块值（枚举）
         * @param number 错误编号
         * @return error_code_t 错误码对象
         *
         * @example
         * enum class subsys_t : uint16_t { db_conn = 1 };
         * enum class module_t : uint16_t { timeout = 2 };
         * auto code = error_builder_t::make_error_code(
         *     error_level_t::error, system_domain_t::database,
         *     subsys_t::db_conn, module_t::timeout, 0x0001);
         */
        template <typename SubSystemEnum, typename ModuleEnum>
        static constexpr error_code_t make_error_code(error_level_t level,
                                                      domain::system_domain_t system,
                                                      SubSystemEnum subsys,
                                                      ModuleEnum module,
                                                      uint16_t number) noexcept {
            return error_code_t{level, system, static_cast<uint16_t>(subsys), static_cast<uint16_t>(module), number};
        }

        /**
         * @brief 从原始 64 位码值恢复错误码
         * @details 用于从网络传输、持久化存储或二进制协议中恢复错误码。
         *          与 error_code_t(code_t) 构造函数等价，但语义上明确表达"反序列化"意图。
         * @param code 64 位原始错误码
         * @return error_code_t 错误码对象
         *
         * @example
         * code_t raw = recv_from_network();
         * error_code_t code = error_builder_t::from_raw(raw);
         */
        static constexpr error_code_t from_raw(code_t code) noexcept { return error_code_t(code); }
    };

}  // namespace error_system::core