#include "error_system/utils/json_utils.h"

#include <sys/stat.h>

#include <filesystem>
#include <random>

#include <gtest/gtest.h>

#include "error_system/utils/file_utils.h"

namespace error_system::utils {

    class json_dict_test_t : public ::testing::Test {
        protected:
        std::filesystem::path temp_dir_;

        void SetUp() override {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(100000, 999999);
            temp_dir_ = std::filesystem::temp_directory_path()
                        / ("error_system_test_" + std::to_string(dis(gen)));
            std::filesystem::create_directories(temp_dir_);
            chmod(temp_dir_.c_str(), 0700);
        }

        void TearDown() override { std::filesystem::remove_all(temp_dir_); }
    };

    TEST_F(json_dict_test_t, parse_empty_string_returns_nullopt) {
        auto result = json_dict_t::parse("");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test_t, parse_valid_flat_json) {
        auto result = json_dict_t::parse(R"({"key1": "value1", "key2": "value2"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 2UL);
        EXPECT_EQ(result->get_value("key1"), "value1");
        EXPECT_EQ(result->get_value("key2"), "value2");
    }

    TEST_F(json_dict_test_t, parse_valid_nested_json) {
        auto result = json_dict_t::parse(R"({"outer": {"inner": "nested_value"}})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 1UL);
        EXPECT_EQ(result->get_value("outer.inner"), "nested_value");
    }

    TEST_F(json_dict_test_t, parse_invalid_json_returns_nullopt) {
        auto result = json_dict_t::parse(R"({"key": })");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test_t, parse_missing_value_returns_nullopt) {
        auto result = json_dict_t::parse(R"({"key"})");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test_t, parse_missing_colon_returns_nullopt) {
        auto result = json_dict_t::parse(R"({"key" "value"})");
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test_t, get_value_returns_nullopt_for_missing_key) {
        auto result = json_dict_t::parse(R"({"key1": "value1"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->get_value("nonexistent").has_value());
    }

    TEST_F(json_dict_test_t, contains_returns_true_for_existing_key) {
        auto result = json_dict_t::parse(R"({"key1": "value1"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->contains("key1"));
    }

    TEST_F(json_dict_test_t, contains_returns_false_for_missing_key) {
        auto result = json_dict_t::parse(R"({"key1": "value1"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->contains("nonexistent"));
    }

    TEST_F(json_dict_test_t, empty_returns_true_for_empty_dict) {
        json_dict_t dict{};
        EXPECT_TRUE(dict.empty());
    }

    TEST_F(json_dict_test_t, empty_returns_false_for_non_empty_dict) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->empty());
    }

    TEST_F(json_dict_test_t, size_returns_zero_for_empty_dict) {
        json_dict_t dict{};
        EXPECT_EQ(dict.size(), 0UL);
    }

    TEST_F(json_dict_test_t, size_returns_correct_count) {
        auto result = json_dict_t::parse(R"({"a": "1", "b": "2", "c": "3"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 3UL);
    }

    TEST_F(json_dict_test_t, operator_bracket_returns_value) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ((*result)["key"], "value");
    }

    TEST_F(json_dict_test_t, get_value_or_returns_default_for_missing_key) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value_or("missing", "default"), "default");
    }

    TEST_F(json_dict_test_t, get_value_or_returns_value_for_existing_key) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value_or("key", "default"), "value");
    }

    TEST_F(json_dict_test_t, from_file_reads_valid_json_file) {
        auto file_path = temp_dir_ / "test.json";
        file_utils_t::write_file(file_path, R"({"file_key": "file_value"})");

        auto result = json_dict_t::from_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("file_key"), "file_value");
    }

    TEST_F(json_dict_test_t, from_file_returns_nullopt_for_missing_file) {
        auto file_path = temp_dir_ / "nonexistent.json";
        auto result = json_dict_t::from_file(file_path);
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(json_dict_test_t, parse_deeply_nested_json) {
        auto result = json_dict_t::parse(R"({"a": {"b": {"c": {"d": "deep_value"}}}})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("a.b.c.d"), "deep_value");
    }

    TEST_F(json_dict_test_t, parse_multiple_nested_keys) {
        auto result = json_dict_t::parse(R"({"level1": {"key1": "val1"}, "level2": {"key2": "val2"}})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), 2UL);
        EXPECT_EQ(result->get_value("level1.key1"), "val1");
        EXPECT_EQ(result->get_value("level2.key2"), "val2");
    }

    TEST_F(json_dict_test_t, parse_json_with_special_characters_in_values) {
        auto result = json_dict_t::parse(R"({"special": "hello world! @#$%"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("special"), "hello world! @#$%");
    }

    TEST_F(json_dict_test_t, move_constructor_works) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());

        json_dict_t moved = std::move(*result);
        EXPECT_EQ(moved.get_value("key"), "value");
    }

    TEST_F(json_dict_test_t, move_assignment_works) {
        auto result = json_dict_t::parse(R"({"key": "value"})");
        ASSERT_TRUE(result.has_value());

        json_dict_t target{};
        target = std::move(*result);
        EXPECT_EQ(target.get_value("key"), "value");
    }

    class json_serializer_test_t : public ::testing::Test {};

    TEST_F(json_serializer_test_t, escape_json_plain_string) {
        auto result = json_serializer_t::escape_json("hello world");
        EXPECT_EQ(result, "hello world");
    }

    TEST_F(json_serializer_test_t, escape_json_quotes) {
        auto result = json_serializer_t::escape_json("\"quoted\"");
        EXPECT_EQ(result, "\\\"quoted\\\"");
    }

    TEST_F(json_serializer_test_t, escape_json_backslash) {
        auto result = json_serializer_t::escape_json("path\\to\\file");
        EXPECT_EQ(result, "path\\\\to\\\\file");
    }

    TEST_F(json_serializer_test_t, escape_json_control_chars) {
        auto result = json_serializer_t::escape_json("line1\nline2\ttab");
        EXPECT_EQ(result, "line1\\nline2\\ttab");
    }

    TEST_F(json_serializer_test_t, escape_json_empty_string) {
        auto result = json_serializer_t::escape_json("");
        EXPECT_TRUE(result.empty());
    }

    // ========== UTF-16 代理对 / \uXXXX 转义测试 ==========
    //
    // 以下测试覆盖 json_lexer 对 \uXXXX 转义的处理，重点验证：
    //   1. BMP 范围内的 \uXXXX 正确编码为 UTF-8
    //   2. UTF-16 代理对（\uD83D\uDE00）正确组合为补充平面码点并编码为 4 字节 UTF-8
    //   3. 孤立的高 / 低代理被静默丢弃，不影响其余字符解析

    TEST_F(json_dict_test_t, parse_unicode_escape_basic_bmp) {
        // \u0041 = 'A'，\u4e2d = '中'（BMP 内的 CJK 字符）
        auto result = json_dict_t::parse(R"({"key": "\u0041\u4e2d"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("key"), "A中");
    }

    TEST_F(json_dict_test_t, parse_unicode_escape_supplementary_plane_surrogate_pair) {
        // U+1F600 (😀) 的 UTF-16 编码为代理对 0xD83D 0xDE00
        // 期望解析后得到 4 字节 UTF-8: F0 9F 98 80
        auto result = json_dict_t::parse(R"({"emoji": "\uD83D\uDE00"})");
        ASSERT_TRUE(result.has_value());
        const auto value = result->get_value("emoji");
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value->size(), 4u);
        EXPECT_EQ(static_cast<unsigned char>((*value)[0]), 0xF0);
        EXPECT_EQ(static_cast<unsigned char>((*value)[1]), 0x9F);
        EXPECT_EQ(static_cast<unsigned char>((*value)[2]), 0x98);
        EXPECT_EQ(static_cast<unsigned char>((*value)[3]), 0x80);
    }

    TEST_F(json_dict_test_t, parse_unicode_escape_isolated_high_surrogate_dropped) {
        // 孤立的高代理（无后续低代理）应被丢弃，前后字符正常保留
        auto result = json_dict_t::parse(R"({"text": "a\uD83Db"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("text"), "ab");
    }

    TEST_F(json_dict_test_t, parse_unicode_escape_isolated_low_surrogate_dropped) {
        // 孤立的低代理（无前导高代理）应被丢弃
        auto result = json_dict_t::parse(R"({"text": "a\uDC00b"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("text"), "ab");
    }

    TEST_F(json_dict_test_t, parse_unicode_escape_high_surrogate_without_low_followed_by_plain) {
        // 高代理后跟普通字符（非 \u 转义）应丢弃高代理，普通字符保留
        auto result = json_dict_t::parse(R"({"text": "\uD83DX"})");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->get_value("text"), "X");
    }

    TEST_F(json_dict_test_t, parse_unicode_escape_invalid_hex_returns_nullopt) {
        // 非法十六进制应使整个解析失败
        auto result = json_dict_t::parse(R"({"text": "\uGGGG"})");
        EXPECT_FALSE(result.has_value());
    }

}  // namespace error_system::utils
