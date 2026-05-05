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
        none = 0,          // 无层
        system = 1,        // 系统层
        kernel = 2,        // 内核层
        middleware = 3,    // 中间件层
        application = 4,   // 应用层
        service = 5,       // 服务层
        network = 6,       // 网络层
        storage = 7,       // 存储层
        database = 8,      // 数据库层
        security = 9,      // 安全层
        ai = 10,            // 人工智能层
        cloud = 11,        // 云计算层
        edge = 12,         // 边缘计算层
        iot = 13,          // 物联网层
        blockchain = 14,   // 区块链层
        bigdata = 15,      // 大数据层
        devops = 16,       // 运维开发层
        distributed = 17,  // 分布式层
        monitoring = 18,   // 监控告警层
        _count             // 占位符：表示系统域的总数
    };

    /**
     * @brief 系统域字符串
     * @details 用于表示系统域的字符串
     *          与系统域分类一一对应，用于日志打印和错误处理
     */
    constexpr const char* SYSTEM_DOMAIN_STRING[] = {"none",
                                                    "system",
                                                    "kernel",
                                                    "middleware",
                                                    "application",
                                                    "service",
                                                    "network",
                                                    "storage",
                                                    "database",
                                                    "security",
                                                    "ai",
                                                    "cloud",
                                                    "edge",
                                                    "iot",
                                                    "blockchain",
                                                    "bigdata",
                                                    "devops",
                                                    "distributed",
                                                    "monitoring"};

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
            return system_domain_t::devops;
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
            case utils::string_utils_t::hash("system"):
                return system_domain_t::system;
            case utils::string_utils_t::hash("kernel"):
                return system_domain_t::kernel;
            case utils::string_utils_t::hash("middleware"):
                return system_domain_t::middleware;
            case utils::string_utils_t::hash("application"):
                return system_domain_t::application;
            case utils::string_utils_t::hash("service"):
                return system_domain_t::service;
            case utils::string_utils_t::hash("network"):
                return system_domain_t::network;
            case utils::string_utils_t::hash("storage"):
                return system_domain_t::storage;
            case utils::string_utils_t::hash("database"):
                return system_domain_t::database;
            case utils::string_utils_t::hash("security"):
                return system_domain_t::security;
            case utils::string_utils_t::hash("ai"):
                return system_domain_t::ai;
            case utils::string_utils_t::hash("cloud"):
                return system_domain_t::cloud;
            case utils::string_utils_t::hash("edge"):
                return system_domain_t::edge;
            case utils::string_utils_t::hash("iot"):
                return system_domain_t::iot;
            case utils::string_utils_t::hash("blockchain"):
                return system_domain_t::blockchain;
            case utils::string_utils_t::hash("bigdata"):
                return system_domain_t::bigdata;
            case utils::string_utils_t::hash("devops"):
                return system_domain_t::devops;
            case utils::string_utils_t::hash("distributed"):
                return system_domain_t::distributed;
            case utils::string_utils_t::hash("monitoring"):
                return system_domain_t::monitoring;
            default:
                return system_domain_t::none;
        }
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

}  // namespace error_system::domain