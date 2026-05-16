#pragma once
#include "error_system/utils/string_utils.h"
#include <cstdint>
#include <type_traits>

/**
 * @file system_domain.h
 * @brief 错误模块分类定义
 * @details 定义错误码系统中的模块层级结构，包括系统域、子系统和功能模块三级分类，
 *          用于64位错误码bit位分配中的模块标识，支持大规模分布式系统的错误码管理
 * @author yiice
 * @version 1.0.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::domain {

    /**
     * @brief 系统域分类
     * @details 定义错误码系统中的系统域分类，用于标识错误码所属的系统域
     */
    enum class system_domain_t : uint8_t {
        none = 0,         // 无分类 / 未知
        system = 1,       // 系统与底层基础设施层 (OS, 内核, 网络底层等)
        middleware = 2,   // 中间件层 (Redis, Kafka, 注册中心等)
        database = 3,     // 数据库层 (MySQL, PostgreSQL等)
        application = 4,  // 内部业务应用/微服务层 (90% 的业务错误码填这个)
        third_party = 5,  // 外部/第三方依赖层 (专门用于记录调用外部 API 失败)
        _count            // 占位符：表示系统域的总数
    };

    /**
     * @brief 系统域字符串
     * @details 用于表示系统域的字符串
     *          与系统域分类一一对应，用于日志打印和错误处理
     */
    constexpr const char* SYSTEM_DOMAIN_STRING[] = {
        "none", "system", "middleware", "database", "application", "third_party"};

    /**
     * @brief 系统域整数
     * @details 用于将系统域转换为系统域整数
     * @param domain 系统域
     * @return uint8_t 系统域整数
     */
    constexpr uint8_t to_int(system_domain_t domain) noexcept {
        return static_cast<uint8_t>(std::underlying_type_t<system_domain_t>(domain));
    }

    /**
     * @brief 系统域字符串
     * @details 用于将系统域转换为系统域字符串
     * @param domain 系统域
     * @return const char* 系统域字符串
     */
    constexpr const char* to_string(system_domain_t domain) noexcept {
        return SYSTEM_DOMAIN_STRING[to_int(domain)];
    }

    /**
     * @brief 系统域整数是否有效
     * @details 用于判断系统域整数是否有效
     * @param domain 系统域整数
     * @return bool 系统域整数是否有效
     */
    constexpr bool is_valid(uint8_t domain) noexcept {
        return domain < to_int(system_domain_t::_count);
    }

    /**
     * @brief 系统域整数转换为系统域
     * @details 用于将系统域整数转换为系统域
     * @param domain 系统域整数
     * @return system_domain_t 系统域
     */
    constexpr system_domain_t from_int(uint8_t domain) noexcept {
        if (!is_valid(domain)) {
            return system_domain_t::none;
        }
        return static_cast<system_domain_t>(domain);
    }

    /**
     * @brief 系统域字符串转换为系统域
     * @details 用于将系统域字符串转换为系统域
     * @param string 系统域字符串
     * @return system_domain_t 系统域
     */
    constexpr system_domain_t from_string(const char* string) noexcept {
        switch (utils::string_utils_t::hash(string)) {
            case utils::string_utils_t::hash("none"):
                return system_domain_t::none;
            case utils::string_utils_t::hash("system"):
                return system_domain_t::system;
            case utils::string_utils_t::hash("middleware"):
                return system_domain_t::middleware;
            case utils::string_utils_t::hash("database"):
                return system_domain_t::database;
            case utils::string_utils_t::hash("application"):
                return system_domain_t::application;
            case utils::string_utils_t::hash("third_party"):
                return system_domain_t::third_party;
            default:
                return system_domain_t::none;
        }
    }
}  // namespace error_system::domain