#pragma once
#include "error_system/utils/string_utils.h"
#include <cstdint>

/**
 * @file error_level.h
 * @brief 错误等级定义与转换
 * @details 定义错误系统的等级分类（debug/info/warn/error/fatal/custom），
 *          提供错误等级与整数、字符串之间的相互转换功能，
 *          支持编译期常量计算，用于日志过滤和错误处理决策
 * @author yiice
 * @version 2.3.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误等级分类
     * @details 定义错误系统的等级分类（debug/info/warn/error/fatal)
     *          用于表示错误等级的严重程度和处理优先级
     */
    enum class error_level_t : uint8_t {
        debug = 0,    // 调试
        info = 1,     // 信息
        warn = 2,     // 警告
        error = 3,    // 错误
        fatal = 4,    // 致命错误
    };

    /**
     * @brief 错误等级字符串
     * @details 用于表示错误等级的字符串
     *          与错误等级分类一一对应，用于日志打印和错误处理
     */
    constexpr const char* ERROR_LEVEL_STRING[] = {"debug", "info", "warn", "error", "fatal"};

    /**
     * @brief 错误等级整数
     * @details 用于将错误等级转换为错误等级整数
     * @param level 错误等级
     * @return uint8_t 错误等级整数
     */
    constexpr uint8_t to_int(error_level_t level) noexcept {
        return static_cast<uint8_t>(std::underlying_type_t<error_level_t>(level));
    }

    /**
     * @brief 错误等级整数是否有效
     * @details 用于判断错误等级整数是否有效
     * @param level 错误等级整数
     * @return bool 错误等级整数是否有效
     */
    constexpr bool is_valid(uint8_t level) noexcept {
        return level <= to_int(error_level_t::fatal);
    }

    /**
     * @brief 错误等级整数转换为错误等级
     * @details 用于将错误等级整数转换为错误等级
     * @param level 错误等级整数
     * @return error_level_t 错误等级
     */
    constexpr error_level_t from_int(uint8_t level) noexcept {
        if (!is_valid(level)) {
            return error_level_t::fatal;
        }
        return static_cast<error_level_t>(level);
    }

    /**
     * @brief 错误等级字符串转换为错误等级
     * @details 用于将错误等级字符串转换为错误等级
     * @param level 错误等级字符串
     * @return error_level_t 错误等级
     */
    constexpr error_level_t from_string(const char* string) noexcept {
        switch (utils::string_utils_t::hash(string)) {
            case utils::string_utils_t::hash("debug"):
                return error_level_t::debug;
            case utils::string_utils_t::hash("info"):
                return error_level_t::info;
            case utils::string_utils_t::hash("warn"):
                return error_level_t::warn;
            case utils::string_utils_t::hash("error"):
                return error_level_t::error;
            case utils::string_utils_t::hash("fatal"):
                return error_level_t::fatal;
            default:
                return error_level_t::info;
        }
    }

    /**
     * @brief 错误等级字符串
     * @details 用于将错误等级转换为错误等级字符串
     * @param level 错误等级
     * @return const char* 错误等级字符串
     */
    constexpr const char* to_string(error_level_t level) noexcept {
        return ERROR_LEVEL_STRING[to_int(level)];
    }

    /**
     * @brief 错误等级整数的下一个错误等级
     * @details 用于获取错误等级整数的下一个错误等级
     * @param level 错误等级整数
     * @return error_level_t 错误等级整数的下一个错误等级
     */
    constexpr error_level_t next_level(error_level_t level) noexcept {
        return from_int(to_int(level) + 1);
    }

    /**
     * @brief 错误等级整数的上一个错误等级
     * @details 用于获取错误等级整数的上一个错误等级
     * @param level 错误等级整数
     * @return error_level_t 错误等级整数的上一个错误等级
     */
    constexpr error_level_t prev_level(error_level_t level) noexcept {
        return from_int(to_int(level) - 1);
    }

    /**
     * @brief 错误等级整数是否大于等于最小错误等级
     * @details 用于判断错误等级整数是否大于等于最小错误等级
     * @param current 错误等级整数
     * @param min_level 最小错误等级整数
     * @return bool 错误等级整数是否大于等于最小错误等级
     */
    constexpr bool should_log(error_level_t current, error_level_t min_level) noexcept {
        return to_int(current) >= to_int(min_level);
    }

}  // namespace error_system::core