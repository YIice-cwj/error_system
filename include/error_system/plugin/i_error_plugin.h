#pragma once
#include <string_view>

#include "error_system/core/error_context.h"
#include "error_system/core/error_level.h"

/**
 * @file i_error_plugin.h
 * @brief 错误系统插件接口
 * @details 定义错误系统插件的基础接口，插件可接收错误事件并进行处理，
 *          例如：日志记录、统计分析、告警通知等
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::plugin {

    /**
     * @brief 错误系统插件接口
     * @details 继承此接口实现自定义插件，注册到 plugin_registry_t 后，
     *          将在每次错误上下文创建时自动收到 on_error() 回调。
     *          通过 min_level() 可过滤低于指定级别的错误事件。
     */
    class i_error_plugin_t {
    public:
        virtual ~i_error_plugin_t() noexcept = default;

        /**
         * @brief 获取插件名称
         * @details 用于标识插件，注册时若名称重复则替换旧插件
         * @return std::string_view 插件名称
         */
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        /**
         * @brief 获取插件关注的最低错误级别
         * @details 仅接收级别 >= 此值的错误事件，默认返回 debug（接收所有级别）
         * @return error_level_t 最低错误级别
         */
        [[nodiscard]] virtual core::error_level_t min_level() const noexcept { return core::error_level_t::debug; }

        /**
         * @brief 错误事件回调
         * @details 当一个 error_context_t 被创建时触发，实现此方法进行日志/统计等处理
         * @param context 错误上下文（只读）
         */
        virtual void on_error(const core::error_context_t& context) noexcept = 0;
    };

}  // namespace error_system::plugin
