#include "error_system/core/error_registry.h"

/**
 * @file error_registry.cc
 * @brief 错误码注册表单例实现，含线程本地元数据缓存
 * @details 提供错误码的注册、注销、查询能力，含主索引、名称索引、模块组索引、子系统索引。
 *          基于纪元机制 + 线程本地环形缓存实现低开销的元数据查询，写操作时 bump 纪元使缓存失效。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */

#include <array>
#include <mutex>
#include <new>

namespace error_system::core {

    std::once_flag error_registry_t::once_flag_;
    std::atomic<uint64_t> error_registry_t::epoch_counter_{0};

    namespace {
        /**
         * @brief 线程本地元数据缓存（环形缓冲）
         * @details 容量 16，线性扫描查找；对错误码注册后基本不变的场景，
         *          命中率接近 100%，避免 shared_lock 开销。
         *          缓存命中时零原子操作、零锁。
         *          纪元不一致时整体清空重建。
         */
        class metadata_cache_t {
        public:
            static constexpr size_t CAPACITY = 16;

            /**
             * @brief 查找结果
             * @details found=true 表示缓存命中；has_metadata=true 表示已注册（metadata 字段有效）
             */
            struct lookup_result_t {
                bool found{false};
                bool has_metadata{false};
                error_metadata_t metadata{};
            };

        private:
            struct entry_t {
                code_t identity_code{0};
                bool valid{false};
                bool has_metadata{false};
                error_metadata_t metadata{};
            };

            uint64_t epoch_{0};
            std::array<entry_t, CAPACITY> entries_{};
            size_t next_idx_{0};

        public:
            /**
             * @brief 检查缓存是否对当前纪元有效
             */
            bool matches_epoch(uint64_t current_epoch) const noexcept {
                return epoch_ == current_epoch;
            }

            /**
             * @brief 整体失效并切换到新纪元
             */
            void reset(uint64_t new_epoch) noexcept {
                entries_.fill(entry_t{});
                next_idx_ = 0;
                epoch_ = new_epoch;
            }

            /**
             * @brief 查找缓存项
             * @return lookup_result_t，found=true 表示命中（含 has_metadata=false 的"未注册"缓存）
             */
            lookup_result_t find(code_t identity_code) const noexcept {
                for (size_t i = 0; i < CAPACITY; ++i) {
                    if (entries_[i].valid && entries_[i].identity_code == identity_code) {
                        return lookup_result_t{true, entries_[i].has_metadata, entries_[i].metadata};
                    }
                }
                return lookup_result_t{};
            }

            /**
             * @brief 插入/更新缓存项
             */
            void insert(code_t identity_code, const std::optional<error_metadata_t>& meta) noexcept {
                for (size_t i = 0; i < CAPACITY; ++i) {
                    if (entries_[i].valid && entries_[i].identity_code == identity_code) {
                        entries_[i].has_metadata = meta.has_value();
                        if (meta) {
                            entries_[i].metadata = *meta;
                        }
                        return;
                    }
                }
                entries_[next_idx_].identity_code = identity_code;
                entries_[next_idx_].valid = true;
                entries_[next_idx_].has_metadata = meta.has_value();
                if (meta) {
                    entries_[next_idx_].metadata = *meta;
                }
                next_idx_ = (next_idx_ + 1) % CAPACITY;
            }
        };

        thread_local metadata_cache_t tls_metadata_cache_;
    }  // namespace

    std::optional<error_metadata_t> error_registry_t::get_info_cached(const error_code_t code) const noexcept {
        const uint64_t current_epoch = epoch_counter_.load(std::memory_order_acquire);
        if (!tls_metadata_cache_.matches_epoch(current_epoch)) {
            tls_metadata_cache_.reset(current_epoch);
        }

        const code_t identity_code = code.get_identity_code();
        const auto cached = tls_metadata_cache_.find(identity_code);
        if (cached.found) {
            if (cached.has_metadata) {
                return cached.metadata;
            }
            return std::nullopt;
        }

        auto meta = get_info(code);
        tls_metadata_cache_.insert(identity_code, meta);
        return meta;
    }

    void error_registry_t::invalidate_metadata_cache() const noexcept {
        tls_metadata_cache_.reset(epoch_counter_.load(std::memory_order_acquire));
    }

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

