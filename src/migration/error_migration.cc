#include "error_system/migration/error_migration.h"

#include <cstdio>
#include <mutex>
#include <utility>

/**
 * @file error_migration.cc
 * @brief 错误码废弃与迁移注册器实现
 * @details 提供 error_migration_registry_t 所有方法的实现，基于 std::call_once 实现线程安全的单例初始化。
 *          所有写入方法（mark_deprecated / register_migration / unmark_* / clear_all）使用 unique_lock，
 *          读取方法（get_deprecation_info / is_deprecated / migrate / migrate_recursive / count / list）使用 shared_lock，
 *          适配读多写少的运行时场景。bad_alloc 等异常在内部 try-catch 捕获并记录到 stderr，保持 noexcept 安全。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::migration {

    namespace {
        constexpr size_t MAX_MIGRATION_RECURSION_DEPTH = 16;  ///< 迁移递归查找的最大深度
    }  // namespace

    using error_system::core::code_t;
    using error_system::core::error_code_t;

    std::once_flag error_migration_registry_t::once_flag_;

    error_migration_registry_t& error_migration_registry_t::instance() noexcept {
        static error_migration_registry_t* instance_ptr = nullptr;
        std::call_once(once_flag_, [] {
            static error_migration_registry_t instance;
            instance_ptr = &instance;
        });
        return *instance_ptr;
    }

    void error_migration_registry_t::mark_deprecated(error_code_t code,
                                                      const deprecation_meta_t& meta) noexcept {
        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            deprecation_info_t info;
            info.deprecated = true;
            info.reason = meta.reason;
            info.replacement = meta.replacement;
            info.since_version = meta.since_version;
            info.removal_version = meta.removal_version;
            deprecations_[code.get_identity_code()] = std::move(info);

            if (meta.replacement) {
                migrations_[code.get_identity_code()] = meta.replacement->get_identity_code();
            }
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_migration] mark_deprecated: std::bad_alloc\n");
        }
    }

    void error_migration_registry_t::register_migration(error_code_t old_code, error_code_t new_code) noexcept {
        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            migrations_[old_code.get_identity_code()] = new_code.get_identity_code();
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_migration] register_migration: std::bad_alloc\n");
        }
    }

    std::optional<deprecation_info_t> error_migration_registry_t::get_deprecation_info(error_code_t code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = deprecations_.find(code.get_identity_code());
        if (it == deprecations_.end()) {
            return std::nullopt;
        }
        try {
            return it->second;
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_migration] get_deprecation_info: std::bad_alloc\n");
            return std::nullopt;
        }
    }

    bool error_migration_registry_t::is_deprecated(error_code_t code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = deprecations_.find(code.get_identity_code());
        return it != deprecations_.end() && it->second.deprecated;
    }

    std::optional<error_code_t> error_migration_registry_t::migrate(error_code_t old_code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = migrations_.find(old_code.get_identity_code());
        if (it == migrations_.end()) {
            return std::nullopt;
        }
        return error_code_t{it->second};
    }

    error_code_t error_migration_registry_t::migrate_recursive(error_code_t old_code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        code_t current = old_code.get_identity_code();
        for (size_t i = 0; i < MAX_MIGRATION_RECURSION_DEPTH; ++i) {
            auto it = migrations_.find(current);
            if (it == migrations_.end()) {
                break;
            }
            current = it->second;
        }
        return error_code_t{current};
    }

    bool error_migration_registry_t::unmark_deprecated(error_code_t code) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = deprecations_.find(code.get_identity_code());
        if (it == deprecations_.end()) {
            return false;
        }
        deprecations_.erase(it);
        return true;
    }

    bool error_migration_registry_t::unregister_migration(error_code_t old_code) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = migrations_.find(old_code.get_identity_code());
        if (it == migrations_.end()) {
            return false;
        }
        migrations_.erase(it);
        return true;
    }

    void error_migration_registry_t::clear_all() noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        deprecations_.clear();
        migrations_.clear();
    }

    size_t error_migration_registry_t::deprecated_count() const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return deprecations_.size();
    }

    size_t error_migration_registry_t::migration_count() const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return migrations_.size();
    }

    std::vector<code_t> error_migration_registry_t::get_deprecated_codes() const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<code_t> codes;
        try {
            codes.reserve(deprecations_.size());
            for (const auto& [identity, _] : deprecations_) {
                codes.push_back(identity);
            }
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_migration] get_deprecated_codes: std::bad_alloc\n");
        }
        return codes;
    }

}  // namespace error_system::migration
