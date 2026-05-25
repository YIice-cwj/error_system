#include "error_system/utils/string_utils.h"
#include <gtest/gtest.h>
#include <string>

namespace error_system::utils {

    class string_utils_test : public ::testing::Test {};

    TEST_F(string_utils_test, hash_empty_string) {
        constexpr auto result = string_utils_t::hash("");
        EXPECT_EQ(result, 14695981039346656037ULL);
    }

    TEST_F(string_utils_test, hash_single_char) {
        constexpr auto result = string_utils_t::hash("a");
        EXPECT_NE(result, 0);
    }

    TEST_F(string_utils_test, hash_same_string_same_result) {
        constexpr auto result1 = string_utils_t::hash("hello");
        constexpr auto result2 = string_utils_t::hash("hello");
        EXPECT_EQ(result1, result2);
    }

    TEST_F(string_utils_test, hash_different_string_different_result) {
        constexpr auto result1 = string_utils_t::hash("hello");
        constexpr auto result2 = string_utils_t::hash("world");
        EXPECT_NE(result1, result2);
    }

    TEST_F(string_utils_test, hash_compile_time_evaluable) {
        constexpr auto result = string_utils_t::hash("constexpr");
        static_assert(result != 0, "hash should be computable at compile time");
        EXPECT_NE(result, 0);
    }

    TEST_F(string_utils_test, hash_limit_truncates_long_string) {
        std::string long_string(200, 'x');
        auto result1 = string_utils_t::hash_limit(long_string, 128);
        auto result2 = string_utils_t::hash_limit(long_string, 64);
        EXPECT_NE(result1, result2);
    }

    TEST_F(string_utils_test, hash_limit_short_string_unchanged) {
        constexpr auto result1 = string_utils_t::hash_limit("short", 128);
        constexpr auto result2 = string_utils_t::hash("short");
        EXPECT_EQ(result1, result2);
    }

    TEST_F(string_utils_test, starts_with_returns_true_for_matching_prefix) {
        EXPECT_TRUE(string_utils_t::starts_with("hello world", "hello"));
    }

    TEST_F(string_utils_test, starts_with_returns_false_for_mismatch) {
        EXPECT_FALSE(string_utils_t::starts_with("hello world", "world"));
    }

    TEST_F(string_utils_test, starts_with_returns_true_for_empty_prefix) {
        EXPECT_TRUE(string_utils_t::starts_with("hello", ""));
    }

    TEST_F(string_utils_test, starts_with_returns_false_for_longer_prefix) {
        EXPECT_FALSE(string_utils_t::starts_with("hi", "hello"));
    }

    TEST_F(string_utils_test, ends_with_returns_true_for_matching_suffix) {
        EXPECT_TRUE(string_utils_t::ends_with("hello world", "world"));
    }

    TEST_F(string_utils_test, ends_with_returns_false_for_mismatch) {
        EXPECT_FALSE(string_utils_t::ends_with("hello world", "hello"));
    }

    TEST_F(string_utils_test, ends_with_returns_true_for_empty_suffix) {
        EXPECT_TRUE(string_utils_t::ends_with("hello", ""));
    }

    TEST_F(string_utils_test, ends_with_returns_false_for_longer_suffix) {
        EXPECT_FALSE(string_utils_t::ends_with("hi", "hello"));
    }

    TEST_F(string_utils_test, replace_all_replaces_single_occurrence) {
        auto result = string_utils_t::replace_all("hello world", "world", "universe");
        EXPECT_EQ(result, "hello universe");
    }

    TEST_F(string_utils_test, replace_all_replaces_multiple_occurrences) {
        auto result = string_utils_t::replace_all("ababab", "ab", "xy");
        EXPECT_EQ(result, "xyxyxy");
    }

    TEST_F(string_utils_test, replace_all_returns_original_when_no_match) {
        auto result = string_utils_t::replace_all("hello", "xyz", "abc");
        EXPECT_EQ(result, "hello");
    }

    TEST_F(string_utils_test, replace_all_handles_empty_from) {
        auto result = string_utils_t::replace_all("hello", "", "x");
        EXPECT_EQ(result, "hello");
    }

    TEST_F(string_utils_test, split_basic) {
        auto result = string_utils_t::split("a,b,c", ",");
        ASSERT_EQ(result.size(), 3);
        EXPECT_EQ(result[0], "a");
        EXPECT_EQ(result[1], "b");
        EXPECT_EQ(result[2], "c");
    }

    TEST_F(string_utils_test, split_empty_string) {
        auto result = string_utils_t::split("", ",");
        EXPECT_TRUE(result.empty());
    }

