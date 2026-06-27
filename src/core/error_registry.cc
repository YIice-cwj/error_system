#include "error_system/core/error_registry.h"

#include <mutex>
#include <new>

namespace error_system::core {

    std::once_flag error_registry_t::once_flag_;

    void error_registry_t::reserve_for_registration_(size_t additional_entries) noexcept {
        if (additional_entries == 0) {
            return;
        }

        primary_index_.reserve(primary_index_.size() + additional_entries);
        name_index_.reserve(name_index_.size() + additional_entries);
        module_index_.reserve(module_index_.size() + (additional_entries / 8) + 1);
    }

    void error_registry_t::erase_from_module_index_(module_group_id_t module_group_id, code_t identity_code) noexcept {
        auto mod_it = module_index_.find(module_group_id);
        if (mod_it == module_index_.end()) {
            return;
        }

        auto& error_list = mod_it->second;
        error_list.erase(std::remove(error_list.begin(), error_list.end(), identity_code), error_list.end());
        if (error_list.empty()) {
            module_index_.erase(mod_it);
        }
    }

    void error_registry_t::erase_from_subsystem_index_(uint16_t subsys_id,
                                                        module_group_id_t module_group_id) noexcept {
        auto it = subsystem_index_.find(subsys_id);
        if (it == subsystem_index_.end()) {
            return;
        }
        it->second.erase(module_group_id);
        if (it->second.empty()) {
            subsystem_index_.erase(it);
        }
    }

    /**
     * @brief 注册错误码
     * @param code 错误码
     * @param name 错误码宏名称
     * @param description 错误码中文描述
     */
    void error_registry_t::register_error(const error_code_t code,
                                          const std::string_view name,
                                          const std::string_view description) noexcept {
        std::unique_lock<std::shared_mutex> lock(index_mutex_);
        reserve_for_registration_(1);

        code_t identity_code = code.get_identity_code();
        auto it = primary_index_.find(identity_code);
        if (it != primary_index_.end()) {
            if (!duplicate_handler_.apply_duplicate_policy(identity_code, &it->second)) {
                return;
            }
            const uint64_t old_group_id = error_code_t{identity_code}.get_module_group_id();
            erase_from_module_index_(old_group_id, identity_code);
            name_index_.erase(it->second.name);
            primary_index_.erase(it);
        }

        try {
            error_metadata_t meta{
                std::string(name), std::string(description), code.get_module(), code.get_number(), code.get_level()};
            name_index_.emplace(meta.name, identity_code);
            primary_index_.emplace(identity_code, std::move(meta));
        } catch (...) {
            fprintf(stderr, "[error_registry] register_error: std::bad_alloc\n");
            return;
        }
        try {
            auto& module_codes = module_index_[code.get_module_group_id()];
            module_codes.reserve(module_codes.size() + 1);
            module_codes.push_back(identity_code);
        } catch (const std::bad_alloc&) {
            fprintf(stderr, "[error_registry] register_error: module_index std::bad_alloc\n");
        }
        {
            uint16_t subsys_id = code.get_subsys();
            subsystem_index_[subsys_id].insert(code.get_module_group_id());
        }
    }

