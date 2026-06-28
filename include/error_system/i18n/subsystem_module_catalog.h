#pragma once
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "error_system/i18n/locale.h"

/**
 * @file subsystem_module_catalog.h
 * @brief 子系统/模块名称多语言目录
 * @details 维护 (subsystem_id, module_id) → (locale → 名称) 的两级映射，
 *          支持按 locale 查询子系统/模块名称，未命中时回退到默认 locale。
 *
 *          设计动机：原 subsystem_module_registry_t 仅支持单语言（中文硬编码），
 *          合并到 i18n 模块后，子系统/模块名称也支持多语言，与错误码消息共用 locale_t 枚举。
 *
 *          本类不持有 locale 状态（active/default），由调用方（如 error_context_serializer）
 *          从 i18n_t::instance() 获取 locale 后显式传入查询接口，避免双源 locale 不同步。
 *
 *          线程安全（读多写少场景使用 shared_mutex）。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::i18n {

    /**
     * @brief 子系统/模块名称映射
     * @details 通过 (subsystem_id << 16 | module_id) 作为 key 存储名称，避免每个错误码重复存储
     */
    struct subsystem_module_info_t {
        std::string subsystem_name{"未知子系统"};
        std::string module_name{"未知模块"};
    };

    /**
     * @brief 子系统/模块名称多语言目录
     * @details 维护 (subsystem_id, module_id) → (locale → subsystem_module_info_t) 的两级映射表，
     *          用于 to_string() 等可读输出场景。线程安全（自带读写锁）。
     *
     *          locale 来源：本类不持有 locale 状态，查询时由调用方显式传入 locale，
     *          通常从 i18n_t::instance().get_active_locale() / get_default_locale() 获取，
     *          保证子系统/模块名称与错误码消息使用同一 locale。
     *
     * @example
     * @code
     * // 注册中文（默认）
     * catalog.register_subsystem_module(locale_t::zh_CN, 101, 1, "交易服务", "订单模块");
     * // 注册英文
     * catalog.register_subsystem_module(locale_t::en_US, 101, 1, "Trade Service", "Order Module");
     *
     * // 查询：显式 locale + 回退 locale
     * auto info = catalog.get_subsystem_module_info(locale_t::en_US, locale_t::zh_CN, 101, 1);
     * @endcode
     */
    class subsystem_module_catalog_t {
    private:
        /**
         * @brief (subsystem_id, module_id) → (locale → 名称) 两级映射
         * @details 外层 key = (subsystem_id << 16) | module_id，内层 key = locale_t
         */
        std::unordered_map<uint32_t, std::unordered_map<locale_t, subsystem_module_info_t>> catalog_;

        /**
         * @brief 映射表互斥锁，保护并发访问
         */
        mutable std::shared_mutex mutex_;

    public:
        subsystem_module_catalog_t() = default;

        ~subsystem_module_catalog_t() noexcept = default;

        subsystem_module_catalog_t(const subsystem_module_catalog_t&) = delete;

        subsystem_module_catalog_t& operator=(const subsystem_module_catalog_t&) = delete;

        subsystem_module_catalog_t(subsystem_module_catalog_t&&) = delete;

        subsystem_module_catalog_t& operator=(subsystem_module_catalog_t&&) = delete;

        /**
         * @brief 注册指定 locale 的子系统/模块名称
         * @param locale 语言区域
         * @param subsystem_id 子系统 ID
         * @param module_id 模块 ID
         * @param subsystem_name 子系统名称
         * @param module_name 模块名称
         * @note 若 (subsystem_id, module_id, locale) 已存在则静默跳过（保留首次注册）
         */
        void register_subsystem_module(locale_t locale,
                                       uint16_t subsystem_id,
                                       uint16_t module_id,
                                       std::string_view subsystem_name,
                                       std::string_view module_name) noexcept;

        /**
         * @brief 注册子系统/模块名称（便捷重载，默认 locale 为 zh_CN）
         * @param subsystem_id 子系统 ID
         * @param module_id 模块 ID
         * @param subsystem_name 子系统名称
         * @param module_name 模块名称
         * @note 等价于 register_subsystem_module(locale_t::zh_CN, ...)
         */
        void register_subsystem_module(uint16_t subsystem_id,
                                       uint16_t module_id,
                                       std::string_view subsystem_name,
                                       std::string_view module_name) noexcept {
            register_subsystem_module(locale_t::zh_CN, subsystem_id, module_id, subsystem_name, module_name);
        }

        /**
         * @brief 查询指定 locale 的子系统/模块名称（带回退）
         * @details 查询顺序：指定 locale → fallback_locale → 默认值
         * @param locale 首选语言区域
         * @param fallback_locale 回退语言区域
         * @param subsystem_id 子系统 ID
         * @param module_id 模块 ID
         * @return subsystem_module_info_t 子系统/模块名称信息副本（未注册时返回默认值）
         */
        [[nodiscard]] subsystem_module_info_t get_subsystem_module_info(locale_t locale,
                                                                         locale_t fallback_locale,
                                                                         uint16_t subsystem_id,
                                                                         uint16_t module_id) const noexcept;

        /**
         * @brief 查询指定 locale 的子系统/模块名称（回退到 zh_CN）
         * @param locale 首选语言区域
         * @param subsystem_id 子系统 ID
         * @param module_id 模块 ID
         * @return subsystem_module_info_t 子系统/模块名称信息副本（未注册时返回默认值）
         */
        [[nodiscard]] subsystem_module_info_t get_subsystem_module_info(locale_t locale,
                                                                         uint16_t subsystem_id,
                                                                         uint16_t module_id) const noexcept {
            return get_subsystem_module_info(locale, locale_t::zh_CN, subsystem_id, module_id);
        }

        /**
         * @brief 查询子系统/模块名称（便捷重载，使用 zh_CN）
         * @param subsystem_id 子系统 ID
         * @param module_id 模块 ID
         * @return subsystem_module_info_t 子系统/模块名称信息副本（未注册时返回默认值）
         * @note 等价于 get_subsystem_module_info(locale_t::zh_CN, subsystem_id, module_id)
         */
        [[nodiscard]] subsystem_module_info_t get_subsystem_module_info(uint16_t subsystem_id,
                                                                         uint16_t module_id) const noexcept {
            return get_subsystem_module_info(locale_t::zh_CN, subsystem_id, module_id);
        }

        /**
         * @brief 清空所有子系统/模块名称映射
         */
        void clear() noexcept;

        /**
         * @brief 清空指定 locale 的所有子系统/模块名称映射
         * @param locale 语言区域
         * @return size_t 被清除的条目数量
         */
        size_t clear_locale(locale_t locale) noexcept;
    };

}  // namespace error_system::i18n
