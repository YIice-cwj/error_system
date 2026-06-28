#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "error_system/core/error_code.h"

/**
 * @file error_migration.h
 * @brief 错误码废弃与迁移管理
 * @details 为已注册错误码提供废弃标记（deprecation）与迁移映射（migration）能力。
 *          - 废弃：标记某错误码已废弃，记录废弃说明与替代码（可选）；
 *          - 迁移：记录旧码→新码的映射，运行时查询时自动提示迁移路径。
 *
 *          典型用途：
 *          1. 版本演进中下线某错误码，希望编译期不破坏但运行时给出废弃警告；
 *          2. 错误码重构（如子系统重新编号），自动将旧码映射到新码。
 *
 *          线程安全（读多写少场景使用 shared_mutex）。
 *          本头文件仅含声明，实现见 error_migration.cc（遵循规范第 5 条）。
 * @author yiice
 * @version 1.1.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::migration {

    /**
     * @brief 废弃信息
     * @details 描述某错误码的废弃状态、废弃说明、可选的替代码与废弃版本
     */
    struct deprecation_info_t {
        /**
         * @brief 是否已废弃
         */
        bool deprecated{false};

        /**
         * @brief 废弃说明（人类可读，建议包含迁移指引）
         */
        std::string reason;

        /**
         * @brief 替代错误码（nullopt 表示无直接替代）
         */
        std::optional<error_system::core::error_code_t> replacement{};

        /**
         * @brief 废弃起始版本（语义化版本字符串，如 "2.0.0"）
         */
        std::string since_version;

        /**
         * @brief 预期完全移除的版本（空表示未定）
         */
        std::string removal_version;
    };

    /**
     * @brief 错误码废弃与迁移注册器
     * @details 单例模式，管理错误码的废弃状态与旧码→新码迁移映射。
     *          查询接口返回值副本，避免锁释放后被另一线程修改导致悬垂引用。
     *
     * @note 设计取舍：
     *       - 废弃状态与迁移映射分离存储，因为废弃不一定有替代码，
     *         迁移也不一定意味着源码已废弃（可能是别名）；
     *       - 使用 shared_mutex 适配读多写少的运行时场景。
     *
     * @example
     * @code
     * auto& reg = error_migration_registry_t::instance();
     * reg.mark_deprecated(ERR_OLD_CODE, "使用 ERR_NEW_CODE 替代", ERR_NEW_CODE, "2.0.0", "3.0.0");
     *
     * if (auto info = reg.get_deprecation_info(ERR_OLD_CODE); info && info->deprecated) {
     *     log_warn("错误码 {} 已废弃: {}", ERR_OLD_CODE, info->reason);
     * }
     *
     * if (auto migrated = reg.migrate(ERR_OLD_CODE)) {
     *     use_new_code(*migrated);
     * }
     * @endcode
     */
    class error_migration_registry_t {
    private:
        /**
         * @brief code identity → 废弃信息
         */
        std::unordered_map<error_system::core::code_t, deprecation_info_t> deprecations_;

        /**
         * @brief 旧码 identity → 新码 identity 的迁移映射
         */
        std::unordered_map<error_system::core::code_t, error_system::core::code_t> migrations_;

        mutable std::shared_mutex mutex_;

        static std::once_flag once_flag_;

        error_migration_registry_t() noexcept = default;

        ~error_migration_registry_t() noexcept = default;

        error_migration_registry_t(const error_migration_registry_t&) = delete;

        error_migration_registry_t& operator=(const error_migration_registry_t&) = delete;

        error_migration_registry_t(error_migration_registry_t&&) = delete;

        error_migration_registry_t& operator=(error_migration_registry_t&&) = delete;

    public:
        /**
         * @brief 标记错误码为废弃
         * @param code 被废弃的错误码
         * @param reason 废弃原因（建议包含迁移指引）
         * @param replacement 替代错误码（可选，nullopt 表示无直接替代）
         * @param since_version 废弃起始版本
         * @param removal_version 预期移除版本（空表示未定）
         */
        void mark_deprecated(error_system::core::error_code_t code,
                             std::string_view reason,
                             std::optional<error_system::core::error_code_t> replacement = std::nullopt,
                             std::string_view since_version = "",
                             std::string_view removal_version = "") noexcept;

        /**
         * @brief 建立迁移映射（不标记废弃）
         * @details 适用于别名场景：旧码与新码共存，查询旧码时自动迁移到新码。
         * @param old_code 旧错误码
         * @param new_code 新错误码
         */
        void register_migration(error_system::core::error_code_t old_code,
                                error_system::core::error_code_t new_code) noexcept;

        /**
         * @brief 查询废弃信息
         * @param code 错误码
         * @return std::optional<deprecation_info_t> 废弃信息副本，未标记废弃返回 nullopt
         */
        [[nodiscard]] std::optional<deprecation_info_t> get_deprecation_info(error_system::core::error_code_t code) const noexcept;

        /**
         * @brief 检查错误码是否已废弃
         * @param code 错误码
         * @return bool 是否已废弃
         */
        [[nodiscard]] bool is_deprecated(error_system::core::error_code_t code) const noexcept;

        /**
         * @brief 迁移错误码（单次跳转）
         * @details 若 old_code 在迁移映射中，返回 new_code；否则返回 nullopt。
         *          不会递归迁移（即新码若也存在于映射中，不会继续跳转）。
         * @param old_code 旧错误码
         * @return std::optional<error_code_t> 新错误码，无映射返回 nullopt
         */
        [[nodiscard]] std::optional<error_system::core::error_code_t> migrate(error_system::core::error_code_t old_code) const noexcept;

        /**
         * @brief 递归迁移错误码（直到终点）
         * @details 沿迁移链递归跳转，直到无进一步映射或检测到环（环检测后返回当前码）。
         *          最大递归深度 16，防止恶意数据导致栈溢出。
         * @param old_code 旧错误码
         * @return error_code_t 迁移终点错误码
         */
        [[nodiscard]] error_system::core::error_code_t migrate_recursive(error_system::core::error_code_t old_code) const noexcept;

        /**
         * @brief 移除废弃标记
         * @param code 错误码
         * @return bool 是否成功移除（未标记返回 false）
         */
        bool unmark_deprecated(error_system::core::error_code_t code) noexcept;

        /**
         * @brief 移除迁移映射
         * @param old_code 旧错误码
         * @return bool 是否成功移除（无映射返回 false）
         */
        bool unregister_migration(error_system::core::error_code_t old_code) noexcept;

        /**
         * @brief 清除所有废弃标记与迁移映射
         */
        void clear_all() noexcept;

        /**
         * @brief 获取已废弃错误码数量
         * @return size_t 废弃标记数量
         */
        [[nodiscard]] size_t deprecated_count() const noexcept;

        /**
         * @brief 获取迁移映射数量
         * @return size_t 迁移映射数量
         */
        [[nodiscard]] size_t migration_count() const noexcept;

        /**
         * @brief 获取所有已废弃的错误码 identity 列表
         * @return std::vector<code_t> 废弃错误码列表
         */
        [[nodiscard]] std::vector<error_system::core::code_t> get_deprecated_codes() const noexcept;

        /**
         * @brief 获取单例实例
         * @return error_migration_registry_t& 单例引用
         */
        static error_migration_registry_t& instance() noexcept;
    };

}  // namespace error_system::migration
