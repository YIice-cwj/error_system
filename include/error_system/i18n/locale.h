#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

/**
 * @file locale.h
 * @brief 语言区域枚举与转换
 * @details 定义 i18n 模块使用的 locale_t 枚举，覆盖常用语言区域。
 *          提供 to_string / from_string / is_valid 转换函数。
 *          使用枚举替代裸字符串，提供类型安全的 locale 标识。
 *          所有 locale 名称集中维护在 LOCALE_TABLE 中，新增 locale 只需在枚举与表末尾追加。
 * @author yiice
 * @version 1.1.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::i18n {

    /**
     * @brief 语言区域枚举
     * @details 覆盖常用语言区域。扩展时在此枚举末尾追加，
     *          并同步在 LOCALE_TABLE 末尾追加对应条目。
     */
    enum class locale_t : uint8_t {
        en_US = 0,
        zh_CN = 1,
        zh_TW = 2,
        ja_JP = 3,
        ko_KR = 4,
        fr_FR = 5,
        de_DE = 6,
        es_ES = 7,
        ru_RU = 8,
        pt_BR = 9,
        it_IT = 10,
        ar_SA = 11,
        hi_IN = 12,
        th_TH = 13,
        vi_VN = 14,
    };

    /**
     * @brief locale_t 枚举值的数量
     */
    inline constexpr size_t LOCALE_COUNT = 15;

    /**
     * @brief locale 名称查找表（按 enum 值顺序排列）
     * @details to_string / from_string / is_valid 的唯一数据源。
     *          新增 locale 时在末尾追加 {locale_t::xxx, "xxx"} 即可，
     *          无需修改任何转换函数实现。
     */
    inline constexpr std::array<std::pair<locale_t, std::string_view>, LOCALE_COUNT> LOCALE_TABLE = {{
        {locale_t::en_US, "en_US"},
        {locale_t::zh_CN, "zh_CN"},
        {locale_t::zh_TW, "zh_TW"},
        {locale_t::ja_JP, "ja_JP"},
        {locale_t::ko_KR, "ko_KR"},
        {locale_t::fr_FR, "fr_FR"},
        {locale_t::de_DE, "de_DE"},
        {locale_t::es_ES, "es_ES"},
        {locale_t::ru_RU, "ru_RU"},
        {locale_t::pt_BR, "pt_BR"},
        {locale_t::it_IT, "it_IT"},
        {locale_t::ar_SA, "ar_SA"},
        {locale_t::hi_IN, "hi_IN"},
        {locale_t::th_TH, "th_TH"},
        {locale_t::vi_VN, "vi_VN"},
    }};

    /**
     * @brief 将 locale_t 转换为字符串标识（如 "zh_CN"）
     * @param locale 语言区域枚举值
     * @return std::string_view 字符串标识；非法值返回 "en_US"
     */
    [[nodiscard]] inline constexpr std::string_view to_string(locale_t locale) noexcept {
        const auto index = static_cast<size_t>(locale);
        if (index < LOCALE_COUNT) {
            return LOCALE_TABLE[index].second;
        }
        return "en_US";
    }

    /**
     * @brief 从字符串标识解析 locale_t
     * @param text 字符串标识（如 "zh_CN"）
     * @return locale_t 枚举值（未识别返回 en_US）
     */
    [[nodiscard]] inline constexpr locale_t from_string(std::string_view text) noexcept {
        for (const auto& [value, name] : LOCALE_TABLE) {
            if (name == text) {
                return value;
            }
        }
        return locale_t::en_US;
    }

    /**
     * @brief 检查字符串是否为已知 locale 标识
     * @param text 字符串标识
     * @return bool 已知返回 true
     */
    [[nodiscard]] inline constexpr bool is_valid(std::string_view text) noexcept {
        for (const auto& [_, name] : LOCALE_TABLE) {
            if (name == text) {
                return true;
            }
        }
        return false;
    }

}  // namespace error_system::i18n
