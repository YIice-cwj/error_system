#include "error_system/translator/error_translator.h"

namespace error_system::translator {

    /**
     * @brief 翻译错误系统ID和模块ID为可读的字符串
     * @details 翻译错误系统ID和模块ID为可读的字符串，用于显示错误信息
     * @param subsys_id 子系统ID
     * @param module_id 模块ID
     * @return std::string 可读的错误信息
     */
    std::string error_translator_t::translate(uint16_t subsys_id, uint16_t module_id) const noexcept {
        std::string_view static_subsys = get_static_subsys_name(subsys_id);
        std::string_view static_module = get_static_module_name(subsys_id, module_id);

        std::string final_subsys = "";
        std::string final_module = "";

        if (static_subsys != "未知子系统") {
            final_subsys = static_subsys;
        } else {
            std::shared_lock lock(rw_mutex_);
            if (auto it = dynamic_subsystems_.find(subsys_id); it != dynamic_subsystems_.end()) {
                final_subsys = it->second;
            } else {
                final_subsys = std::to_string(subsys_id);
            }
        }

        // 同理处理模块（Module）
        if (static_module != "未知模块") {
            final_module = static_module;
        } else {
            std::shared_lock lock(rw_mutex_);
            uint32_t key = (static_cast<uint32_t>(subsys_id) << 16) | module_id;
            if (auto it = dynamic_modules_.find(key); it != dynamic_modules_.end()) {
                final_module = it->second;
            } else {
                final_module = std::to_string(module_id);
            }
        }

        return "SubSys: " + final_subsys + ", Module: " + final_module;
    }
}  // namespace error_system::translator
