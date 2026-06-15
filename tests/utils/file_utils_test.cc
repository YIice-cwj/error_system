#include "error_system/utils/file_utils.h"
#include <filesystem>
#include <gtest/gtest.h>

namespace error_system::utils {

    class file_utils_test : public ::testing::Test {
        protected:
        std::filesystem::path temp_dir_;

        void SetUp() override {
            temp_dir_ = std::filesystem::temp_directory_path() / "error_system_file_test";
            std::filesystem::create_directories(temp_dir_);
        }

        void TearDown() override { std::filesystem::remove_all(temp_dir_); }
    };

    TEST_F(file_utils_test, write_file_creates_file_with_content) {
        auto file_path = temp_dir_ / "write_test.txt";
        std::string content = "hello world";

        EXPECT_TRUE(file_utils_t::write_file(file_path, content));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));

        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), content);
    }

    TEST_F(file_utils_test, write_file_creates_parent_directories) {
        auto file_path = temp_dir_ / "sub" / "dir" / "nested.txt";
        std::string content = "nested content";

        EXPECT_TRUE(file_utils_t::write_file(file_path, content));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test, read_file_returns_nullopt_for_missing_file) {
        auto file_path = temp_dir_ / "nonexistent.txt";

        auto result = file_utils_t::read_file(file_path);
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(file_utils_test, read_file_returns_nullopt_for_directory) {
        auto dir_path = temp_dir_ / "a_directory";
        std::filesystem::create_directories(dir_path);

        auto result = file_utils_t::read_file(dir_path);
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(file_utils_test, read_file_handles_empty_file) {
        auto file_path = temp_dir_ / "empty.txt";
        file_utils_t::write_file(file_path, "");

        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result.value().empty());
    }

    TEST_F(file_utils_test, read_file_preserves_binary_content) {
        auto file_path = temp_dir_ / "binary.bin";
        std::string content = std::string{"\x00\x01\x02\x03\xff\xfe", 6};

        file_utils_t::write_file(file_path, content);
        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), content);
    }

    TEST_F(file_utils_test, create_file_creates_new_file) {
        auto file_path = temp_dir_ / "created.txt";

        EXPECT_TRUE(file_utils_t::create_file(file_path));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test, create_file_returns_true_for_existing_file) {
        auto file_path = temp_dir_ / "existing.txt";
        file_utils_t::write_file(file_path, "content");

        EXPECT_TRUE(file_utils_t::create_file(file_path));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test, create_file_creates_parent_directories) {
        auto file_path = temp_dir_ / "deep" / "path" / "file.txt";

        EXPECT_TRUE(file_utils_t::create_file(file_path));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test, delete_file_removes_existing_file) {
        auto file_path = temp_dir_ / "to_delete.txt";
        file_utils_t::write_file(file_path, "delete me");

        EXPECT_TRUE(file_utils_t::delete_file(file_path));
        EXPECT_FALSE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test, delete_file_returns_true_for_already_deleted_file) {
        auto file_path = temp_dir_ / "already_gone.txt";

        EXPECT_TRUE(file_utils_t::delete_file(file_path));
    }

    TEST_F(file_utils_test, force_delete_file_removes_file) {
        auto file_path = temp_dir_ / "force_delete.txt";
        file_utils_t::write_file(file_path, "content");

        EXPECT_TRUE(file_utils_t::force_delete_file(file_path));
        EXPECT_FALSE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test, force_delete_file_removes_directory_recursively) {
        auto dir_path = temp_dir_ / "dir_to_remove";
        std::filesystem::create_directories(dir_path / "subdir");
        file_utils_t::write_file(dir_path / "file.txt", "content");

        EXPECT_TRUE(file_utils_t::force_delete_file(dir_path));
        EXPECT_FALSE(std::filesystem::exists(dir_path));
    }

    TEST_F(file_utils_test, file_exists_returns_true_for_file) {
        auto file_path = temp_dir_ / "exists.txt";
        file_utils_t::write_file(file_path, "content");

        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test, file_exists_returns_false_for_directory) {
        auto dir_path = temp_dir_ / "a_dir";
        std::filesystem::create_directories(dir_path);

        EXPECT_FALSE(file_utils_t::file_exists(dir_path));
    }

    TEST_F(file_utils_test, file_exists_returns_false_for_missing_path) {
        auto file_path = temp_dir_ / "missing.txt";

        EXPECT_FALSE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test, dir_exists_returns_true_for_directory) {
        auto dir_path = temp_dir_ / "test_dir";
        std::filesystem::create_directories(dir_path);

        EXPECT_TRUE(file_utils_t::dir_exists(dir_path));
    }

    TEST_F(file_utils_test, dir_exists_returns_false_for_file) {
        auto file_path = temp_dir_ / "test_file.txt";
        file_utils_t::write_file(file_path, "content");

        EXPECT_FALSE(file_utils_t::dir_exists(file_path));
    }

    TEST_F(file_utils_test, dir_exists_returns_false_for_missing_path) {
        auto dir_path = temp_dir_ / "missing_dir";

        EXPECT_FALSE(file_utils_t::dir_exists(dir_path));
    }

    TEST_F(file_utils_test, write_file_overwrites_existing_content) {
        auto file_path = temp_dir_ / "overwrite.txt";
        file_utils_t::write_file(file_path, "old content");

        EXPECT_TRUE(file_utils_t::write_file(file_path, "new content"));

        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), "new content");
    }

    TEST_F(file_utils_test, read_file_handles_large_content) {
        auto file_path = temp_dir_ / "large.txt";
        std::string large_content(10000, 'x');

        file_utils_t::write_file(file_path, large_content);
        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().size(), 10000);
        EXPECT_EQ(result.value(), large_content);
    }

}  // namespace error_system::utils
