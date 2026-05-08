#include "error_system/core/error_builder.h"
#include "error_system/core/error_config.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_level.h"
#include "error_system/domain/system_domain.h"
#include "error_system/plugin/error_router_plugin.h"
#include "error_system/plugin/plugin_registry.h"

#include <atomic>
#include <gtest/gtest.h>
#include <string>

namespace error_system::plugin {

    class error_router_plugin_test : public ::testing::Test {
        protected:
        void SetUp() override {
            plugin_registry_t::instance().clear();
            core::error_config_t::set_enable_validation(false);
        }

        void TearDown() override { plugin_registry_t::instance().clear(); }
    };

    TEST_F(error_router_plugin_test, instance_returns_singleton) {
        auto& instance1 = error_router_plugin_t::instance();
        auto& instance2 = error_router_plugin_t::instance();
        EXPECT_EQ(&instance1, &instance2);
    }

    TEST_F(error_router_plugin_test, name_returns_expected_value) {
        auto& router = error_router_plugin_t::instance();
        EXPECT_EQ(router.name(), "global_error_router");
    }

    TEST_F(error_router_plugin_test, register_handler_by_code_triggers_on_error) {
        auto& router = error_router_plugin_t::instance();
        std::atomic<bool> handler_called{false};

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::system, 0xAB, 0xCD, 0x1234);

        router.register_handler_by_code(code.get_code(), [&](const core::error_context_t&) { handler_called = true; });

        plugin_registry_t::instance().register_plugin(&router);

        core::error_context_t ctx(code, "test message");

        EXPECT_TRUE(handler_called);

