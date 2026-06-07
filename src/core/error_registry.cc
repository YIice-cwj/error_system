#include "error_system/core/error_registry.h"
#include <iostream>

namespace error_system::core {

    error_registry_t::error_registry_t() {
        duplicate_warn_callback_ = [](code_t raw_code, const error_metadata_t& meta) {
            std::cerr << "[error_system::warn] 重复注册错误码: " << meta.name << " (0x" << std::hex << raw_code
                      << std::dec << ")"
                      << ", 已有描述: " << meta.description << "\n";
        };
    }

    /**
     * @brief 处理 skip 策略
     * @param raw_code 错误码原始值
     * @return bool 是否继续注册流程
     */
    bool error_registry_t::__handle_duplicate_skip(code_t /*raw_code*/) noexcept {
        return false;
    }

    /**
     * @brief 处理 overwrite 策略
     * @param raw_code 错误码原始值
     * @return bool 是否继续注册流程
     */
    bool error_registry_t::__handle_duplicate_overwrite(code_t raw_code) noexcept {
        uint64_t old_group_id = error_code_t(raw_code).get_module_group_id();
        auto mod_it = module_index_.find(old_group_id);
        if (mod_it != module_index_.end()) {
            auto& vec = mod_it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), raw_code), vec.end());
            if (vec.empty()) {
                module_index_.erase(mod_it);
            }
        }
        primary_index_.erase(raw_code);
        return true;
    }

    /**
     * @brief 处理 warn 策略
     * @param raw_code 错误码原始值
     * @return bool 是否继续注册流程
     */
    bool error_registry_t::__handle_duplicate_warn(code_t raw_code) noexcept {
        auto it = primary_index_.find(raw_code);
        if (it == primary_index_.end()) {
            return false;
        }
        if (duplicate_warn_callback_) {
            duplicate_warn_callback_(raw_code, it->second);
        }
        return false;
    }

    /**
     * @brief 根据当前策略处理重复错误码
     * @param raw_code 错误码原始值
     * @return bool 是否继续注册流程
     */
    bool error_registry_t::__apply_duplicate_policy(code_t raw_code) noexcept {
        switch (duplicate_policy_) {
            case duplicate_policy_t::skip:
                return __handle_duplicate_skip(raw_code);
            case duplicate_policy_t::overwrite:
                return __handle_duplicate_overwrite(raw_code);
            case duplicate_policy_t::warn:
                return __handle_duplicate_warn(raw_code);
        }
        return false;
    }

    /**
     * @brief 注册子系统/模块名称
     * @param subsys_id 子系统 ID
     * @param module_id 模块 ID
     * @param subsystem_name 子系统名称
     * @param module_name 模块名称
     */
    void error_registry_t::register_subsystem_module(uint16_t subsys_id,
                                                     uint16_t module_id,
                                                     const std::string_view subsystem_name,
                                                     const std::string_view module_name) noexcept {
        std::unique_lock<std::shared_mutex> lock(index_mutex_);
        uint32_t key = (static_cast<uint32_t>(subsys_id) << 16) | module_id;
        if (subsystem_module_index_.find(key) == subsystem_module_index_.end()) {
            subsystem_module_index_.emplace(
                key, subsystem_module_info_t{std::string(subsystem_name), std::string(module_name)});
        }
    }

    /**
     * @brief 查询子系统/模块名称
     * @param subsys_id 子系统 ID
     * @param module_id 模块 ID
     * @return const subsystem_module_info_t& 子系统/模块名称信息
     */
    const subsystem_module_info_t& error_registry_t::get_subsystem_module_info(uint16_t subsys_id,
                                                                               uint16_t module_id) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        uint32_t key = (static_cast<uint32_t>(subsys_id) << 16) | module_id;
        auto it = subsystem_module_index_.find(key);
        if (it != subsystem_module_index_.end()) {
            return it->second;
        }
        static const subsystem_module_info_t unknown{"未知子系统", "未知模块"};
        return unknown;
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

        code_t identity_code = code.get_identity_code();
        auto it = primary_index_.find(identity_code);
        if (it != primary_index_.end()) {
            if (!__apply_duplicate_policy(identity_code)) {
                return;
            }
        }

        error_metadata_t meta{
            std::string(name), std::string(description), code.get_module(), code.get_number(), code.get_level()};
        primary_index_.emplace(identity_code, meta);
        module_index_[code.get_module_group_id()].push_back(identity_code);
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

        size_t registered_count = 0;
        for (size_t i = 0; i < codes.size(); ++i) {
            code_t identity_code = codes[i].get_identity_code();
            auto it = primary_index_.find(identity_code);
            if (it != primary_index_.end()) {
                if (!__apply_duplicate_policy(identity_code)) {
                    continue;
                }
            }

            error_metadata_t meta{std::string(names[i]),
                                  std::string(descriptions[i]),
                                  codes[i].get_module(),
                                  codes[i].get_number(),
                                  codes[i].get_level()};

            primary_index_.emplace(identity_code, meta);
            module_index_[codes[i].get_module_group_id()].push_back(identity_code);
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
        auto mod_it = module_index_.find(group_id);
        if (mod_it != module_index_.end()) {
            auto& vec = mod_it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), identity_code), vec.end());
            if (vec.empty()) {
                module_index_.erase(mod_it);
            }
        }
        primary_index_.erase(it);
    }

    /**
     * @brief 注销错误码
     * @param name 错误码宏名称
     */
    void error_registry_t::unregister_error(const std::string_view name) noexcept {
        std::unique_lock<std::shared_mutex> lock(index_mutex_);
        for (auto it = primary_index_.begin(); it != primary_index_.end(); ++it) {
            if (it->second.name == name) {
                code_t identity_code = it->first;
                uint64_t group_id = error_code_t(identity_code).get_module_group_id();
                auto mod_it = module_index_.find(group_id);
                if (mod_it != module_index_.end()) {
                    auto& vec = mod_it->second;
                    vec.erase(std::remove(vec.begin(), vec.end(), identity_code), vec.end());
                    if (vec.empty()) {
                        module_index_.erase(mod_it);
                    }
                }
                primary_index_.erase(it);
                return;
            }
        }
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
            primary_index_.erase(raw_code);
        }
        module_index_.erase(mod_it);
    }

    /**
     * @brief 注销所有错误码
     */
    void error_registry_t::unregister_all() noexcept {
        std::unique_lock<std::shared_mutex> lock(index_mutex_);
        decltype(primary_index_)().swap(primary_index_);
        decltype(module_index_)().swap(module_index_);
        decltype(subsystem_module_index_)().swap(subsystem_module_index_);
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
     * @brief 通过 64 位错误码 获取详情
     * @param code 错误码
     * @return std::optional<std::reference_wrapper<const error_metadata_t>> 错误码元数据引用，若未注册则返回空可选
     */
    std::optional<std::reference_wrapper<const error_metadata_t>>
    error_registry_t::get_info(const error_code_t code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        auto it = primary_index_.find(code.get_identity_code());
        if (it != primary_index_.end()) {
            return std::ref(it->second);
        }
        return std::nullopt;
    }

    /**
     * @brief 通过模块组 ID 获取所有错误码
     * @param module_group_id 模块组 ID
     * @return std::vector<std::reference_wrapper<const error_metadata_t>> 模块组内所有错误码的元数据引用
     */
    std::vector<std::reference_wrapper<const error_metadata_t>>
    error_registry_t::get_errors_by_module(const module_group_id_t module_group_id) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        auto it = module_index_.find(module_group_id);
        if (it != module_index_.end()) {
            std::vector<std::reference_wrapper<const error_metadata_t>> errors;
            errors.reserve(it->second.size());
            for (code_t raw_code : it->second) {
                errors.push_back(primary_index_.at(raw_code));
            }
            return errors;
        }
        return {};
    }
}  // namespace error_system::core