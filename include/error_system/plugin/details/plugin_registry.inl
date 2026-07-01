#pragma once
#include "error_system/plugin/plugin_registry.h"

namespace error_system::plugin {

    /**
     * @brief 原子更新插件快照
     * @details 在写锁保护下，基于当前快照创建新副本、修改后原子替换。
     *          旧快照由 shared_ptr 自动管理生命周期。
     *          modifier 同时操作快照副本与 owned_plugins_，保证二者在
     *          同一写锁保护下保持一致。make_shared 与 modifier 中的
     *          push_back 可能抛出 std::bad_alloc，捕获后记录日志并返回，
     *          保持旧快照不变。
     * @tparam Modifier 修改回调函数类型
     * @param modifier 修改新副本与 owned_plugins_ 的回调函数
     */
    template <typename Modifier>
    inline void plugin_registry_t::update_snapshot_(Modifier&& modifier) noexcept {
        try {
            std::unique_lock<std::shared_mutex> lock(plugins_mutex_);
            auto old_snapshot = std::atomic_load(&plugins_snapshot_);
            auto new_snapshot_ptr = std::make_shared<plugin_list_t>(*old_snapshot);
            modifier(*new_snapshot_ptr, owned_plugins_);
            std::atomic_store(&plugins_snapshot_,
                              std::static_pointer_cast<const plugin_list_t>(new_snapshot_ptr));
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[plugin_registry] update_snapshot_: std::bad_alloc\n");
        }
    }

}  // namespace error_system::plugin
