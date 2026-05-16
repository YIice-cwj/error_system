#include "error_system/config/error_config.h"
#include "error_system/core/error_context.h"
#include <gtest/gtest.h>

namespace error_system::config {

    class error_config_test : public ::testing::Test {};

    TEST_F(error_config_test, set_and_get_custom_formatter) {
        bool formatter_called = false;
        formatter_callback_t formatter = [&formatter_called](const core::error_context_t&) -> std::string {
            formatter_called = true;
            return "custom formatted";
        };

        error_config_t::set_custom_formatter(formatter);
        auto retrieved = error_config_t::get_custom_formatter();

        ASSERT_NE(retrieved, nullptr);
        core::error_context_t ctx;
        retrieved(ctx);
        EXPECT_TRUE(formatter_called);
    }

    TEST_F(error_config_test, custom_formatter_can_be_null) {
        error_config_t::set_custom_formatter(nullptr);
        auto retrieved = error_config_t::get_custom_formatter();
        EXPECT_EQ(retrieved, nullptr);
    }

    TEST_F(error_config_test, stacktrace_level_defaults) {
        auto level = error_config_t::get_stacktrace_level();
        EXPECT_GE(static_cast<int>(level), 0);
    }

    TEST_F(error_config_test, set_stacktrace_level_works) {
        auto original = error_config_t::get_stacktrace_level();

        error_config_t::set_stacktrace_level(core::error_level_t::fatal);
        EXPECT_EQ(error_config_t::get_stacktrace_level(), core::error_level_t::fatal);

        error_config_t::set_stacktrace_level(original);
    }

    TEST_F(error_config_test, enable_stacktrace_can_be_toggled) {
        error_config_t::set_enable_stacktrace(true);
        EXPECT_TRUE(error_config_t::is_stacktrace_enabled());

        error_config_t::set_enable_stacktrace(false);
        EXPECT_FALSE(error_config_t::is_stacktrace_enabled());
    }

    TEST_F(error_config_test, validation_can_be_toggled) {
        error_config_t::set_enable_validation(true);
        EXPECT_TRUE(error_config_t::is_validation_enabled());

        error_config_t::set_enable_validation(false);
        EXPECT_FALSE(error_config_t::is_validation_enabled());
    }

    TEST_F(error_config_test, source_location_can_be_toggled) {
        error_config_t::set_enable_source_location(true);
        EXPECT_TRUE(error_config_t::is_source_location_enabled());

        error_config_t::set_enable_source_location(false);
        EXPECT_FALSE(error_config_t::is_source_location_enabled());
    }

    TEST_F(error_config_test, short_filename_can_be_toggled) {
        error_config_t::set_enable_short_filename(true);
        EXPECT_TRUE(error_config_t::is_short_filename_enabled());

        error_config_t::set_enable_short_filename(false);
        EXPECT_FALSE(error_config_t::is_short_filename_enabled());
    }

}  // namespace error_system::config
