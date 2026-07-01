#include "error_system/plugin/error_router_plugin.h"

#include <gtest/gtest.h>

namespace error_system::plugin {

    using error_system::core::error_code_t;

    class error_router_plugin_test_t : public ::testing::Test {
        protected:
        static constexpr core::code_t TEST_CODE_A = 12345;
        static constexpr core::code_t TEST_CODE_B = 99999;
        static constexpr core::code_t TEST_CODE_C = 11111;

        void SetUp() override {
            error_router_plugin_t::instance().unregister_handler_by_code(core::error_code_t(0));
            error_router_plugin_t::instance().unregister_handler_by_module_group_id(0);
            error_router_plugin_t::instance().unregister_handler_by_domain(domain::system_domain_t::none);

            auto& registry = error_system::core::error_registry_t::instance();
            registry.register_error(core::error_code_t(TEST_CODE_A), "TEST_CODE_A", "test");
            registry.register_error(core::error_code_t(TEST_CODE_B), "TEST_CODE_B", "test");
            registry.register_error(core::error_code_t(TEST_CODE_C), "TEST_CODE_C", "test");
            registry.register_error(error_code_t(core::error_level_t::error, domain::system_domain_t::database, core::subsystem_id_t{1}, core::module_id_t{1}, core::error_number_t{1}),
                "TEST_CODE_DB", "test");
            registry.register_error(error_code_t(core::error_level_t::error, domain::system_domain_t::middleware, core::subsystem_id_t{1}, core::module_id_t{1}, core::error_number_t{1}),
                "TEST_CODE_MW", "test");
            registry.register_error(error_code_t(core::error_level_t::error, domain::system_domain_t::application, core::subsystem_id_t{1}, core::module_id_t{1}, core::error_number_t{1}),
                "TEST_CODE_APP", "test");
        }

        void TearDown() override {
            error_router_plugin_t::instance().unregister_handler_by_code(core::error_code_t(TEST_CODE_A));
            error_router_plugin_t::instance().unregister_handler_by_code(core::error_code_t(TEST_CODE_B));
            error_router_plugin_t::instance().unregister_handler_by_code(core::error_code_t(TEST_CODE_C));
            error_router_plugin_t::instance().unregister_handler_by_domain(domain::system_domain_t::database);
            error_router_plugin_t::instance().unregister_handler_by_domain(domain::system_domain_t::middleware);
            error_router_plugin_t::instance().unregister_handler_by_domain(domain::system_domain_t::application);
        }
    };

    TEST_F(error_router_plugin_test_t, instance_returns_singleton) {
        auto& instance1 = error_router_plugin_t::instance();
        auto& instance2 = error_router_plugin_t::instance();
        EXPECT_EQ(&instance1, &instance2);
    }

    TEST_F(error_router_plugin_test_t, name_returns_expected_value) {
        auto name = error_router_plugin_t::instance().name();
        EXPECT_EQ(name, "global_error_router");
    }

    TEST_F(error_router_plugin_test_t, register_handler_by_code_and_trigger) {
        bool handler_called = false;
        core::error_code_t target_code(TEST_CODE_A);

        error_router_plugin_t::instance().register_handler_by_code(
            target_code, [&handler_called](const core::error_context_t&) { handler_called = true; });

        core::error_context_t context(core::located_code_t{target_code}, "test");
        error_router_plugin_t::instance().on_error(context);

        EXPECT_TRUE(handler_called);
    }

    TEST_F(error_router_plugin_test_t, register_handler_by_domain_and_trigger) {
        bool handler_called = false;

        error_router_plugin_t::instance().register_handler_by_domain(
            domain::system_domain_t::database,
            [&handler_called](const core::error_context_t&) { handler_called = true; });

        auto code = error_code_t(core::error_level_t::error, domain::system_domain_t::database, core::subsystem_id_t{1}, core::module_id_t{1}, core::error_number_t{1});

        core::error_context_t context(core::located_code_t{code}, "test");
        error_router_plugin_t::instance().on_error(context);

        EXPECT_TRUE(handler_called);
    }

    TEST_F(error_router_plugin_test_t, handler_receives_correct_context) {
        core::code_t received_code = 0;
        core::error_code_t target_code(TEST_CODE_B);

        error_router_plugin_t::instance().register_handler_by_code(
            target_code, [&received_code](const core::error_context_t& context) {
                received_code = context.get_code().get_code();
            });

        core::error_context_t context(core::located_code_t{target_code}, "test");
        error_router_plugin_t::instance().on_error(context);

        EXPECT_EQ(received_code, TEST_CODE_B);
    }

    TEST_F(error_router_plugin_test_t, unregister_handler_by_code_removes_it) {
        bool handler_called = false;
        core::error_code_t target_code(TEST_CODE_C);

        error_router_plugin_t::instance().register_handler_by_code(
            target_code, [&handler_called](const core::error_context_t&) { handler_called = true; });

        error_router_plugin_t::instance().unregister_handler_by_code(target_code);

        core::error_context_t context(core::located_code_t{target_code}, "test");
        error_router_plugin_t::instance().on_error(context);

        EXPECT_FALSE(handler_called);
    }

    TEST_F(error_router_plugin_test_t, unregister_handler_by_domain_removes_it) {
        bool handler_called = false;

        error_router_plugin_t::instance().register_handler_by_domain(
            domain::system_domain_t::middleware,
            [&handler_called](const core::error_context_t&) { handler_called = true; });

        error_router_plugin_t::instance().unregister_handler_by_domain(domain::system_domain_t::middleware);

        auto code = error_code_t(core::error_level_t::error, domain::system_domain_t::middleware, core::subsystem_id_t{1}, core::module_id_t{1}, core::error_number_t{1});

        core::error_context_t context(core::located_code_t{code}, "test");
        error_router_plugin_t::instance().on_error(context);

        EXPECT_FALSE(handler_called);
    }

    TEST_F(error_router_plugin_test_t, code_handler_takes_priority_over_domain) {
        bool code_handler_called = false;
        bool domain_handler_called = false;

        auto code = error_code_t(core::error_level_t::error, domain::system_domain_t::application, core::subsystem_id_t{1}, core::module_id_t{1}, core::error_number_t{1});

        error_router_plugin_t::instance().register_handler_by_domain(
            domain::system_domain_t::application,
            [&domain_handler_called](const core::error_context_t&) { domain_handler_called = true; });

        error_router_plugin_t::instance().register_handler_by_code(
            code, [&code_handler_called](const core::error_context_t&) { code_handler_called = true; });

        core::error_context_t context(core::located_code_t{code}, "test");
        error_router_plugin_t::instance().on_error(context);

        EXPECT_TRUE(code_handler_called);
    }

    TEST_F(error_router_plugin_test_t, no_handler_does_not_crash) {
        core::error_context_t context(core::located_code_t{core::error_code_t(TEST_CODE_A)}, "test");

        EXPECT_NO_THROW(error_router_plugin_t::instance().on_error(context));
    }

}  // namespace error_system::plugin