    /**
     * @brief 批量注册错误码
     * @param codes 错误码数组
     * @param names 错误码宏名称数组
     * @param descriptions 错误码中文描述数组
     * @return size_t 实际注册成功的错误码数量
     */
    size_t error_registry_t::register_errors(const std::vector<error_code_t>& codes,
                                             const std::vector<std::string_view>& names,
                                             const std::vector<std::string_view>& descriptions) noexcept {
        if (codes.size() != names.size() || codes.size() != descriptions.size()) {
            return 0;
        }
        std::unique_lock<std::shared_mutex> lock(index_mutex_);
        reserve_for_registration_(codes.size());

        std::unordered_map<module_group_id_t, size_t> module_group_counts;
        module_group_counts.reserve((codes.size() / 8) + 1);
        for (const auto& code : codes) {
            ++module_group_counts[code.get_module_group_id()];
        }
        try {
            for (const auto& [module_group_id, count] : module_group_counts) {
                auto& module_codes = module_index_[module_group_id];
                module_codes.reserve(module_codes.size() + count);
            }
        } catch (const std::bad_alloc&) {
            fprintf(stderr, "[error_registry] register_errors: module_index reserve std::bad_alloc\n");
        }

        size_t registered_count = 0;
        for (size_t i = 0; i < codes.size(); ++i) {
            code_t identity_code = codes[i].get_identity_code();
            auto it = primary_index_.find(identity_code);
            if (it != primary_index_.end()) {
                if (!duplicate_handler_.apply_duplicate_policy(identity_code, &it->second)) {
                    continue;
                }
                const uint64_t old_group_id = error_code_t{identity_code}.get_module_group_id();
                erase_from_module_index_(old_group_id, identity_code);
                name_index_.erase(it->second.name);
                primary_index_.erase(it);
            }

            try {
                error_metadata_t meta{std::string(names[i]),
                                      std::string(descriptions[i]),
                                      codes[i].get_module(),
                                      codes[i].get_number(),
                                      codes[i].get_level()};

                name_index_.emplace(meta.name, identity_code);
                primary_index_.emplace(identity_code, std::move(meta));
            } catch (...) {
                fprintf(stderr, "[error_registry] register_error_batch: std::bad_alloc at index %zu\n", i);
                continue;
            }
            module_index_[codes[i].get_module_group_id()].push_back(identity_code);
            {
                uint16_t subsys_id = codes[i].get_subsys();
                subsystem_index_[subsys_id].insert(codes[i].get_module_group_id());
            }

            ++registered_count;
        }
        return registered_count;
    }

    /**
     * @brief 注销错误码
     * @param code 错误码
     */
    void error_registry_t::unregister_error(const error_code_t code) noexcept {
        std::unique_lock<std::shared_mutex> lock(index_mutex_);

        code_t identity_code = code.get_identity_code();
        auto it = primary_index_.find(identity_code);
        if (it == primary_index_.end()) {
            return;
        }
        uint64_t group_id = code.get_module_group_id();
        name_index_.erase(it->second.name);
        erase_from_module_index_(group_id, identity_code);

        if (module_index_.find(group_id) == module_index_.end()) {
            erase_from_subsystem_index_(code.get_subsys(), group_id);
        }

        primary_index_.erase(it);
    }

    /**
     * @brief 注销错误码
     * @param name 错误码宏名称
     */
    void error_registry_t::unregister_error(const std::string_view name) noexcept {
        std::unique_lock<std::shared_mutex> lock(index_mutex_);
        auto name_it = [&]() noexcept {
            try {
                return name_index_.find(std::string(name));
            } catch (...) {
                fprintf(stderr, "[error_registry] unregister_error: std::bad_alloc\n");
                return name_index_.end();
            }
        }();
        if (name_it == name_index_.end()) {
            return;
        }

        const code_t identity_code = name_it->second;
        const auto primary_it = primary_index_.find(identity_code);
        if (primary_it == primary_index_.end()) {
            name_index_.erase(name_it);
            return;
        }

        error_code_t code{identity_code};
        uint64_t group_id = code.get_module_group_id();
        erase_from_module_index_(group_id, identity_code);
        if (module_index_.find(group_id) == module_index_.end()) {
            erase_from_subsystem_index_(code.get_subsys(), group_id);
        }

        primary_index_.erase(primary_it);
        name_index_.erase(name_it);
    }

    /**
     * @brief 注销模块组的所有错误码
     * @param module_group_id 模块组 ID
     */
    void error_registry_t::unregister_module(const module_group_id_t module_group_id) noexcept {
        std::unique_lock<std::shared_mutex> lock(index_mutex_);
        auto mod_it = module_index_.find(module_group_id);
        if (mod_it == module_index_.end()) {
            return;
        }
        for (code_t raw_code : mod_it->second) {
            const auto primary_it = primary_index_.find(raw_code);
            if (primary_it != primary_index_.end()) {
                name_index_.erase(primary_it->second.name);
                primary_index_.erase(primary_it);
            }
        }
        uint16_t subsys_id = static_cast<uint16_t>((module_group_id >> 32) & 0xFFFF);
        erase_from_subsystem_index_(subsys_id, module_group_id);

        module_index_.erase(mod_it);
    }

    /**
     * @brief 注销所有错误码
     */
    void error_registry_t::unregister_all() noexcept {
        std::unique_lock<std::shared_mutex> lock(index_mutex_);
        decltype(primary_index_)().swap(primary_index_);
        decltype(name_index_)().swap(name_index_);
        decltype(module_index_)().swap(module_index_);
        decltype(subsystem_index_)().swap(subsystem_index_);
        subsystem_module_registry_.clear();
    }

