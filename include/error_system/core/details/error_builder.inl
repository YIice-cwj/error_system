#pragma once
#include "error_system/core/error_builder.h"

namespace error_system::core {

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
     */
    template <typename SubSystemEnum, typename ModuleEnum>
    inline constexpr error_code_t error_builder_t::make_error_code(error_level_t level,
                                                  domain::system_domain_t system,
                                                  SubSystemEnum subsystem,
                                                  ModuleEnum module,
                                                  uint16_t number) noexcept {
        return error_code_t{level,
                            system,
                            subsystem_id_t{static_cast<uint16_t>(subsystem)},
                            module_id_t{static_cast<uint16_t>(module)},
                            error_number_t{number}};
    }

}  // namespace error_system::core