        router.unregister_handler_by_code(code.get_code());
    }

    TEST_F(error_router_plugin_test, register_handler_by_domain_triggers_on_error) {
        auto& router = error_router_plugin_t::instance();
        std::atomic<bool> handler_called{false};

        auto test_domain = domain::system_domain_t::database;

        router.register_handler_by_domain(test_domain, [&](const core::error_context_t& ctx) {
            handler_called = true;
            EXPECT_EQ(ctx.code.get_system(), test_domain);
        });

        plugin_registry_t::instance().register_plugin(&router);

        auto code = core::error_builder_t::make_error_code(core::error_level_t::error, test_domain, 0xAA, 0xBB, 0xCCDD);
        core::error_context_t ctx(code, "database error");

        EXPECT_TRUE(handler_called);

        router.unregister_handler_by_domain(test_domain);
    }

    TEST_F(error_router_plugin_test, code_handler_takes_priority_over_domain_handler) {
        auto& router = error_router_plugin_t::instance();
        std::atomic<int> code_handler_count{0};
        std::atomic<int> domain_handler_count{0};

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::network, 0x11, 0x22, 0x3344);

        router.register_handler_by_code(code.get_code(), [&](const core::error_context_t&) { ++code_handler_count; });

        router.register_handler_by_domain(domain::system_domain_t::network,
                                          [&](const core::error_context_t&) { ++domain_handler_count; });

        plugin_registry_t::instance().register_plugin(&router);

        core::error_context_t ctx(code, "network error");

        EXPECT_EQ(code_handler_count, 1);
        EXPECT_EQ(domain_handler_count, 0);

        router.unregister_handler_by_code(code.get_code());
        router.unregister_handler_by_domain(domain::system_domain_t::network);
    }

    TEST_F(error_router_plugin_test, unregister_handler_by_code_removes_handler) {
        auto& router = error_router_plugin_t::instance();
        std::atomic<int> handler_count{0};

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::system, 0x33, 0x44, 0x5566);

        router.register_handler_by_code(code.get_code(), [&](const core::error_context_t&) { ++handler_count; });

        router.unregister_handler_by_code(code.get_code());

        plugin_registry_t::instance().register_plugin(&router);

        core::error_context_t ctx(code, "test");

        EXPECT_EQ(handler_count, 0);
    }

    TEST_F(error_router_plugin_test, unregister_handler_by_domain_removes_handler) {
        auto& router = error_router_plugin_t::instance();
        std::atomic<int> handler_count{0};

        auto test_domain = domain::system_domain_t::security;

        router.register_handler_by_domain(test_domain, [&](const core::error_context_t&) { ++handler_count; });

        router.unregister_handler_by_domain(test_domain);

        plugin_registry_t::instance().register_plugin(&router);

        auto code = core::error_builder_t::make_error_code(core::error_level_t::error, test_domain, 0x55, 0x66, 0x7788);
        core::error_context_t ctx(code, "security error");

        EXPECT_EQ(handler_count, 0);
    }

    TEST_F(error_router_plugin_test, no_handler_called_for_unregistered_code) {
        auto& router = error_router_plugin_t::instance();
        std::atomic<bool> handler_called{false};

        auto registered_code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::system, 0x77, 0x88, 0x99AA);
        auto unregistered_code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::system, 0xBB, 0xCC, 0xDDEE);

        router.register_handler_by_code(registered_code.get_code(),
                                        [&](const core::error_context_t&) { handler_called = true; });

        plugin_registry_t::instance().register_plugin(&router);

        core::error_context_t ctx(unregistered_code, "unregistered error");

        EXPECT_FALSE(handler_called);

        router.unregister_handler_by_code(registered_code.get_code());
    }

    TEST_F(error_router_plugin_test, handler_receives_correct_context_data) {
        auto& router = error_router_plugin_t::instance();
        core::code_t captured_code{0};
        std::string captured_message;

        auto code = core::error_builder_t::make_error_code(
            core::error_level_t::fatal, domain::system_domain_t::application, 0xEE, 0xFF, 0xAABB);

        router.register_handler_by_code(code.get_code(), [&](const core::error_context_t& ctx) {
            captured_code = ctx.code.get_code();
            captured_message = ctx.message;
        });

        plugin_registry_t::instance().register_plugin(&router);

        core::error_context_t ctx(code, "detailed error message");
        ctx.with("key1", "value1").with("key2", "value2");

        EXPECT_EQ(captured_code, code.get_code());
        EXPECT_EQ(captured_message, "detailed error message");

        router.unregister_handler_by_code(code.get_code());
    }

    TEST_F(error_router_plugin_test, multiple_handlers_can_be_registered) {
        auto& router = error_router_plugin_t::instance();
        std::atomic<int> handler1_count{0};
        std::atomic<int> handler2_count{0};

        auto code1 = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::system, 0x12, 0x34, 0x5678);
        auto code2 = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::system, 0x9A, 0xBC, 0xDEF0);

        router.register_handler_by_code(code1.get_code(), [&](const core::error_context_t&) { ++handler1_count; });

        router.register_handler_by_code(code2.get_code(), [&](const core::error_context_t&) { ++handler2_count; });

        plugin_registry_t::instance().register_plugin(&router);

        core::error_context_t ctx1(code1, "error 1");
        core::error_context_t ctx2(code2, "error 2");

        EXPECT_EQ(handler1_count, 1);
        EXPECT_EQ(handler2_count, 1);

        router.unregister_handler_by_code(code1.get_code());
        router.unregister_handler_by_code(code2.get_code());
    }

    TEST_F(error_router_plugin_test, domain_handler_called_for_matching_system) {
        auto& router = error_router_plugin_t::instance();
        std::atomic<int> database_count{0};
        std::atomic<int> network_count{0};

        router.register_handler_by_domain(domain::system_domain_t::database,
                                          [&](const core::error_context_t&) { ++database_count; });

        router.register_handler_by_domain(domain::system_domain_t::network,
                                          [&](const core::error_context_t&) { ++network_count; });

        plugin_registry_t::instance().register_plugin(&router);

        auto db_code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::database, 0x01, 0x02, 0x0304);
        auto net_code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::network, 0x05, 0x06, 0x0708);

        core::error_context_t ctx1(db_code, "db error");
        core::error_context_t ctx2(net_code, "network error");

        EXPECT_EQ(database_count, 1);
        EXPECT_EQ(network_count, 1);

        router.unregister_handler_by_domain(domain::system_domain_t::database);
        router.unregister_handler_by_domain(domain::system_domain_t::network);
    }

}  // namespace error_system::plugin
