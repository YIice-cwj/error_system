#include "error_system/config/error_config.h"
#include "error_system/core/error_builder.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_registry.h"
#include "error_system/plugin/plugin_registry.h"
#include <gtest/gtest.h>
#include <string_view>

namespace error_system::core {
    namespace {
        error_code_t make_code(uint16_t number) noexcept {
            return error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 9, 9, number);
        }
    }  // namespace

    class error_context_test : public ::testing::Test {
        protected:
        void SetUp() override {
            auto& registry = error_registry_t::instance();
            registry.unregister_all();
            registry.set_duplicate_policy(duplicate_policy_t::skip);
            registry.set_duplicate_warn_callback(nullptr);

            plugin::plugin_registry_t::instance().clear();
            config::error_config_t::set_custom_formatter(nullptr);
            config::error_config_t::set_enable_text_output(true);
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            config::error_config_t::set_enable_stacktrace(false);
#endif
#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
            config::error_config_t::set_enable_validation(true);
#endif
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
            config::error_config_t::set_enable_source_location(true);
            config::error_config_t::set_enable_short_filename(false);
#endif

            registered_code_ = make_code(1);
            registry.register_subsystem_module(registered_code_.get_subsys(), registered_code_.get_module(), "perf", "ctx");
            registry.register_error(registered_code_, "ERR_CTX_REGISTERED", "Registered error context");
        }

        void TearDown() override {
            plugin::plugin_registry_t::instance().clear();
            error_registry_t::instance().unregister_all();
            config::error_config_t::set_custom_formatter(nullptr);
            config::error_config_t::set_enable_text_output(true);
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            config::error_config_t::set_enable_stacktrace(true);
#endif
#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
            config::error_config_t::set_enable_validation(true);
#endif
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
            config::error_config_t::set_enable_source_location(true);
            config::error_config_t::set_enable_short_filename(true);
#endif
        }

        error_code_t registered_code_{};
    };

    TEST_F(error_context_test, constructor_with_registered_code_keeps_code_and_message) {
        error_context_t context({registered_code_}, "hello {}", "world");

        EXPECT_EQ(context.code.get_code(), registered_code_.get_code());
        EXPECT_EQ(context.message, "hello world");
        EXPECT_TRUE(context.get_payload().empty());
    }

    TEST_F(error_context_test, constructor_with_unregistered_code_marks_context_invalid) {
        const auto unregistered_code = make_code(99);
        error_context_t context({unregistered_code}, "boom");

        EXPECT_EQ(context.code.get_level(), error_level_t::fatal);
        EXPECT_NE(context.message.find("[UNREGISTERED CODE]"), std::string::npos);

        const auto& payload = context.get_payload();
        ASSERT_EQ(payload.count("illegal_raw_code"), 1UL);
        EXPECT_EQ(payload.at("illegal_raw_code"), std::to_string(unregistered_code.get_code()));
    }

    TEST_F(error_context_test, with_string_view_inserts_payload_without_extra_overload_work) {
        error_context_t context({registered_code_}, "payload test");
        context.with(std::string_view("user"), std::string_view("alice"));

        const auto& payload = context.get_payload();
        ASSERT_EQ(payload.count("user"), 1UL);
        EXPECT_EQ(payload.at("user"), "alice");
    }

    TEST_F(error_context_test, serialization_contains_registered_metadata_and_payload) {
        error_context_t context({registered_code_}, "serialize me");
        context.with("user", "alice");

        const std::string text = context.to_string();
        const std::string json = context.to_json();

        EXPECT_NE(text.find("ERR_CTX_REGISTERED"), std::string::npos);
        EXPECT_NE(text.find("Registered error context"), std::string::npos);
        EXPECT_NE(json.find("\"message\":\"serialize me\""), std::string::npos);
        EXPECT_NE(json.find("\"user\":\"alice\""), std::string::npos);
    }
}  // namespace error_system::core
