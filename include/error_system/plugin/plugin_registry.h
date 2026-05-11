#pragma once
#include "error_system/plugin/i_error_plugin.h"
#include <shared_mutex>
#include <string_view>
#include <vector>
// IWYU pragma: begin_exports
#include <mutex>
// IWYU pragma: end_exports

/**
 * @file plugin_registry.h
 * @brief 插件注册表
 * @details 单例模式，管理所有已注册的错误系统插件，
 *          在错误事件发生时依次通知所有插件
 * @author yiice
 * @version 1.0.0
 * @date 2026-05-01
 * @copyright Copyright (c) 2026
 */
namespace error_system::plugin {

    /**
     * @brief 插件注册表
     * @details 单例，不持有插件所有权，调用方负责插件对象的生命周期
     */
    class plugin_registry_t {
        private:
        std::vector<i_error_plugin_t*> plugins_;
        mutable std::shared_mutex plugins_mutex_;

        private:
        plugin_registry_t() = default;

        ~plugin_registry_t() = default;

        plugin_registry_t(const plugin_registry_t&) = delete;

        plugin_registry_t& operator=(const plugin_registry_t&) = delete;

        plugin_registry_t(plugin_registry_t&&) = delete;

        plugin_registry_t& operator=(plugin_registry_t&&) = delete;

        public:
        /**
         * @brief 注册插件
         * @details 若已存在同名插件，将替换旧插件
         *          不持有所有权，调用方需保证插件在整个使用期间有效
         * @param plugin 插件指针，不能为 nullptr
         */
        void register_plugin(i_error_plugin_t* plugin) noexcept;

        /**
         * @brief 注销插件
         * @details 按名称查找并移除，未找到则无操作
         * @param name 插件名称
         */
        void unregister_plugin(std::string_view name) noexcept;

        /**
         * @brief 通知所有插件发生了错误事件
         * @details 依次调用每个插件的 on_error()，异常不会向外传播
         * @param context 错误上下文
         */
        void notify_error(const core::error_context_t& context) noexcept;

        /**
         * @brief 获取已注册插件数量
         * @return size_t 插件数量
         */
        size_t size() const noexcept;

        /**
         * @brief 判断是否有已注册的插件
         * @return bool 是否为空
         */
        bool empty() const noexcept;

        /**
         * @brief 清空所有已注册插件
         */
        void clear() noexcept;

        public:
        /**
         * @brief 获取单例实例
         * @return plugin_registry_t& 单例引用
         */
        static plugin_registry_t& instance() noexcept;
    };

}  // namespace error_system::plugin