    /**
     * @brief 检查错误码是否已注册
     * @param code 错误码
     * @return bool 错误码是否已注册
     */
    bool error_registry_t::is_registered(const error_code_t code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        return primary_index_.find(code.get_identity_code()) != primary_index_.end();
    }

    /**
     * @brief 通过 64 位错误码 获取详情（值副本，线程安全）
     * @details 在锁内完成拷贝，避免锁释放后另一线程注销导致 use-after-free
     * @param code 错误码
     * @return std::optional<error_metadata_t> 错误码元数据副本，未注册返回 nullopt
     */
    std::optional<error_metadata_t> error_registry_t::get_info(const error_code_t code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        auto it = primary_index_.find(code.get_identity_code());
        if (it == primary_index_.end()) {
            return std::nullopt;
        }
        try {
            return it->second;
        } catch (...) {
            fprintf(stderr, "[error_registry] get_info: std::bad_alloc\n");
            return std::nullopt;
        }
    }

    /**
     * @brief 通过模块组 ID 获取所有错误码（值副本，线程安全）
     * @details 在锁内完成拷贝，避免悬垂引用
     * @param module_group_id 模块组 ID
     * @return std::vector<error_metadata_t> 模块组内所有错误码的元数据副本
     */
    std::vector<error_metadata_t>
    error_registry_t::get_errors_by_module(const module_group_id_t module_group_id) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        std::vector<error_metadata_t> errors;
        auto it = module_index_.find(module_group_id);
        if (it == module_index_.end()) {
            return errors;
        }
        try {
            errors.reserve(it->second.size());
            for (code_t raw_code : it->second) {
                auto primary_it = primary_index_.find(raw_code);
                if (primary_it != primary_index_.end()) {
                    errors.push_back(primary_it->second);
                }
            }
        } catch (...) {
            fprintf(stderr, "[error_registry] get_errors_by_module: std::bad_alloc\n");
        }
        return errors;
    }

    /**
     * @brief 通过子系统 ID 获取该子系统下所有错误码（值副本，线程安全）
     * @details 在锁内完成拷贝，避免悬垂引用
     * @param subsys_id 子系统 ID
     * @return std::vector<error_metadata_t> 子系统下所有错误码的元数据副本
     */
    std::vector<error_metadata_t>
    error_registry_t::get_errors_by_subsystem(uint16_t subsys_id) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        std::vector<error_metadata_t> errors;

        auto it = subsystem_index_.find(subsys_id);
        if (it == subsystem_index_.end()) {
            return errors;
        }

        try {
            for (module_group_id_t module_group_id : it->second) {
                auto mod_it = module_index_.find(module_group_id);
                if (mod_it == module_index_.end()) {
                    continue;
                }
                errors.reserve(errors.size() + mod_it->second.size());
                for (code_t raw_code : mod_it->second) {
                    auto primary_it = primary_index_.find(raw_code);
                    if (primary_it != primary_index_.end()) {
                        errors.push_back(primary_it->second);
                    }
                }
            }
        } catch (...) {
            fprintf(stderr, "[error_registry] get_errors_by_subsystem: std::bad_alloc\n");
        }
        return errors;
    }

    /**
     * @brief 通过错误码名称查找错误码
     * @param name 错误码名称
     * @return std::optional<error_code_t> 错误码，若未注册则返回空可选
     */
    std::optional<error_code_t> error_registry_t::find_by_name(const std::string_view name) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        auto name_it = [&]() noexcept {
            try {
                return name_index_.find(std::string(name));
            } catch (...) {
                fprintf(stderr, "[error_registry] find_by_name: std::bad_alloc\n");
                return name_index_.end();
            }
        }();
        if (name_it != name_index_.end()) {
            return error_code_t(name_it->second);
        }
        return std::nullopt;
    }

    error_registry_t& error_registry_t::instance() noexcept {
        static error_registry_t* instance_ptr = nullptr;
        std::call_once(once_flag_, [] {
            static error_registry_t instance;
            instance_ptr = &instance;
        });
        return *instance_ptr;
    }

}  // namespace error_system::core
