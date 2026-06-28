#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "error_system/core/error_code.h"
#include "error_system/core/error_level.h"
#include "error_system/i18n/locale.h"

/**
 * @file i18n.h
 * @brief 多语言消息目录
 * @details 为已注册错误码提供多语言描述文本（i18n）支持。
 *          运行时按 locale_t + 错误码 identity 查询本地化描述，未命中时回退至默认描述。
 *          线程安全（读多写少场景使用 shared_mutex）。
 *          不依赖第三方 i18n 库，避免引入 ICU/gettext 等重依赖。
 *
 *          语言区域枚举定义见 locale.h（locale_t）。
 * @note 本头文件仅含声明，实现见 i18n.cc
 * @author yiice
 * @version 2.1.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::i18n {

    using error_system::core::code_t;
    using error_system::core::error_code_t;

    /**
     * @brief 多语言消息目录
     * @details 单例模式，按 locale_t 枚举分组管理错误码的本地化描述。
     *          查询路径：active locale → default locale → 空字符串
     *          （由调用方决定回退到 registry 默认描述）。
     *
     * @note 设计取舍：使用 unordered_map<locale_t, unordered_map<code, string>> 两级哈希。
     *       对 locale 数量通常 < 10 的场景，外层哈希常数开销可忽略。
     *       内层按 code_t（uint64）哈希，O(1) 查找。
     *
     * @example
     * @code
     * auto& catalog = i18n_t::instance();
     * catalog.set_default_locale(locale_t::zh_CN);
     * catalog.register_message(locale_t::en_US, ERR_DB_TIMEOUT, "Database connection timeout");
     * catalog.register_message(locale_t::zh_CN, ERR_DB_TIMEOUT, "数据库连接超时");
     *
     * // 运行时按当前 locale 查询
     * catalog.set_active_locale(locale_t::en_US);
     * auto message = catalog.get_message(ERR_DB_TIMEOUT);
     * @endcode
     */
    class i18n_t {
    public:
        using message_t = std::string;
        using code_identity_t = code_t;

    private:
        /**
         * @brief active_locale_ 的"无激活 locale"哨兵值
         * @details 使用 uint8_t 最大值作为哨兵，避免污染 locale_t 枚举。
         *          locale_t 共 15 个值（0-14），255 永不冲突。
         */
        static constexpr uint8_t ACTIVE_LOCALE_NONE_ = 255;

        /**
         * @brief locale → (code identity → message) 的两级映射
         */
        std::unordered_map<locale_t, std::unordered_map<code_identity_t, message_t>> catalog_;

        /**
         * @brief 默认 locale（回退查询使用）
         */
        locale_t default_locale_{locale_t::zh_CN};

        /**
         * @brief 当前激活 locale（运行时切换，原子操作）
         * @details 值为 ACTIVE_LOCALE_NONE_ 时表示未设置，回退到 default_locale_。
         */
        std::atomic<uint8_t> active_locale_{ACTIVE_LOCALE_NONE_};

        /**
         * @brief 读写锁，读多写少场景
         */
        mutable std::shared_mutex mutex_;

        /**
         * @brief 单例初始化一次性标志（规范 22）
         */
        static std::once_flag once_flag_;

        i18n_t() noexcept = default;

        ~i18n_t() noexcept = default;

        i18n_t(const i18n_t&) = delete;

        i18n_t& operator=(const i18n_t&) = delete;

        i18n_t(i18n_t&&) = delete;

        i18n_t& operator=(i18n_t&&) = delete;

    public:
        /**
         * @brief 注册本地化消息
         * @param locale 语言区域
         * @param code 错误码
         * @param message 本地化描述文本
         */
        void register_message(locale_t locale,
                              error_code_t code,
                              std::string_view message) noexcept;

        /**
         * @brief 批量注册本地化消息
         * @param locale 语言区域
         * @param entries (code, message) 列表
         * @return size_t 实际注册数量
         */
        size_t register_messages(locale_t locale,
                                 const std::vector<std::pair<error_code_t, std::string_view>>& entries) noexcept;

        /**
         * @brief 查询本地化消息
         * @details 查询顺序：指定 locale → 默认 locale → 空字符串
         * @param locale 语言区域
         * @param code 错误码
         * @return std::string_view 本地化描述，未命中返回空 string_view
         */
        [[nodiscard]] std::string_view get_message(locale_t locale, error_code_t code) const noexcept;

        /**
         * @brief 使用当前激活 locale 查询
         * @details 若未设置激活 locale，使用默认 locale 查询
         * @param code 错误码
         * @return std::string_view 本地化描述，未命中返回空 string_view
         */
        [[nodiscard]] std::string_view get_message(error_code_t code) const noexcept;

        /**
         * @brief 设置默认 locale（回退查询使用）
         * @param locale 语言区域
         */
        void set_default_locale(locale_t locale) noexcept;

        /**
         * @brief 获取默认 locale
         * @return locale_t 默认 locale
         */
        [[nodiscard]] locale_t get_default_locale() const noexcept;

        /**
         * @brief 设置当前激活 locale（线程安全，原子操作）
         * @details 设置后，get_message(code) 将使用此 locale 查询。
         * @param locale 语言区域
         */
        void set_active_locale(locale_t locale) noexcept;

        /**
         * @brief 清除当前激活 locale，回退到默认 locale
         */
        void clear_active_locale() noexcept;

        /**
         * @brief 获取当前激活 locale
         * @return std::optional<locale_t> 激活 locale，未设置返回 nullopt
         */
        [[nodiscard]] std::optional<locale_t> get_active_locale() const noexcept;

        /**
         * @brief 清除指定 locale 的所有消息
         * @param locale 语言区域
         * @return size_t 被清除的消息数量
         */
        size_t clear_locale(locale_t locale) noexcept;

        /**
         * @brief 清除所有 locale 的所有消息
         */
        void clear_all() noexcept;

        /**
         * @brief 获取已注册的 locale 列表
         * @return std::vector<locale_t> locale 列表
         */
        [[nodiscard]] std::vector<locale_t> get_locales() const noexcept;

        /**
         * @brief 获取指定 locale 的消息数量
         * @param locale 语言区域
         * @return size_t 消息数量
         */
        [[nodiscard]] size_t message_count(locale_t locale) const noexcept;

        /**
         * @brief 获取单例实例
         * @return i18n_t& 单例引用
         */
        static i18n_t& instance() noexcept;
    };

}  // namespace error_system::i18n
