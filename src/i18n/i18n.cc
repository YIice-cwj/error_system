#include "error_system/i18n/i18n.h"

/**
 * @file i18n.cc
 * @brief 多语言消息目录实现
 * @details 提供多语言错误消息的注册与查询能力，基于 std::call_once 实现线程安全的单例初始化。
 *          查询路径：active locale → default locale → 空字符串。
 *          线程安全（读多写少场景使用 shared_mutex）。
 * @author yiice
 * @version 2.1.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */

#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>

namespace error_system::i18n {

    std::once_flag i18n_t::once_flag_;

    /**
     * @brief 获取单例实例
     * @details 使用 std::call_once + 函数局部静态保证线程安全的单例初始化
     * @return 单例引用
     */
    i18n_t& i18n_t::instance() noexcept {
        static i18n_t* instance_ptr = nullptr;
        std::call_once(once_flag_, [] {
            static i18n_t instance;
            instance_ptr = &instance;
        });
        return *instance_ptr;
    }

    /**
     * @brief 注册本地化消息
     * @param locale 语言区域
     * @param code 错误码
     * @param message 本地化描述文本
     */
    void i18n_t::register_message(locale_t locale,
                                  error_code_t code,
                                  std::string_view message) noexcept {
        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            catalog_[locale][code.get_identity_code()] = std::string(message);
        } catch (...) {
            std::fprintf(stderr, "[i18n] register_message: std::bad_alloc\n");
        }
    }

    /**
     * @brief 批量注册本地化消息
     * @param locale 语言区域
     * @param entries (code, message) 列表
     * @return size_t 实际注册数量
     */
    size_t i18n_t::register_messages(locale_t locale,
                                     const std::vector<std::pair<error_code_t, std::string_view>>& entries) noexcept {
        try {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto& locale_map = catalog_[locale];
            locale_map.reserve(locale_map.size() + entries.size());
            size_t count = 0;
            for (const auto& [code, message] : entries) {
                locale_map[code.get_identity_code()] = std::string(message);
                ++count;
            }
            return count;
        } catch (...) {
            std::fprintf(stderr, "[i18n] register_messages: std::bad_alloc\n");
            return 0;
        }
    }

    /**
     * @brief 查询本地化消息
     * @details 查询顺序：指定 locale → 默认 locale → 空字符串
     * @param locale 语言区域
     * @param code 错误码
     * @return std::string_view 本地化描述，未命中返回空 string_view
     */
    std::string_view i18n_t::get_message(locale_t locale, error_code_t code) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        const auto identity = code.get_identity_code();

        auto loc_it = catalog_.find(locale);
        if (loc_it != catalog_.end()) {
            auto msg_it = loc_it->second.find(identity);
            if (msg_it != loc_it->second.end()) {
                return msg_it->second;
            }
        }

        if (locale != default_locale_) {
            auto def_it = catalog_.find(default_locale_);
            if (def_it != catalog_.end()) {
                auto msg_it = def_it->second.find(identity);
                if (msg_it != def_it->second.end()) {
                    return msg_it->second;
                }
            }
        }

        return {};
    }

    /**
     * @brief 使用当前激活 locale 查询
     * @details 若未设置激活 locale，使用默认 locale 查询
     * @param code 错误码
     * @return std::string_view 本地化描述，未命中返回空 string_view
     */
    std::string_view i18n_t::get_message(error_code_t code) const noexcept {
        const uint8_t raw = active_locale_.load(std::memory_order_acquire);
        if (raw == ACTIVE_LOCALE_NONE_) {
            return get_message(default_locale_, code);
        }
        return get_message(static_cast<locale_t>(raw), code);
    }

    /**
     * @brief 设置默认 locale（回退查询使用）
     * @param locale 语言区域
     */
    void i18n_t::set_default_locale(locale_t locale) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        default_locale_ = locale;
    }

    /**
     * @brief 获取默认 locale
     * @return locale_t 默认 locale
     */
    locale_t i18n_t::get_default_locale() const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return default_locale_;
    }

    /**
     * @brief 设置当前激活 locale（线程安全，原子操作）
     * @details 设置后，get_message(code) 将使用此 locale 查询。
     * @param locale 语言区域
     */
    void i18n_t::set_active_locale(locale_t locale) noexcept {
        active_locale_.store(static_cast<uint8_t>(locale), std::memory_order_release);
    }

    /**
     * @brief 清除当前激活 locale，回退到默认 locale
     */
    void i18n_t::clear_active_locale() noexcept {
        active_locale_.store(ACTIVE_LOCALE_NONE_, std::memory_order_release);
    }

    /**
     * @brief 获取当前激活 locale
     * @return std::optional<locale_t> 激活 locale，未设置返回 nullopt
     */
    std::optional<locale_t> i18n_t::get_active_locale() const noexcept {
        const uint8_t raw = active_locale_.load(std::memory_order_acquire);
        if (raw == ACTIVE_LOCALE_NONE_) {
            return std::nullopt;
        }
        return static_cast<locale_t>(raw);
    }

    /**
     * @brief 清除指定 locale 的所有消息
     * @param locale 语言区域
     * @return size_t 被清除的消息数量
     */
    size_t i18n_t::clear_locale(locale_t locale) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = catalog_.find(locale);
        if (it == catalog_.end()) {
            return 0;
        }
        size_t count = it->second.size();
        catalog_.erase(it);
        return count;
    }

    /**
     * @brief 清除所有 locale 的所有消息
     */
    void i18n_t::clear_all() noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        catalog_.clear();
    }

    /**
     * @brief 获取已注册的 locale 列表
     * @return std::vector<locale_t> locale 列表
     */
    std::vector<locale_t> i18n_t::get_locales() const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<locale_t> locales;
        try {
            locales.reserve(catalog_.size());
            for (const auto& [locale, _] : catalog_) {
                locales.push_back(locale);
            }
        } catch (...) {
            std::fprintf(stderr, "[i18n] get_locales: std::bad_alloc\n");
        }
        return locales;
    }

    /**
     * @brief 获取指定 locale 的消息数量
     * @param locale 语言区域
     * @return size_t 消息数量
     */
    size_t i18n_t::message_count(locale_t locale) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = catalog_.find(locale);
        if (it == catalog_.end()) {
            return 0;
        }
        return it->second.size();
    }

}  // namespace error_system::i18n
