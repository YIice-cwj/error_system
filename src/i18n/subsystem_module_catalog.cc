#include "error_system/i18n/subsystem_module_catalog.h"

#include <cstdio>
#include <mutex>
#include <utility>

/**
 * @file subsystem_module_catalog.cc
 * @brief 子系统/模块名称多语言目录实现
 * @details 实现 subsystem_module_catalog_t 的注册、查询、清理方法。
 *          写入方法使用 unique_lock，读取方法使用 shared_lock，适配读多写少场景。
 *          bad_alloc 等异常在内部 try-catch 捕获并记录到 stderr，保持 noexcept 安全。
 * @author yiice
 * @version 1.1.0
 * @date 2026-06-29
 * @copyright Copyright (c) 2026
 */
namespace error_system::i18n {

    std::once_flag subsystem_module_catalog_t::once_flag_;

    subsystem_module_catalog_t& subsystem_module_catalog_t::instance() noexcept {
        static subsystem_module_catalog_t* instance_ptr = nullptr;
        std::call_once(once_flag_, [] {
            static subsystem_module_catalog_t instance;
            instance_ptr = &instance;
        });
        return *instance_ptr;
    }

    namespace {
        /**
         * @brief 计算 (subsystem_id, module_id) 复合键
         * @details key = (subsystem_id << 16) | module_id，避免每个错误码重复存储名称
         */
        constexpr uint32_t make_catalog_key_(uint16_t subsystem_id, uint16_t module_id) noexcept {
            return (static_cast<uint32_t>(subsystem_id) << 16) | module_id;
        }
    }  // namespace

    void subsystem_module_catalog_t::register_subsystem_module(locale_t locale,
                                                                uint16_t subsystem_id,
                                                                uint16_t module_id,
                                                                std::string_view subsystem_name,
                                                                std::string_view module_name) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        const uint32_t key = make_catalog_key_(subsystem_id, module_id);
        auto& locale_map = catalog_[key];
        if (locale_map.find(locale) != locale_map.end()) {
            return; 
        }
        try {
            locale_map.emplace(locale,
                               subsystem_module_info_t{std::string(subsystem_name), std::string(module_name)});
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[subsystem_module_catalog] register_subsystem_module: std::bad_alloc\n");
        }
    }

    subsystem_module_info_t subsystem_module_catalog_t::get_subsystem_module_info(
        locale_t locale,
        locale_t fallback_locale,
        uint16_t subsystem_id,
        uint16_t module_id) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        const uint32_t key = make_catalog_key_(subsystem_id, module_id);

        auto outer_it = catalog_.find(key);
        if (outer_it == catalog_.end()) {
            return subsystem_module_info_t{};
        }

        const auto& locale_map = outer_it->second;

        auto loc_it = locale_map.find(locale);
        if (loc_it != locale_map.end()) {
            try {
                return loc_it->second;
            } catch (const std::bad_alloc&) {
                std::fprintf(stderr, "[subsystem_module_catalog] get_subsystem_module_info: primary locale copy failed\n");
            }
        }

        if (fallback_locale != locale) {
            auto fallback_it = locale_map.find(fallback_locale);
            if (fallback_it != locale_map.end()) {
                try {
                    return fallback_it->second;
                } catch (const std::bad_alloc&) {
                    std::fprintf(stderr, "[subsystem_module_catalog] get_subsystem_module_info: fallback locale copy failed\n");
                }
            }
        }

        return subsystem_module_info_t{};
    }

    void subsystem_module_catalog_t::clear() noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        decltype(catalog_)().swap(catalog_);
    }

    size_t subsystem_module_catalog_t::clear_locale(locale_t locale) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        size_t cleared_count = 0;
        for (auto it = catalog_.begin(); it != catalog_.end(); ) {
            auto& locale_map = it->second;
            auto loc_it = locale_map.find(locale);
            if (loc_it != locale_map.end()) {
                locale_map.erase(loc_it);
                ++cleared_count;
            }
            if (locale_map.empty()) {
                it = catalog_.erase(it);
            } else {
                ++it;
            }
        }
        return cleared_count;
    }

    i_subsystem_module_resolver_t* get_default_subsystem_module_resolver() noexcept {
        return &subsystem_module_catalog_t::instance();
    }

}  // namespace error_system::i18n
