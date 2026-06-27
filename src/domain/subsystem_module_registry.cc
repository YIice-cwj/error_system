#include "error_system/domain/subsystem_module_registry.h"

#include <cstdio>

namespace error_system::core {

    /**
     * @brief 注册子系统/模块名称
     * @param subsys_id 子系统 ID
     * @param module_id 模块 ID
     * @param subsystem_name 子系统名称
     * @param module_name 模块名称
     */
    void subsystem_module_registry_t::register_subsystem_module(uint16_t subsys_id,
                                                                uint16_t module_id,
                                                                const std::string_view subsystem_name,
                                                                const std::string_view module_name) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint32_t key = (static_cast<uint32_t>(subsys_id) << 16) | module_id;
        if (subsystem_module_index_.find(key) == subsystem_module_index_.end()) {
            try {
                subsystem_module_index_.emplace(
                    key, subsystem_module_info_t{std::string(subsystem_name), std::string(module_name)});
            } catch (...) {
                std::fprintf(stderr, "[subsystem_module_registry] register_subsystem_module: std::bad_alloc\n");
            }
        }
    }

    /**
     * @brief 查询子系统/模块名称（值副本，线程安全）
     * @details 在锁内完成拷贝，避免锁释放后另一线程调用 clear() 导致悬垂引用
     * @param subsys_id 子系统 ID
     * @param module_id 模块 ID
     * @return subsystem_module_info_t 子系统/模块名称信息副本（未注册时返回默认值）
     */
    subsystem_module_info_t subsystem_module_registry_t::get_subsystem_module_info(
        uint16_t subsys_id, uint16_t module_id) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        uint32_t key = (static_cast<uint32_t>(subsys_id) << 16) | module_id;
        auto it = subsystem_module_index_.find(key);
        if (it != subsystem_module_index_.end()) {
            try {
                return it->second;
            } catch (...) {
                std::fprintf(stderr, "[subsystem_module_registry] get_subsystem_module_info: std::bad_alloc\n");
            }
        }
        return subsystem_module_info_t{};
    }

    /**
     * @brief 清空所有子系统/模块名称映射
     */
    void subsystem_module_registry_t::clear() noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        decltype(subsystem_module_index_)().swap(subsystem_module_index_);
    }

}  // namespace error_system::core
