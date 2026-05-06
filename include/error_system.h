#pragma once

/**
 * @file error_system.h
 * @brief 错误系统统一入口
 * @details 统一包含错误系统的所有公共头文件，用户只需引入本文件即可使用完整的错误系统功能，
 *          包括错误码定义、子系统/模块聚合、Traits 特化、国际化翻译等所有组件
 * @author yiice
 * @version 1.0.0
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
// ─── Domain ───────────────────────────────────────────────────────────────────
#include "error_system/domain/system_domain.h"

// ─── Subsystem (聚合) ─────────────────────────────────────────────────────────
#include "error_system/subsystem/subsystem.h"

// ─── Module (聚合) ────────────────────────────────────────────────────────────
#include "error_system/module/module.h"

// ─── Plugin ─────────────────────────────────────────────────────────────────────
#include "error_system/plugin/plugin_registry.h"

// ─── Traits ───────────────────────────────────────────────────────────────────
#include "error_system/traits/module_dispatcher.h"
#include "error_system/traits/subsystem_dispatcher.h"

// ─── I18n ─────────────────────────────────────────────────────────────────────
#include "error_system/i18n/i_translator.h"
#include "error_system/i18n/json_translator.h"
#include "error_system/i18n/language.h"
#include "error_system/i18n/translator_registry.h"

// ─── Utils ─────────────────────────────────────────────────────────────────────
#include "error_system/utils/error_formatter.h"
// IWYU pragma: end_exports
