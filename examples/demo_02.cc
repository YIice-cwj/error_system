#include "error_system.h"
#include <iostream>

namespace demo {

    using namespace error_system;

    // ========================================================================
    // 第 1 部分：结构化负载（Payload）
    // ========================================================================

    /**
     * @brief 演示错误上下文的结构化负载功能
     */
    void demo_payload() {
        std::cout << "\n========== 1. 结构化负载（Payload）==========\n";

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::database, 0, 0, 1001);

        core::error_context_t ctx(code, "数据库连接失败");
        ctx.with("host", "192.168.1.100").with("port", "3306").with("database", "user_db").with("timeout_ms", "3000");

        std::cout << "错误信息: " << ctx.to_string() << "\n";
    }

    // ========================================================================
    // 第 2 部分：堆栈跟踪
    // ========================================================================

    /**
     * @brief 演示错误上下文的堆栈跟踪功能
     */
    void demo_stack_trace() {
        std::cout << "\n========== 2. 堆栈跟踪 ==========\n";

        core::error_config_t::set_stacktrace_level(core::error_level_t::warn);

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::kernel, 0, 0, 500);

        core::error_context_t ctx(code, "内核 panic");
        std::cout << "包含堆栈的错误:\n" << ctx.to_string() << "\n";

        core::error_config_t::set_stacktrace_level(core::error_level_t::fatal);
    }

    // ========================================================================
    // 第 3 部分：多语言翻译
    // ========================================================================

    /**
     * @brief 演示多语言翻译功能
     */
    void demo_i18n() {
        std::cout << "\n========== 3. 多语言翻译 ==========\n";

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::fatal, domain::system_domain_t::database, 0, 0, 404);

        core::error_context_t ctx(code, "连接超时");

        // 中文翻译
        i18n::json_translator_t zh_translator(i18n::language_t::zh_cn);
        std::cout << "中文: " << ctx.to_string(&zh_translator) << "\n";

        // 英文翻译
        i18n::json_translator_t en_translator(i18n::language_t::en_us);
        std::cout << "英文: " << ctx.to_string(&en_translator) << "\n";

        // 使用全局注册表
        i18n::translator_registry_t::instance().set(&zh_translator);
        std::cout << "全局翻译器: " << ctx.to_string() << "\n";
        i18n::translator_registry_t::instance().set(nullptr);
    }

    // ========================================================================
    // 第 4 部分：插件系统
    // ========================================================================

    /**
     * @brief 日志插件示例
     */
    class logging_plugin_t : public plugin::i_error_plugin_t {
        public:
        std::string_view name() const noexcept override { return "logging"; }

        void on_error(const core::error_context_t& ctx) noexcept override {
            std::cout << "  [日志插件] 捕获错误: " << ctx.message << "\n";
        }
    };

    /**
     * @brief 统计插件示例
     */
    class stats_plugin_t : public plugin::i_error_plugin_t {
        private:
        mutable size_t error_count_{0};

        public:
        std::string_view name() const noexcept override { return "stats"; }

        void on_error(const core::error_context_t& context) noexcept override {
            const auto& message = context.message;
            ++error_count_;
            std::cout << "  [统计插件] 累计错误数: " << error_count_ << " 错误信息: " << message << "\n";
        }

        size_t count() const noexcept { return error_count_; }
    };

    /**
     * @brief 演示插件系统的注册和通知
     */
    void demo_plugin() {
        std::cout << "\n========== 4. 插件系统 ==========\n";

        logging_plugin_t logger;
        stats_plugin_t stats;

        auto& registry = plugin::plugin_registry_t::instance();
        registry.register_plugin(&logger);
        registry.register_plugin(&stats);

        std::cout << "注册插件后数量: " << registry.size() << "\n";

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::application, 0, 0, 1);

        std::cout << "触发错误:\n";
        core::error_context_t ctx(code, "业务逻辑异常");

        std::cout << "当前统计: " << stats.count() << "\n";

        registry.clear();
        std::cout << "清空插件后数量: " << registry.size() << "\n";
    }

    // ========================================================================
    // 第 5 部分：result_t 链式处理
    // ========================================================================

    /**
     * @brief 模拟数据库查询
     */
    core::result_t<std::string> query_database(const std::string& sql) {
        if (sql.empty()) {
            auto code = core::error_builder_t::make_error_code(
                core::error_level_t::error, domain::system_domain_t::database, 0, 0, 1);
            return core::error_context_t(code, "SQL 不能为空");
        }
        return std::string("result_set_42");
    }

    /**
     * @brief 模拟数据转换
     */
    core::result_t<int> parse_id(const std::string& raw) {
        if (raw.empty()) {
            auto code = core::error_builder_t::make_error_code(
                core::error_level_t::error, domain::system_domain_t::application, 0, 0, 2);
            return core::error_context_t(code, "ID 解析失败");
        }
        return 42;
    }

    /**
     * @brief 演示 result_t 的链式处理
     */
    void demo_result_chain() {
        std::cout << "\n========== 5. result_t 链式处理 ==========\n";

        auto result = query_database("SELECT * FROM users");
        if (result.is_error()) {
            std::cout << "查询失败: " << result.error().message << "\n";
        } else {
            std::cout << "查询成功: " << result.value() << "\n";
        }

        auto bad_result = query_database("");
        if (bad_result.is_error()) {
            std::cout << "查询失败: " << bad_result.error().message << "\n";
        }
    }

    // ========================================================================
    // 第 6 部分：错误配置
    // ========================================================================

    /**
     * @brief 演示错误配置功能
     */
    void demo_error_config_t() {
        std::cout << "\n========== 6. 错误配置 ==========\n";

        std::cout << "当前堆栈等级: " << core::to_string(core::error_config_t::get_stacktrace_level()) << "\n";
        std::cout << "当前默认语言: " << core::error_config_t::get_default_language() << "\n";

        core::error_config_t::set_stacktrace_level(core::error_level_t::warn);
        core::error_config_t::set_default_language("en_us");

        std::cout << "修改后堆栈等级: " << core::to_string(core::error_config_t::get_stacktrace_level()) << "\n";
        std::cout << "修改后默认语言: " << core::error_config_t::get_default_language() << "\n";

        core::error_config_t::set_stacktrace_level(core::error_level_t::error);
        core::error_config_t::set_default_language("zh_cn");
    }

}  // namespace demo

// ========================================================================
// 主函数
// ========================================================================

int main() {
    std::cout << "error_system 高级功能演示\n";
    std::cout << "========================";

    demo::demo_payload();
    demo::demo_stack_trace();
    demo::demo_i18n();
    demo::demo_plugin();
    demo::demo_result_chain();
    demo::demo_error_config_t();

    std::cout << "\n演示结束\n";
    return 0;
}
