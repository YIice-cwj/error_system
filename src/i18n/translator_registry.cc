#include "error_system/i18n/translator_registry.h"
#include "error_system/core/error_config.h"
#include "error_system/i18n/language.h"

namespace error_system::i18n {

    /**
     * @brief 获取单例实例
     * @return translator_registry_t& 单例引用
     */
    translator_registry_t& translator_registry_t::instance() noexcept {
        static translator_registry_t instance;
        return instance;
    }

    /**
     * @brief 注册全局翻译器
     * @param translator 翻译器指针，传入 nullptr 可清除注册
     */
    void translator_registry_t::set(i_translator_t* translator) noexcept {
        translator_.store(translator, std::memory_order_relaxed);
    }

    /**
     * @brief 获取当前全局翻译器
     * @details 若未注册，自动创建并注册默认中文翻译器
     * @return i_translator_t* 全局翻译器指针
     */
    i_translator_t* translator_registry_t::get() const noexcept {
        i_translator_t* current_translator = translator_.load(std::memory_order_relaxed);
        if (!current_translator) {
            static json_translator_t default_translator(i18n::from_string(core::error_config_t::get_default_language()));
            translator_.store(&default_translator, std::memory_order_relaxed);
            return &default_translator;
        }
        return current_translator;
    }

    /**
     * @brief 判断是否已注册全局翻译器
     * @return bool 是否已注册
     */
    bool translator_registry_t::has_translator() const noexcept {
        return translator_.load(std::memory_order_relaxed) != nullptr;
    }

}  // namespace error_system::i18n
