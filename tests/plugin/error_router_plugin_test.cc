#include "error_system/core/error_builder.h"
#include "error_system/plugin/error_router_plugin.h"
#include <gtest/gtest.h>

namespace error_system::plugin {

    class error_router_plugin_test : public ::testing::Test {
        protected:
        void SetUp() override {
            error_router_plugin_t::instance().unregister_handler_by_code(0);
            error_router_plugin_t::instance().unregister_handler_by_module_group_id(0);
            error_router_plugin_t::instance().unregister_handler_by_domain(domain::system_domain_t::none);
        }
    };

    TEST_F(error_router_plugin_test, instance_returns_singleton) {
        auto& instance1 = error_router_plugin_t::instance();
        auto& instance2 = error_router_plugin_t::instance();
        EXPECT_EQ(&instance1, &instance2);
    }

    TEST_F(error_router_plugin_test, name_returns_expected_value) {
        auto name = error_router_plugin_t::instance().name();
        EXPECT_EQ(name, "global_error_router");
    }

    TEST_F(error_router_plugin_test, register_handler_by_code_and_trigger) {
        bool handler_called = false;
        core::code_t target_code = 12345;

        error_router_plugin_t::instance().register_handler_by_code(
            target_code, [&handler_called](const core::error_context_t&) { handler_called = true; });

        // Use default constructed context and set code directly
        core::error_context_t ctx;
        ctx.code = core::error_code_t(target_code);
        ctx.message = "test";
        error_router_plugin_t::instance().on_error(ctx);

        EXPECT_TRUE(handler_called);
    }

    TEST_F(error_router_plugin_test, register_handler_by_domain_and_trigger) {
        bool handler_called = false;

        error_router_plugin_t::instance().register_handler_by_domain(
            domain::system_domain_t::database,
            [&handler_called](const core::error_context_t&) { handler_called = true; });

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        // Use default constructed context and set code directly
        core::error_context_t ctx;
        ctx.code = code;
        ctx.message = "test";
        error_router_plugin_t::instance().on_error(ctx);

        EXPECT_TRUE(handler_called);
    }

    TEST_F(error_router_plugin_test, handler_receives_correct_context) {
        core::code_t received_code = 0;

        error_router_plugin_t::instance().register_handler_by_code(
            99999, [&received_code](const core::error_context_t& ctx) { received_code = ctx.code.get_code(); });

        // Use default constructed context and set code directly
        core::error_context_t ctx;
        ctx.code = core::error_code_t(99999);
        ctx.message = "test";
        error_router_plugin_t::instance().on_error(ctx);

        EXPECT_EQ(received_code, 99999);
    }

    TEST_F(error_router_plugin_test, unregister_handler_by_code_removes_it) {
        bool handler_called = false;

        error_router_plugin_t::instance().register_handler_by_code(
            11111, [&handler_called](const core::error_context_t&) { handler_called = true; });

        error_router_plugin_t::instance().unregister_handler_by_code(11111);

        // Use default constructed context and set code directly
        core::error_context_t ctx;
        ctx.code = core::error_code_t(11111);
        ctx.message = "test";
        error_router_plugin_t::instance().on_error(ctx);

        EXPECT_FALSE(handler_called);
    }

    TEST_F(error_router_plugin_test, unregister_handler_by_domain_removes_it) {
        bool handler_called = false;

        error_router_plugin_t::instance().register_handler_by_domain(
            domain::system_domain_t::middleware,
            [&handler_called](const core::error_context_t&) { handler_called = true; });

        error_router_plugin_t::instance().unregister_handler_by_domain(domain::system_domain_t::middleware);

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::middleware, 1, 1, 1);

        // Use default constructed context and set code directly
        core::error_context_t ctx;
        ctx.code = code;
        ctx.message = "test";
        error_router_plugin_t::instance().on_error(ctx);

        EXPECT_FALSE(handler_called);
    }

    TEST_F(error_router_plugin_test, code_handler_takes_priority_over_domain) {
        bool code_handler_called = false;
        bool domain_handler_called = false;

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::application, 1, 1, 1);

        error_router_plugin_t::instance().register_handler_by_domain(
            domain::system_domain_t::application,
            [&domain_handler_called](const core::error_context_t&) { domain_handler_called = true; });

        error_router_plugin_t::instance().register_handler_by_code(
            code.get_code(), [&code_handler_called](const core::error_context_t&) { code_handler_called = true; });

        // Use default constructed context and set code directly
        core::error_context_t ctx;
        ctx.code = code;
        ctx.message = "test";
        error_router_plugin_t::instance().on_error(ctx);

        EXPECT_TRUE(code_handler_called);
    }

    TEST_F(error_router_plugin_test, no_handler_does_not_crash) {
        // Use default constructed context and set code directly
        core::error_context_t ctx;
        ctx.code = core::error_code_t(12345);
        ctx.message = "test";

        EXPECT_NO_THROW(error_router_plugin_t::instance().on_error(ctx));
    }

}  // namespace error_system::plugin
