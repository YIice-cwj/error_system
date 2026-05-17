#include "error_system/utils/file_utils.h"
#include "error_system/utils/json_utils.h"
#include <filesystem>
#include <gtest/gtest.h>

namespace error_system::utils {

    class json_dict_test : public ::testing::Test {
        protected:
        std::filesystem::path temp_dir_;

        void SetUp() override {
            temp_dir_ = std::filesystem::temp_directory_path() / "error_system_test";
            std::filesystem::create_directories(temp_dir_);
        }

        void TearDown() override { std::filesystem::remove_all(temp_dir_); }
    };

    TEST_F(json_dict_test, parse_empty_string_returns_nullopt) {
        auto result = json_dict_t::parse("");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test, parse_valid_flat_json) {
        auto result = json_dict_t::parse(R"({"key1": "value1", "key2": "value2"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 2UL);
        EXPECT_EQ(result->get_value("key1"), "value1");
        EXPECT_EQ(result->get_value("key2"), "value2");
    }

    TEST_F(json_dict_test, parse_valid_nested_json) {
        auto result = json_dict_t::parse(R"({"outer": {"inner": "nested_value"}})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 1UL);
        EXPECT_EQ(result->get_value("outer.inner"), "nested_value");
    }

    TEST_F(json_dict_test, parse_invalid_json_returns_nullopt) {
        auto result = json_dict_t::parse(R"({"key": })");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test, parse_missing_value_returns_nullopt) {
        auto result = json_dict_t::parse(R"({"key"})");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test, parse_missing_colon_returns_nullopt) {
        auto result = json_dict_t::parse(R"({"key" "value"})");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test, get_value_returns_nullopt_for_missing_key) {
        auto result = json_dict_t::parse(R"({"key1": "value1"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->get_value("nonexistent").has_value());
    }

    TEST_F(json_dict_test, contains_returns_true_for_existing_key) {
        auto result = json_dict_t::parse(R"({"key1": "value1"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->contains("key1"));
    }

    TEST_F(json_dict_test, contains_returns_false_for_missing_key) {
        auto result = json_dict_t::parse(R"({"key1": "value1"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->contains("nonexistent"));
    }

    TEST_F(json_dict_test, empty_returns_true_for_empty_dict) {
        json_dict_t dict{};
        EXPECT_TRUE(dict.empty());
    }

    TEST_F(json_dict_test, empty_returns_false_for_non_empty_dict) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->empty());
    }

    TEST_F(json_dict_test, size_returns_zero_for_empty_dict) {
        json_dict_t dict{};
        EXPECT_EQ(dict.size(), 0UL);
    }

    TEST_F(json_dict_test, size_returns_correct_count) {
        auto result = json_dict_t::parse(R"({"a": "1", "b": "2", "c": "3"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 3UL);
    }

    TEST_F(json_dict_test, operator_bracket_returns_value) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ((*result)["key"], "value");
    }

    TEST_F(json_dict_test, get_value_or_returns_default_for_missing_key) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value_or("missing", "default"), "default");
    }

    TEST_F(json_dict_test, get_value_or_returns_value_for_existing_key) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value_or("key", "default"), "value");
    }

    TEST_F(json_dict_test, from_file_reads_valid_json_file) {
        auto file_path = temp_dir_ / "test.json";
        file_utils::write_file(file_path, R"({"file_key": "file_value"})");

        auto result = json_dict_t::from_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("file_key"), "file_value");
    }

    TEST_F(json_dict_test, from_file_returns_nullopt_for_missing_file) {
        auto file_path = temp_dir_ / "nonexistent.json";
        auto result = json_dict_t::from_file(file_path);
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test, parse_deeply_nested_json) {
        auto result = json_dict_t::parse(R"({"a": {"b": {"c": {"d": "deep_value"}}}})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("a.b.c.d"), "deep_value");
    }

    TEST_F(json_dict_test, parse_multiple_nested_keys) {
        auto result = json_dict_t::parse(R"({"level1": {"key1": "val1"}, "level2": {"key2": "val2"}})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 2UL);
        EXPECT_EQ(result->get_value("level1.key1"), "val1");
        EXPECT_EQ(result->get_value("level2.key2"), "val2");
    }

    TEST_F(json_dict_test, parse_json_with_special_characters_in_values) {
        auto result = json_dict_t::parse(R"({"special": "hello world! @#$%"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("special"), "hello world! @#$%");
    }

    TEST_F(json_dict_test, move_constructor_works) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());

        json_dict_t moved = std::move(*result);
        EXPECT_EQ(moved.get_value("key"), "value");
    }

    TEST_F(json_dict_test, move_assignment_works) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());

        json_dict_t target{};
        target = std::move(*result);
        EXPECT_EQ(target.get_value("key"), "value");
    }

}  // namespace error_system::utils
