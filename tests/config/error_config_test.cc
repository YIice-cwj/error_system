#include "error_system/config/error_config.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_context.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

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

    TEST_F(error_config_test, concurrent_set_and_get_stacktrace_level) {
        auto original = error_config_t::get_stacktrace_level();

        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 100; ++j) {
                    error_config_t::set_stacktrace_level(core::error_level_t::error);
                    auto level = error_config_t::get_stacktrace_level();
                    if (level == core::error_level_t::error) {
                        success_count.fetch_add(1);
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        EXPECT_EQ(success_count.load(), 1000);
        error_config_t::set_stacktrace_level(original);
    }

    TEST_F(error_config_test, set_and_get_notify_mode) {
        error_config_t::set_notify_mode(error_config_t::notify_mode_t::sync);
        EXPECT_EQ(error_config_t::get_notify_mode(), error_config_t::notify_mode_t::sync);

        error_config_t::set_notify_mode(error_config_t::notify_mode_t::async_queue);
        EXPECT_EQ(error_config_t::get_notify_mode(), error_config_t::notify_mode_t::async_queue);

        // 恢复默认
        error_config_t::set_notify_mode(error_config_t::notify_mode_t::sync);
    }

    TEST_F(error_config_test, per_code_stacktrace_level_set_and_get) {
        uint64_t id1 = 0x123450000000001ULL;
        uint64_t id2 = 0x543210000000001ULL;

        // 设置 per-code 等级
        error_config_t::set_per_code_stacktrace_level(id1, core::error_level_t::warn);
        error_config_t::set_per_code_stacktrace_level(id2, core::error_level_t::fatal);

        // 获取已设置的
        auto level1 = error_config_t::get_per_code_stacktrace_level(id1);
        ASSERT_TRUE(level1.has_value());
        EXPECT_EQ(level1.value(), core::error_level_t::warn);

        auto level2 = error_config_t::get_per_code_stacktrace_level(id2);
        ASSERT_TRUE(level2.has_value());
        EXPECT_EQ(level2.value(), core::error_level_t::fatal);

        // 获取未设置的值
        auto level3 = error_config_t::get_per_code_stacktrace_level(0xDEAD);
        EXPECT_FALSE(level3.has_value());
    }

    TEST_F(error_config_test, per_code_stacktrace_level_remove) {
        uint64_t id = 0xABCD0000000001ULL;

        error_config_t::set_per_code_stacktrace_level(id, core::error_level_t::warn);
        ASSERT_TRUE(error_config_t::get_per_code_stacktrace_level(id).has_value());

        error_config_t::remove_per_code_stacktrace_level(id);
        EXPECT_FALSE(error_config_t::get_per_code_stacktrace_level(id).has_value());
    }

    TEST_F(error_config_test, per_code_stacktrace_level_overwrite) {
        uint64_t id = 0xDEAD0000000001ULL;

        error_config_t::set_per_code_stacktrace_level(id, core::error_level_t::warn);
        error_config_t::set_per_code_stacktrace_level(id, core::error_level_t::fatal);

        auto level = error_config_t::get_per_code_stacktrace_level(id);
        ASSERT_TRUE(level.has_value());
        EXPECT_EQ(level.value(), core::error_level_t::fatal);  // 后设覆盖前设

        error_config_t::remove_per_code_stacktrace_level(id);
    }

}  // namespace error_system::config
