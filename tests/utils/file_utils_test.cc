#include "error_system/utils/file_utils.h"

#include <sys/stat.h>

#include <filesystem>
#include <fstream>
#include <random>

#include <gtest/gtest.h>

namespace error_system::utils {

    class file_utils_test_t : public ::testing::Test {
        protected:
        std::filesystem::path temp_dir_;

        void SetUp() override {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(100000, 999999);
            temp_dir_ = std::filesystem::temp_directory_path()
                        / ("error_system_file_test_" + std::to_string(dis(gen)));
            std::filesystem::create_directories(temp_dir_);
            chmod(temp_dir_.c_str(), 0700);
        }

        void TearDown() override { std::filesystem::remove_all(temp_dir_); }
    };

    TEST_F(file_utils_test_t, write_file_creates_file_with_content) {
        auto file_path = temp_dir_ / "write_test.txt";
        std::string content = "hello world";

        EXPECT_TRUE(file_utils_t::write_file(file_path, content));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));

        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), content);
    }

    TEST_F(file_utils_test_t, write_file_creates_parent_directories) {
        auto file_path = temp_dir_ / "sub" / "dir" / "nested.txt";
        std::string content = "nested content";

        EXPECT_TRUE(file_utils_t::write_file(file_path, content));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test_t, read_file_returns_nullopt_for_missing_file) {
        auto file_path = temp_dir_ / "nonexistent.txt";

        auto result = file_utils_t::read_file(file_path);
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(file_utils_test_t, read_file_returns_nullopt_for_directory) {
        auto dir_path = temp_dir_ / "a_directory";
        std::filesystem::create_directories(dir_path);

        auto result = file_utils_t::read_file(dir_path);
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(file_utils_test_t, read_file_handles_empty_file) {
        auto file_path = temp_dir_ / "empty.txt";
        ASSERT_TRUE(file_utils_t::write_file(file_path, ""));

        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result.value().empty());
    }

    TEST_F(file_utils_test_t, read_file_preserves_binary_content) {
        auto file_path = temp_dir_ / "binary.bin";
        std::string content = std::string{"\x00\x01\x02\x03\xff\xfe", 6};

        ASSERT_TRUE(file_utils_t::write_file(file_path, content));
        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), content);
    }

    TEST_F(file_utils_test_t, create_file_creates_new_file) {
        auto file_path = temp_dir_ / "created.txt";

        EXPECT_TRUE(file_utils_t::create_file(file_path));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test_t, create_file_returns_true_for_existing_file) {
        auto file_path = temp_dir_ / "existing.txt";
        ASSERT_TRUE(file_utils_t::write_file(file_path, "content"));

        EXPECT_TRUE(file_utils_t::create_file(file_path));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test_t, create_file_creates_parent_directories) {
        auto file_path = temp_dir_ / "deep" / "path" / "file.txt";

        EXPECT_TRUE(file_utils_t::create_file(file_path));
        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test_t, delete_file_removes_existing_file) {
        auto file_path = temp_dir_ / "to_delete.txt";
        ASSERT_TRUE(file_utils_t::write_file(file_path, "delete me"));

        EXPECT_TRUE(file_utils_t::delete_file(file_path));
        EXPECT_FALSE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test_t, delete_file_returns_true_for_already_deleted_file) {
        auto file_path = temp_dir_ / "already_gone.txt";

        EXPECT_TRUE(file_utils_t::delete_file(file_path));
    }

    TEST_F(file_utils_test_t, force_delete_file_removes_file) {
        auto file_path = temp_dir_ / "force_delete.txt";
        ASSERT_TRUE(file_utils_t::write_file(file_path, "content"));

        EXPECT_TRUE(file_utils_t::force_delete_file(file_path));
        EXPECT_FALSE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test_t, force_delete_file_removes_directory_recursively) {
        auto dir_path = temp_dir_ / "dir_to_remove";
        std::filesystem::create_directories(dir_path / "subdir");
        ASSERT_TRUE(file_utils_t::write_file(dir_path / "file.txt", "content"));

        EXPECT_TRUE(file_utils_t::force_delete_file(dir_path));
        EXPECT_FALSE(std::filesystem::exists(dir_path));
    }

    TEST_F(file_utils_test_t, file_exists_returns_true_for_file) {
        auto file_path = temp_dir_ / "exists.txt";
        ASSERT_TRUE(file_utils_t::write_file(file_path, "content"));

        EXPECT_TRUE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test_t, file_exists_returns_false_for_directory) {
        auto dir_path = temp_dir_ / "a_dir";
        std::filesystem::create_directories(dir_path);

        EXPECT_FALSE(file_utils_t::file_exists(dir_path));
    }

    TEST_F(file_utils_test_t, file_exists_returns_false_for_missing_path) {
        auto file_path = temp_dir_ / "missing.txt";

        EXPECT_FALSE(file_utils_t::file_exists(file_path));
    }

    TEST_F(file_utils_test_t, dir_exists_returns_true_for_directory) {
        auto dir_path = temp_dir_ / "test_dir";
        std::filesystem::create_directories(dir_path);

        EXPECT_TRUE(file_utils_t::dir_exists(dir_path));
    }

    TEST_F(file_utils_test_t, dir_exists_returns_false_for_file) {
        auto file_path = temp_dir_ / "test_file.txt";
        ASSERT_TRUE(file_utils_t::write_file(file_path, "content"));

        EXPECT_FALSE(file_utils_t::dir_exists(file_path));
    }

    TEST_F(file_utils_test_t, dir_exists_returns_false_for_missing_path) {
        auto dir_path = temp_dir_ / "missing_dir";

        EXPECT_FALSE(file_utils_t::dir_exists(dir_path));
    }

    TEST_F(file_utils_test_t, write_file_overwrites_existing_content) {
        auto file_path = temp_dir_ / "overwrite.txt";
        ASSERT_TRUE(file_utils_t::write_file(file_path, "old content"));

        EXPECT_TRUE(file_utils_t::write_file(file_path, "new content"));

        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), "new content");
    }

    TEST_F(file_utils_test_t, read_file_handles_large_content) {
        constexpr size_t LARGE_CONTENT_SIZE = 10000;
        auto file_path = temp_dir_ / "large.txt";
        std::string large_content(LARGE_CONTENT_SIZE, 'x');

        ASSERT_TRUE(file_utils_t::write_file(file_path, large_content));
        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().size(), LARGE_CONTENT_SIZE);
        EXPECT_EQ(result.value(), large_content);
    }

    //
    // 验证 read_file 在文件大小超过 MAX_READ_FILE_SIZE 时拒绝读取，
    // 避免恶意大文件导致内存耗尽攻击。

    TEST_F(file_utils_test_t, read_file_rejects_file_exceeding_max_size) {
        auto file_path = temp_dir_ / "oversize.txt";
        // 写入一个超过阈值 1 字节的文件。使用 truncate(2) 创建稀疏文件，
        // 避免真正分配 MAX_READ_FILE_SIZE+1 字节的磁盘空间。
        const auto target_size = file_utils_t::MAX_READ_FILE_SIZE + 1;
        {
            std::ofstream f(file_path, std::ios::binary | std::ios::trunc);
            ASSERT_TRUE(f.is_open());
            // seek 到目标位置后写一个字节，文件实际大小为 target_size
            f.seekp(static_cast<std::streamoff>(target_size - 1));
            f.put('\n');
            ASSERT_TRUE(f.good());
        }

        auto result = file_utils_t::read_file(file_path);
        EXPECT_FALSE(result.has_value());
    }

    TEST_F(file_utils_test_t, read_file_accepts_file_at_max_size_boundary) {
        // 边界测试：文件大小恰好等于 MAX_READ_FILE_SIZE 时应允许读取。
        // 注意：此用例仅当磁盘空间充足时才会真正分配内存。
        // 为避免测试环境内存压力过大，仅在 MAX_READ_FILE_SIZE 较小（<= 64MB）时执行。
        if (file_utils_t::MAX_READ_FILE_SIZE > 64u * 1024u * 1024u) {
            GTEST_SKIP() << "MAX_READ_FILE_SIZE 过大，跳过边界测试以避免内存压力";
        }
        auto file_path = temp_dir_ / "boundary.txt";
        {
            std::ofstream f(file_path, std::ios::binary | std::ios::trunc);
            ASSERT_TRUE(f.is_open());
            // 分块写入以避免一次性大 string 分配失败
            constexpr size_t CHUNK = 4 * 1024 * 1024;
            std::string chunk(CHUNK, 'x');
            size_t written = 0;
            while (written + CHUNK <= file_utils_t::MAX_READ_FILE_SIZE) {
                f.write(chunk.data(), static_cast<std::streamsize>(CHUNK));
                written += CHUNK;
            }
            if (written < file_utils_t::MAX_READ_FILE_SIZE) {
                const size_t remain = file_utils_t::MAX_READ_FILE_SIZE - written;
                f.write(chunk.data(), static_cast<std::streamsize>(remain));
            }
            ASSERT_TRUE(f.good());
        }
        auto result = file_utils_t::read_file(file_path);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), file_utils_t::MAX_READ_FILE_SIZE);
    }

}  // namespace error_system::utils
