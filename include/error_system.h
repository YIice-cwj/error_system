#pragma once

/**
 * @file error_system.h
 * @brief 错误系统统一入口
 * @details 统一包含错误系统的所有公共头文件，用户只需引入本文件即可使用完整的错误系统功能，
 *          包括错误码定义、子系统/模块聚合、Traits 特化、国际化翻译等所有组件
 * @author yiice
 * @version 2.3.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */

// IWYU pragma: begin_exports

// ─── Core ─────────────────────────────────────────────────────────────────────
#include "error_system/core/error_builder.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_exception.h"
#include "error_system/core/error_level.h"
#include "error_system/core/error_registry.h"
#include "error_system/core/result.h"
// ─── Config ────────────────────────────────────────────────────────────────────
#include "error_system/config/error_config.h"
// ─── Domain ───────────────────────────────────────────────────────────────────
#include "error_system/domain/system_domain.h"

// ─── Plugin ─────────────────────────────────────────────────────────────────────
#include "error_system/plugin/error_router_plugin.h"
#include "error_system/plugin/i_error_plugin.h"
#include "error_system/plugin/plugin_registry.h"

// ─── Utils ─────────────────────────────────────────────────────────────────────
#include "error_system/utils/async_queue.h"
#include "error_system/utils/error_formatter.h"
#include "error_system/utils/stack_trace_utils.h"
#include "error_system/utils/source_location.h"
// IWYU pragma: end_exports
