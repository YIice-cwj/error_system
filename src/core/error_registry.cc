#include "error_system/core/error_registry.h"

namespace error_system::core {

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

        code_t raw_code = code.get_code();
        if (primary_index_.find(raw_code) != primary_index_.end()) {
            return;
        }

        error_metadata_t meta{
            std::string(name), std::string(description), code.get_module(), code.get_number(), code.get_level()};
        primary_index_.emplace(raw_code, meta);
        module_index_[code.get_module_group_id()].push_back(raw_code);
    }

    /**
     * @brief 批量注册错误码
     * @param codes 错误码数组
     * @param names 错误码宏名称数组
     * @param descriptions 错误码中文描述数组
     */
    void error_registry_t::register_errors(const std::vector<error_code_t>& codes,
                                           const std::vector<std::string_view>& names,
                                           const std::vector<std::string_view>& descriptions) noexcept {
        if (codes.size() != names.size() || codes.size() != descriptions.size()) {
            return;
        }
        std::unique_lock<std::shared_mutex> lock(index_mutex_);

        for (size_t i = 0; i < codes.size(); ++i) {
            code_t raw_code = codes[i].get_code();
            if (primary_index_.find(raw_code) != primary_index_.end()) {
                continue;
            }

            error_metadata_t meta{std::string(names[i]),
                                  std::string(descriptions[i]),
                                  codes[i].get_module(),
                                  codes[i].get_number(),
                                  codes[i].get_level()};

            primary_index_.emplace(raw_code, meta);
            module_index_[codes[i].get_module_group_id()].push_back(raw_code);
        }
    }

    /**
     * @brief 注销错误码
     * @param code 错误码
     */
    void error_registry_t::unregister_error(const error_code_t code) noexcept {
        std::unique_lock<std::shared_mutex> lock(index_mutex_);

        code_t raw_code = code.get_code();
        auto it = primary_index_.find(raw_code);
        if (it == primary_index_.end()) {
            return;
        }
        uint64_t group_id = code.get_module_group_id();
        auto mod_it = module_index_.find(group_id);
        if (mod_it != module_index_.end()) {
            auto& vec = mod_it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), raw_code), vec.end());
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
                code_t raw_code = it->first;
                uint64_t group_id = error_code_t(raw_code).get_module_group_id();
                auto mod_it = module_index_.find(group_id);
                if (mod_it != module_index_.end()) {
                    auto& vec = mod_it->second;
                    vec.erase(std::remove(vec.begin(), vec.end(), raw_code), vec.end());
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
    }

    /**
     * @brief 检查错误码是否已注册
     * @param code 错误码
     * @return bool 错误码是否已注册
     */
    bool error_registry_t::is_registered(const error_code_t code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        return primary_index_.find(code.get_code()) != primary_index_.end();
    }

    /**
     * @brief 通过 64 位错误码 获取详情
     * @param code 错误码
     * @return std::optional<error_metadata_t> 错误码元数据，若未注册则返回空可选
     */
    std::optional<error_metadata_t> error_registry_t::get_info(const error_code_t code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        auto it = primary_index_.find(code.get_code());
        if (it != primary_index_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief 通过模块组 ID 获取所有错误码
     * @param module_group_id 模块组 ID
     * @return std::vector<error_metadata_t> 模块组内所有错误码的元数据
     */
    std::vector<error_metadata_t>
    error_registry_t::get_errors_by_module(const module_group_id_t module_group_id) const noexcept {
        std::shared_lock<std::shared_mutex> lock(index_mutex_);
        auto it = module_index_.find(module_group_id);
        if (it != module_index_.end()) {
            std::vector<error_metadata_t> errors;
            errors.reserve(it->second.size());
            for (code_t raw_code : it->second) {
                errors.push_back(primary_index_.at(raw_code));
            }
            return errors;
        }
        return {};
    }
}  // namespace error_system::core