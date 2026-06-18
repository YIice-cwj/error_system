#include "error_system/core/error_level.h"
#include <gtest/gtest.h>

namespace error_system::core {

    class error_level_test_t : public ::testing::Test {};

    TEST_F(error_level_test_t, to_int_converts_correctly) {
        EXPECT_EQ(to_int(error_level_t::debug), 0);
        EXPECT_EQ(to_int(error_level_t::info), 1);
        EXPECT_EQ(to_int(error_level_t::warn), 2);
        EXPECT_EQ(to_int(error_level_t::error), 3);
        EXPECT_EQ(to_int(error_level_t::fatal), 4);
    }

    TEST_F(error_level_test_t, from_int_converts_correctly) {
        EXPECT_EQ(from_int(0), error_level_t::debug);
        EXPECT_EQ(from_int(1), error_level_t::info);
        EXPECT_EQ(from_int(2), error_level_t::warn);
        EXPECT_EQ(from_int(3), error_level_t::error);
        EXPECT_EQ(from_int(4), error_level_t::fatal);
    }

    TEST_F(error_level_test_t, from_int_invalid_returns_fatal) {
        EXPECT_EQ(from_int(5), error_level_t::fatal);
        EXPECT_EQ(from_int(255), error_level_t::fatal);
    }

    TEST_F(error_level_test_t, is_valid_checks_correctly) {
        EXPECT_TRUE(is_valid(0));
        EXPECT_TRUE(is_valid(1));
        EXPECT_TRUE(is_valid(2));
        EXPECT_TRUE(is_valid(3));
        EXPECT_TRUE(is_valid(4));
        EXPECT_FALSE(is_valid(5));
        EXPECT_FALSE(is_valid(100));
    }

    TEST_F(error_level_test_t, from_string_converts_correctly) {
        EXPECT_EQ(from_string("debug"), error_level_t::debug);
        EXPECT_EQ(from_string("info"), error_level_t::info);
        EXPECT_EQ(from_string("warn"), error_level_t::warn);
        EXPECT_EQ(from_string("error"), error_level_t::error);
        EXPECT_EQ(from_string("fatal"), error_level_t::fatal);
    }

    TEST_F(error_level_test_t, from_string_invalid_returns_info) {
        EXPECT_EQ(from_string("unknown"), error_level_t::info);
        EXPECT_EQ(from_string(""), error_level_t::info);
    }

    TEST_F(error_level_test_t, to_string_converts_correctly) {
        EXPECT_STREQ(to_string(error_level_t::debug), "debug");
        EXPECT_STREQ(to_string(error_level_t::info), "info");
        EXPECT_STREQ(to_string(error_level_t::warn), "warn");
        EXPECT_STREQ(to_string(error_level_t::error), "error");
        EXPECT_STREQ(to_string(error_level_t::fatal), "fatal");
    }

    TEST_F(error_level_test_t, next_level_returns_correct_value) {
        EXPECT_EQ(next_level(error_level_t::debug), error_level_t::info);
        EXPECT_EQ(next_level(error_level_t::info), error_level_t::warn);
        EXPECT_EQ(next_level(error_level_t::warn), error_level_t::error);
        EXPECT_EQ(next_level(error_level_t::error), error_level_t::fatal);
    }

    TEST_F(error_level_test_t, prev_level_returns_correct_value) {
        EXPECT_EQ(prev_level(error_level_t::info), error_level_t::debug);
        EXPECT_EQ(prev_level(error_level_t::warn), error_level_t::info);
        EXPECT_EQ(prev_level(error_level_t::error), error_level_t::warn);
        EXPECT_EQ(prev_level(error_level_t::fatal), error_level_t::error);
    }

    TEST_F(error_level_test_t, should_log_compares_correctly) {
        EXPECT_TRUE(should_log(error_level_t::error, error_level_t::warn));
        EXPECT_TRUE(should_log(error_level_t::fatal, error_level_t::error));
        EXPECT_FALSE(should_log(error_level_t::debug, error_level_t::info));
        EXPECT_TRUE(should_log(error_level_t::info, error_level_t::info));
    }

    TEST_F(error_level_test_t, constexpr_evaluation_works) {
        constexpr auto level = error_level_t::error;
        static_assert(to_int(level) == 3, "constexpr evaluation failed");
        static_assert(is_valid(3), "constexpr evaluation failed");
        EXPECT_EQ(to_int(level), 3);
    }

    TEST_F(error_level_test_t, next_level_at_fatal_returns_fatal) {
        // fatal 的下一级应保持 fatal（无更高级别）
        EXPECT_EQ(next_level(error_level_t::fatal), error_level_t::fatal);
    }

    TEST_F(error_level_test_t, prev_level_at_debug_returns_fatal) {
        // debug 的上一级下溢后 from_int 返回 fatal（无更低级别）
        EXPECT_EQ(prev_level(error_level_t::debug), error_level_t::fatal);
    }

}  // namespace error_system::core
