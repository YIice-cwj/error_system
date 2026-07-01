#pragma once
#include <cstdint>
#include <string>

#include "error_system/core/error_level.h"

/**
 * @file error_metadata.h
 * @brief 错误码元数据定义
 * @details 定义错误码元数据的数据结构（名称、描述、模块 ID、错误编号、等级）。
 *          从 error_registry.h 中提取为独立头文件，降低 core 层内部模块间耦合，
 *          便于 serializer、builder、context 等组件按需包含，而不必引入完整的注册表。
 * @author yiice
 * @version 1.0.0
 * @date 2026-07-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 错误码元数据信息（数据负载）
     * @details 仅包含值语义字段，可复制，线程安全（注册表返回副本）。
     */
    struct error_metadata_t {
        std::string name{};               ///< 错误码宏名称
        std::string description{};        ///< 错误码描述文本
        uint16_t module_id{0};            ///< 模块 ID
        uint16_t error_number{0};         ///< 错误编号
        error_level_t level{error_level_t::info};  ///< 错误等级
    };

}  // namespace error_system::core
