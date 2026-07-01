#pragma once
#include <atomic>
#include <cstdint>
#include <optional>

#include "error_system/i18n/locale.h"

/**
 * @file i18n_config.h
 * @brief i18n 配置类
 * @details 从 error_config_t 拆分而来，单一职责：管理 i18n（多语言）功能的启用开关
 *          与输出语言区域配置。线程安全（使用 std::atomic 无锁读写）。
 *
 *          配置项：
 *          - enable_i18n：总开关，false 时序列化输出回退为原始 ID 数字
 *          - output_locale：输出语言区域（显式设置后使用此值）
 *          - default_locale：默认语言区域（output_locale 未设置时回退使用）
 *
 *          locale 解析顺序：output_locale（若已设置）→ default_locale
 *
 *          本类仅持有配置状态，不直接调用 i18n_t / error_registry_t。
 *          序列化器从本类读取 locale 后，显式传给 registry / i18n_t 查询本地化文本。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-29
 * @copyright Copyright (c) 2026
 */
namespace error_system::config {

    using error_system::i18n::locale_t;

    /**
     * @brief i18n 配置类
     * @details 管理 i18n 启用开关与输出 locale 配置。类仅包含静态成员，禁止实例化。
     *
     * @note 与 i18n_t 的关系：
     *       本类是 locale 配置的唯一入口。i18n_t 的 set_default_locale / set_active_locale 等
     *       接口保留以兼容现有调用方，但内部已委托给本类，避免双源配置不同步。
     *       序列化器应从本类读取 locale 配置，保证 config 层单一职责。
     */
    class i18n_config_t {
    private:
        /**
         * @brief i18n 启用标志位存储
         * @details 使用 std::atomic<bool> 保证无锁并发读写
         * @return std::atomic<bool>& i18n 启用标志位引用
         */
        static std::atomic<bool>& get_flag_i18n_enabled_() noexcept {
            static std::atomic<bool> enabled{true};
            return enabled;
        }

        /**
         * @brief 默认 locale 存储（使用 uint8_t 存储 locale_t 的 underlying value）
         * @details std::atomic<locale_t> 在部分平台不支持原子操作，使用 uint8_t 规避
         * @return std::atomic<uint8_t>& 默认 locale 存储引用
         */
        static std::atomic<uint8_t>& get_default_locale_storage_() noexcept {
            static std::atomic<uint8_t> locale{static_cast<uint8_t>(locale_t::zh_CN)};
            return locale;
        }

        /**
         * @brief 输出 locale 存储
         * @details 显式设置的输出 locale，未设置时回退到 default_locale
         * @return std::atomic<uint8_t>& 输出 locale 存储引用
         */
        static std::atomic<uint8_t>& get_output_locale_storage_() noexcept {
            static std::atomic<uint8_t> locale{static_cast<uint8_t>(locale_t::zh_CN)};
            return locale;
        }

        /**
         * @brief 输出 locale 是否已显式设置的标志位
         * @details false 表示未设置，序列化时回退到 default_locale
         * @return std::atomic<bool>& 输出 locale 已设置标志位引用
         */
        static std::atomic<bool>& get_output_locale_set_() noexcept {
            static std::atomic<bool> set{false};
            return set;
        }

    public:
        i18n_config_t() = delete;

        ~i18n_config_t() = delete;

        i18n_config_t(const i18n_config_t&) = delete;

        i18n_config_t& operator=(const i18n_config_t&) = delete;

        i18n_config_t(i18n_config_t&&) = delete;

        i18n_config_t& operator=(i18n_config_t&&) = delete;

        /**
         * @brief 启用/禁用 i18n 功能
         * @details false 时序列化输出回退为原始 ID 数字，
         *          true 时按 output_locale / default_locale 查询本地化文本。
         * @param enable 是否启用
         */
        static void set_enable_i18n(bool enable) noexcept {
            get_flag_i18n_enabled_().store(enable);
        }

        /**
         * @brief 检查 i18n 功能是否启用
         * @return bool 是否启用
         */
        [[nodiscard]] static bool is_i18n_enabled() noexcept {
            return get_flag_i18n_enabled_().load();
        }

        /**
         * @brief 设置默认 locale（回退查询使用）
         * @param locale 默认语言区域
         */
        static void set_default_locale(locale_t locale) noexcept {
            get_default_locale_storage_().store(static_cast<uint8_t>(locale));
        }

        /**
         * @brief 获取默认 locale
         * @return locale_t 默认语言区域
         */
        [[nodiscard]] static locale_t get_default_locale() noexcept {
            return static_cast<locale_t>(get_default_locale_storage_().load());
        }

        /**
         * @brief 设置输出 locale（运行时切换语言）
         * @details 设置后序列化器使用此 locale 查询本地化文本。
         * @param locale 输出语言区域
         */
        static void set_output_locale(locale_t locale) noexcept {
            get_output_locale_storage_().store(static_cast<uint8_t>(locale));
            get_output_locale_set_().store(true);
        }

        /**
         * @brief 清除输出 locale，回退到默认 locale
         */
        static void clear_output_locale() noexcept {
            get_output_locale_set_().store(false);
        }

        /**
         * @brief 获取显式设置的输出 locale
         * @return std::optional<locale_t> 已设置则返回 locale，未设置返回 nullopt
         */
        [[nodiscard]] static std::optional<locale_t> get_output_locale() noexcept {
            if (get_output_locale_set_().load()) {
                return static_cast<locale_t>(get_output_locale_storage_().load());
            }
            return std::nullopt;
        }

        /**
         * @brief 解析最终输出 locale
         * @details 解析顺序：output_locale（若已设置）→ default_locale
         * @return locale_t 最终输出语言区域
         */
        [[nodiscard]] static locale_t resolve_output_locale() noexcept {
            if (get_output_locale_set_().load()) {
                return static_cast<locale_t>(get_output_locale_storage_().load());
            }
            return static_cast<locale_t>(get_default_locale_storage_().load());
        }
    };

}  // namespace error_system::config
