#pragma once
#include <cstdint>
#include <string>

#include "error_system/i18n/locale.h"

/**
 * @file i_subsystem_module_resolver.h
 * @brief 子系统/模块名称解析器抽象接口
 * @details 解耦 core 层（如 error_context_serializer_t）对 i18n::subsystem_module_catalog_t
 *          的具体依赖。core 层通过此接口按 locale 查询子系统和模块的本地化名称，
 *          i18n 层提供具体实现（如 subsystem_module_catalog_t）。
 *          本头文件仅包含数据结构与接口声明，不依赖 core 层类型。
 * @author yiice
 * @version 1.0.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::i18n {

    /**
     * @brief 子系统/模块名称映射
     * @details 通过 (subsystem_id << 16 | module_id) 作为 key 存储名称，避免每个错误码重复存储
     */
    struct subsystem_module_info_t {
        std::string subsystem_name{"未知子系统"};  ///< 子系统本地化名称
        std::string module_name{"未知模块"};       ///< 模块本地化名称
    };

    /**
     * @brief 子系统/模块名称解析器接口
     * @details 定义按 locale 查询子系统/模块名称的抽象接口。实现类必须保证 noexcept 安全，
     *          内部异常不应向外传播；未命中时返回默认名称。
     * @see subsystem_module_catalog_t
     */
    class i_subsystem_module_resolver_t {
    public:
        /**
         * @brief 虚析构函数
         * @details 确保通过基类指针正确释放派生类对象（规范 6）
         */
        virtual ~i_subsystem_module_resolver_t() noexcept = default;

        /**
         * @brief 按 locale 解析子系统/模块名称
         * @details 实现类应遵循：首选 locale → fallback locale → 默认名称 的回退顺序。
         * @param output_locale 首选语言区域
         * @param fallback_locale 回退语言区域
         * @param subsystem_id 子系统 ID
         * @param module_id 模块 ID
         * @return subsystem_module_info_t 子系统/模块名称信息副本
         */
        [[nodiscard]] virtual subsystem_module_info_t resolve_subsystem_module(
            locale_t output_locale,
            locale_t fallback_locale,
            uint16_t subsystem_id,
            uint16_t module_id) const noexcept = 0;
    };

}  // namespace error_system::i18n