    void error_registry_t::erase_from_subsystem_index_(uint16_t subsystem_id,
                                                        module_group_id_t module_group_id) noexcept {
        auto it = subsystem_index_.find(subsystem_id);
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
            uint16_t subsystem_id = code.get_subsys();
            subsystem_index_[subsystem_id].insert(code.get_module_group_id());
        }
        bump_epoch_();
    }

    /**
     * @brief 批量注册错误码
     * @param codes 错误码数组
     * @param names 错误码宏名称数组
     * @param descriptions 错误码中文描述数组
     * @return size_t 实际注册成功的错误码数量
     */
    void error_registry_t::preallocate_module_buckets_(const std::vector<error_code_t>& codes) noexcept {
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
            std::fprintf(stderr, "[error_registry] preallocate_module_buckets_: std::bad_alloc\n");
        }
    }

    bool error_registry_t::register_single_entry_(error_code_t code, std::string_view name,
                                                  std::string_view description) noexcept {
        code_t identity_code = code.get_identity_code();
        auto it = primary_index_.find(identity_code);
        if (it != primary_index_.end()) {
            if (!duplicate_handler_.apply_duplicate_policy(identity_code, &it->second)) {
                return false;
            }
            const uint64_t old_group_id = error_code_t{identity_code}.get_module_group_id();
            erase_from_module_index_(old_group_id, identity_code);
            name_index_.erase(it->second.name);
            primary_index_.erase(it);
        }

        try {
            error_metadata_t meta{std::string(name),
                                  std::string(description),
                                  code.get_module(),
                                  code.get_number(),
                                  code.get_level()};
            name_index_.emplace(meta.name, identity_code);
            primary_index_.emplace(identity_code, std::move(meta));
        } catch (...) {
            std::fprintf(stderr, "[error_registry] register_single_entry_: std::bad_alloc\n");
            return false;
        }
        module_index_[code.get_module_group_id()].push_back(identity_code);
        subsystem_index_[code.get_subsys()].insert(code.get_module_group_id());
        return true;
    }

    size_t error_registry_t::register_errors(const std::vector<error_code_t>& codes,
                                             const std::vector<std::string_view>& names,
                                             const std::vector<std::string_view>& descriptions) noexcept {
        if (codes.size() != names.size() || codes.size() != descriptions.size()) {
            return 0;
        }
        std::unique_lock<std::shared_mutex> lock(index_mutex_);
        reserve_for_registration_(codes.size());
        preallocate_module_buckets_(codes);

        size_t registered_count = 0;
        for (size_t i = 0; i < codes.size(); ++i) {
            if (register_single_entry_(codes[i], names[i], descriptions[i])) {
                ++registered_count;
            }
        }
        if (registered_count > 0) {
            bump_epoch_();
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
        bump_epoch_();
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
        bump_epoch_();
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
        uint16_t subsystem_id = static_cast<uint16_t>((module_group_id >> 32) & 0xFFFF);
        erase_from_subsystem_index_(subsystem_id, module_group_id);

        module_index_.erase(mod_it);
        bump_epoch_();
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
        bump_epoch_();
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
     * @param subsystem_id 子系统 ID
     * @return std::vector<error_metadata_t> 子系统下所有错误码的元数据副本
     */

    namespace {

        /**
         * @brief 收集单个模块组下所有错误码到 errors
         * @details 遍历模块组内的 raw_code，从 primary_index 查找元数据并追加。
         *          分配失败时捕获异常并记录日志，已收集的部分保留。
         */
        void collect_module_errors(const std::vector<code_t>& raw_codes,
                                   const std::unordered_map<code_t, error_metadata_t>& primary_index,
                                   std::vector<error_metadata_t>& errors) noexcept {
            for (code_t raw_code : raw_codes) {
                auto primary_it = primary_index.find(raw_code);
                if (primary_it != primary_index.end()) {
                    errors.push_back(primary_it->second);
                }
            }
        }

    }  // namespace

    std::vector<error_metadata_t>
    error_registry_t::get_errors_by_subsystem(uint16_t subsystem_id) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        std::vector<error_metadata_t> errors;

        auto it = subsystem_index_.find(subsystem_id);
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
                collect_module_errors(mod_it->second, primary_index_, errors);
            }
        } catch (...) {
            std::fprintf(stderr, "[error_registry] get_errors_by_subsystem: std::bad_alloc\n");
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

    /**
     * @brief 获取单例实例
     * @details 使用 std::call_once + 函数局部静态保证线程安全的单例初始化
     * @return 单例引用
     */
    error_registry_t& error_registry_t::instance() noexcept {
        static error_registry_t* instance_ptr = nullptr;
        std::call_once(once_flag_, [] {
            static error_registry_t instance;
            instance_ptr = &instance;
        });
        return *instance_ptr;
    }

}  // namespace error_system::core
