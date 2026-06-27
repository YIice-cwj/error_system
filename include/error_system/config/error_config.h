#pragma once
#include "error_system/config/feature_flags.h"
#include "error_system/config/formatter_config.h"
#include "error_system/config/stacktrace_config.h"

/**
 * @file error_config.h
 * @brief 错误配置统一入口（facade）
 * @details 原 error_config_t 类已按单一职责原则（SRP）拆分为三个独立配置类：
 *          - feature_flags_t：编译期特性开关与运行时布尔标志位
 *          - formatter_config_t：自定义格式化器配置
 *          - stacktrace_config_t：堆栈追踪全局阈值与 per-code 覆盖配置
 *          本文件仅作向后兼容的统一包含入口，新代码请直接包含对应的细分头文件。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