    TEST_F(string_utils_test, split_no_delimiter) {
        auto result = string_utils_t::split("hello", ",");
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0], "hello");
    }

    TEST_F(string_utils_test, split_consecutive_delimiters) {
        auto result = string_utils_t::split("a,,b", ",");
        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0], "a");
        EXPECT_EQ(result[1], "b");
    }

    TEST_F(string_utils_test, join_basic) {
        std::vector<std::string_view> tokens = {"a", "b", "c"};
        auto result = string_utils_t::join(tokens, ",");
        EXPECT_EQ(result, "a,b,c");
    }

    TEST_F(string_utils_test, join_empty_vector) {
        std::vector<std::string_view> tokens;
        auto result = string_utils_t::join(tokens, ",");
        EXPECT_TRUE(result.empty());
    }

    TEST_F(string_utils_test, join_single_element) {
        std::vector<std::string_view> tokens = {"hello"};
        auto result = string_utils_t::join(tokens, ",");
        EXPECT_EQ(result, "hello");
    }

    TEST_F(string_utils_test, trim_removes_leading_trailing_whitespace) {
        auto result = string_utils_t::trim("  hello  ");
        EXPECT_EQ(result, "hello");
    }

    TEST_F(string_utils_test, trim_removes_all_whitespace_types) {
        auto result = string_utils_t::trim(" \t\n\rhello \t\n\r");
        EXPECT_EQ(result, "hello");
    }

    TEST_F(string_utils_test, trim_empty_string) {
        auto result = string_utils_t::trim("");
        EXPECT_TRUE(result.empty());
    }

    TEST_F(string_utils_test, trim_all_whitespace) {
        auto result = string_utils_t::trim("   \t\n  ");
        EXPECT_TRUE(result.empty());
    }

    TEST_F(string_utils_test, trim_no_whitespace) {
        auto result = string_utils_t::trim("hello");
        EXPECT_EQ(result, "hello");
    }

    TEST_F(string_utils_test, to_lower_converts_uppercase) {
        auto result = string_utils_t::to_lower("HELLO");
        EXPECT_EQ(result, "hello");
    }

    TEST_F(string_utils_test, to_lower_mixed_case) {
        auto result = string_utils_t::to_lower("HeLLo");
        EXPECT_EQ(result, "hello");
    }

    TEST_F(string_utils_test, to_lower_already_lowercase) {
        auto result = string_utils_t::to_lower("hello");
        EXPECT_EQ(result, "hello");
    }

    TEST_F(string_utils_test, to_upper_converts_lowercase) {
        auto result = string_utils_t::to_upper("hello");
        EXPECT_EQ(result, "HELLO");
    }

    TEST_F(string_utils_test, to_upper_mixed_case) {
        auto result = string_utils_t::to_upper("HeLLo");
        EXPECT_EQ(result, "HELLO");
    }

    TEST_F(string_utils_test, to_upper_already_uppercase) {
        auto result = string_utils_t::to_upper("HELLO");
        EXPECT_EQ(result, "HELLO");
    }

    TEST_F(string_utils_test, parse_number_valid_integer) {
        auto result = string_utils_t::parse_number<int>("42");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 42);
    }

    TEST_F(string_utils_test, parse_number_valid_float) {
        auto result = string_utils_t::parse_number<double>("3.14");
        ASSERT_TRUE(result.has_value());
        EXPECT_DOUBLE_EQ(result.value(), 3.14);
    }

    TEST_F(string_utils_test, parse_number_invalid) {
        auto result = string_utils_t::parse_number<int>("abc");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(string_utils_test, parse_number_empty) {
        auto result = string_utils_t::parse_number<int>("");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(string_utils_test, format_replaces_single_placeholder) {
        auto result = string_utils_t::format("hello {}", "world");
        EXPECT_EQ(result, "hello world");
    }

    TEST_F(string_utils_test, format_replaces_multiple_placeholders) {
        auto result = string_utils_t::format("{} + {} = {}", 1, 2, 3);
        EXPECT_EQ(result, "1 + 2 = 3");
    }

    TEST_F(string_utils_test, format_handles_integers) {
        auto result = string_utils_t::format("number: {}", 42);
        EXPECT_EQ(result, "number: 42");
    }

    TEST_F(string_utils_test, format_handles_floats) {
        auto result = string_utils_t::format("pi: {}", 3.14);
        EXPECT_EQ(result, "pi: 3.14");
    }

    TEST_F(string_utils_test, format_handles_no_placeholders) {
        auto result = string_utils_t::format("no placeholders");
        EXPECT_EQ(result, "no placeholders");
    }

    TEST_F(string_utils_test, format_handles_empty_string) {
        auto result = string_utils_t::format("");
        EXPECT_TRUE(result.empty());
    }

    TEST_F(string_utils_test, format_handles_more_placeholders_than_args) {
        auto result = string_utils_t::format("{} {}", "only_one");
        EXPECT_EQ(result, "only_one {}");
    }

    TEST_F(string_utils_test, format_handles_empty_arg) {
        auto result = string_utils_t::format("start{}end", "");
        EXPECT_EQ(result, "startend");
    }

    TEST_F(string_utils_test, format_escapes_double_braces) {
        auto result = string_utils_t::format("{{hello}}", "world");
        EXPECT_EQ(result, "{hello}");
    }

    TEST_F(string_utils_test, format_mixed_placeholders_and_escaped_braces) {
        auto result = string_utils_t::format("{{}} {}", 42);
        EXPECT_EQ(result, "{} 42");
    }

    TEST_F(string_utils_test, format_escapes_only_left_brace) {
        auto result = string_utils_t::format("{{", "x");
        EXPECT_EQ(result, "{");
    }

    TEST_F(string_utils_test, format_escapes_only_right_brace) {
        auto result = string_utils_t::format("}}", "x");
        EXPECT_EQ(result, "}");
    }

}  // namespace error_system::utils
