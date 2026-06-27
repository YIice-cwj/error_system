#include "error_system/core/error_context_initializer.h"

#include "error_system/config/error_config.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_registry.h"
#include "error_system/plugin/plugin_registry.h"
#include "error_system/utils/source_location.h"
#include "error_system/utils/stack_trace_utils.h"

using error_system::config::feature_flags_t;
using error_system::config::stacktrace_config_t;

/**
 * @file error_context_initializer.cc
 * @brief 错误上下文初始化器实现
 * @details 实现 error_context_initializer_t::initialize() 及其私有辅助方法，
 *          包含错误码校验、堆栈捕获、源位置记录和插件通知。从 error_context.cc 拆分而来。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {
    namespace {
        /**
         * @brief 通知所有已注册插件（同步路径）
         * @details 从原 error_context.cc 的 notify_plugins 迁移，供 initialize 内部调用
         * @param ctx 错误上下文
         */
        void notify_plugins(const error_context_t& ctx) noexcept {
            plugin::plugin_registry_t::instance().notify_error(ctx);
        }
    }  // namespace

    void error_context_initializer_t::fill_validation_fields_(error_context_t& ctx) noexcept {
        if (error_registry_t::instance().is_registered(ctx.code_)) {
            return;
        }

        try {
            ctx.with("illegal_raw_code", std::to_string(ctx.code_.get_code()));
            ctx.message = "[UNREGISTERED CODE] " + std::move(ctx.message);
        } catch (...) {
            std::fprintf(stderr, "[error_context_initializer] fill_validation_fields_: std::bad_alloc\n");
        }
        ctx.code_ = error_code_t(error_level_t::fatal, domain::system_domain_t::none, 0, 0, 0xFFFF);
        auto info = error_registry_t::instance().get_info(ctx.code_);
        if (info) {
            ctx.metadata_ = std::move(*info);
        }
    }

    void error_context_initializer_t::fill_stacktrace_(error_context_t& ctx) noexcept {
        if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
            ctx.stack_frames = utils::stack_trace_utils_t::generate(1);
        }
    }

    void error_context_initializer_t::fill_source_location_(error_context_t& ctx, bool short_filename_enabled) noexcept {
        if constexpr (feature_flags_t::LOCATION_ENABLED) {
            ctx.file_name = short_filename_enabled ? utils::extract_short_filename(ctx.source_location.file_name())
                                                   : ctx.source_location.file_name();
        }
    }

    void error_context_initializer_t::initialize(error_context_t& ctx) noexcept {
        const bool validation_enabled = feature_flags_t::is_validation_enabled();
        const bool stacktrace_enabled = feature_flags_t::is_stacktrace_enabled();
        const bool location_enabled = feature_flags_t::is_source_location_enabled();

        auto stacktrace_level = core::error_level_t::warn;
        if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
            stacktrace_level = stacktrace_enabled ? stacktrace_config_t::get_stacktrace_level() : core::error_level_t::warn;
            if (stacktrace_enabled) {
                const auto per_code_level = stacktrace_config_t::get_per_code_stacktrace_level(ctx.code_.get_identity_code());
                if (per_code_level.has_value()) {
                    stacktrace_level = per_code_level.value();
                }
            }
        }

        const bool short_filename_enabled = [&]() {
            if constexpr (feature_flags_t::LOCATION_ENABLED) {
                return location_enabled && feature_flags_t::is_short_filename_enabled();
            } else {
                return false;
            }
        }();

        if (validation_enabled) {
            fill_validation_fields_(ctx);
        }

        if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
            if (stacktrace_enabled && ctx.code_.get_level() >= stacktrace_level) {
                fill_stacktrace_(ctx);
            }
        }

        if constexpr (feature_flags_t::LOCATION_ENABLED) {
            if (location_enabled) {
                fill_source_location_(ctx, short_filename_enabled);
            }
        }

        if (feature_flags_t::get_notify_mode() == feature_flags_t::notify_mode_t::async_queue) {
            plugin::plugin_registry_t::instance().enqueue_notification(ctx);
        } else {
            notify_plugins(ctx);
        }
    }

}  // namespace error_system::core
