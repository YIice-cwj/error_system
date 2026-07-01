#include "error_system/core/error_context_initializer.h"

#include <atomic>

#include "error_system/config/error_config.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_registry.h"
#include "error_system/core/i_error_notifier.h"
#include "error_system/utils/source_location.h"
#include "error_system/utils/stack_trace_utils.h"

using error_system::config::feature_flags_t;
using error_system::config::stacktrace_config_t;

/**
 * @file error_context_initializer.cc
 * @brief 错误上下文初始化器实现
 * @details 实现 error_context_initializer_t::initialize() 及其私有辅助方法，
 *          包含错误码校验、堆栈捕获、源位置记录和插件通知。从 error_context.cc 拆分而来。
 *          插件通知通过 i_error_notifier_t 抽象接口完成，不再直接依赖 plugin 层。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    i_error_notifier_t* error_context_initializer_t::notifier_{nullptr};

    namespace {
        constexpr uint16_t FALLBACK_ERROR_NUMBER = 0xFFFF;  ///< 未注册错误码的回退错误编号

        /**
         * @brief 获取错误通知器并在未设置时输出一次性告警
         * @details 若通知器为 nullptr，输出告警提示用户初始化 plugin_registry_t 或设置自定义通知器。
         *          使用静态原子标志保证进程生命周期内仅告警一次，避免热路径刷屏。
         * @return i_error_notifier_t* 当前通知器指针，可能为 nullptr
         */
        i_error_notifier_t* get_notifier_or_warn_() noexcept {
            auto* notifier = error_context_initializer_t::get_error_notifier();
            if (notifier != nullptr) {
                return notifier;
            }
            static std::atomic<bool> warned{false};
            if (!warned.exchange(true, std::memory_order_relaxed)) {
                std::fprintf(stderr,
                             "[error_context_initializer] no error notifier registered; "
                             "plugin notifications will be silently dropped. "
                             "Call plugin_registry_t::instance() before creating error_context_t, "
                             "or set a custom notifier via error_context_initializer_t::set_error_notifier().\n");
            }
            return nullptr;
        }

        /**
         * @brief 通知所有已注册插件（同步路径）
         * @details 从原 error_context.cc 的 notify_plugins 迁移，供 initialize 内部调用。
         *          通过 i_error_notifier_t 接口转发，若未设置通知器则输出一次性告警。
         * @param context 错误上下文
         */
        void notify_plugins(const error_context_t& context) noexcept {
            auto* notifier = get_notifier_or_warn_();
            if (notifier != nullptr) {
                notifier->notify_error(context);
            }
        }
    }  // namespace

    void error_context_initializer_t::fill_validation_fields_(error_context_t& context) noexcept {
        auto info = error_registry_t::instance().get_info_cached(context.code_);
        if (info) {
            return;
        }

        try {
            context.with("illegal_raw_code", std::to_string(context.code_.get_code()));
            context.message = "[UNREGISTERED CODE] " + std::move(context.message);
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_context_initializer] fill_validation_fields_: std::bad_alloc\n");
        }
        context.code_ = error_code_t(error_level_t::fatal, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{FALLBACK_ERROR_NUMBER});
        auto fallback = error_registry_t::instance().get_info_cached(context.code_);
        if (fallback) {
            context.metadata_ = std::move(*fallback);
        }
    }

    void error_context_initializer_t::fill_stacktrace_(error_context_t& context) noexcept {
        if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
            context.stack_frames = utils::stack_trace_utils_t::generate(1);
        }
    }

    void error_context_initializer_t::fill_source_location_(error_context_t& context, bool short_filename_enabled) noexcept {
        if constexpr (feature_flags_t::LOCATION_ENABLED) {
            context.file_name = short_filename_enabled ? utils::extract_short_filename(context.source_location.file_name())
                                                       : context.source_location.file_name();
        }
    }

    void error_context_initializer_t::initialize(error_context_t& context) noexcept {
        const bool validation_enabled = feature_flags_t::is_validation_enabled();
        const bool stacktrace_enabled = feature_flags_t::is_stacktrace_enabled();
        const bool location_enabled = feature_flags_t::is_source_location_enabled();

        auto stacktrace_level = core::error_level_t::warn;
        if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
            stacktrace_level = stacktrace_enabled ? stacktrace_config_t::get_stacktrace_level() : core::error_level_t::warn;
            if (stacktrace_enabled) {
                const auto per_code_level = stacktrace_config_t::get_per_code_stacktrace_level(context.code_.get_identity_code());
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
            fill_validation_fields_(context);
        }

        if constexpr (feature_flags_t::STACKTRACE_ENABLED) {
            if (stacktrace_enabled && context.code_.get_level() >= stacktrace_level) {
                fill_stacktrace_(context);
            }
        }

        if constexpr (feature_flags_t::LOCATION_ENABLED) {
            if (location_enabled) {
                fill_source_location_(context, short_filename_enabled);
            }
        }

        if (feature_flags_t::get_notify_mode() == feature_flags_t::notify_mode_t::async_queue) {
            auto* notifier = get_notifier_or_warn_();
            if (notifier != nullptr) {
                notifier->enqueue_notification(context);
            }
        } else if (feature_flags_t::get_notify_mode() == feature_flags_t::notify_mode_t::sync_deferred) {
            auto* notifier = get_notifier_or_warn_();
            if (notifier != nullptr) {
                notifier->enqueue_deferred_notification(context);
            }
        } else {
            notify_plugins(context);
        }
    }

    void error_context_initializer_t::set_error_notifier(i_error_notifier_t* notifier) noexcept {
        notifier_ = notifier;
    }

    i_error_notifier_t* error_context_initializer_t::get_error_notifier() noexcept {
        return notifier_;
    }

}  // namespace error_system::core
