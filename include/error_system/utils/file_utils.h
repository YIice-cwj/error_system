#pragma once
#include <filesystem>
#include <optional>
#include <string>

/**
 * @file file_utils.h
 * @brief 文件工具
 * @details 定义文件相关的相关的函数，用于读取、写入、创建文件等
 * @author yiice
 * @version 2.3.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils {

    class file_utils_t {
    public:
        /**
         * @brief 读取文件的最大字节数阈值
         * @details 超过此大小的文件将被拒绝读取，避免内存耗尽攻击
         */
        static constexpr size_t MAX_READ_FILE_SIZE = 64 * 1024 * 1024;  // 64 MB

        file_utils_t() = delete;
        ~file_utils_t() noexcept = delete;
        file_utils_t(const file_utils_t&) = delete;
        file_utils_t& operator=(const file_utils_t&) = delete;
        file_utils_t(file_utils_t&&) = delete;
        file_utils_t& operator=(file_utils_t&&) = delete;
        /**
         * @brief 读取文件内容
         * @details 从指定文件路径读取文件内容，返回文件内容的字符串表示。
         *          文件大小超过 MAX_READ_FILE_SIZE 时返回 nullopt，避免 OOM
         * @param path 文件路径
         * @return std::optional<std::string> 文件内容的字符串表示，如果文件不存在或过大则返回空可选
         */
        static std::optional<std::string> read_file(const std::filesystem::path& path) noexcept;

        /**
         * @brief 写入文件内容
         * @details 将指定字符串内容写入到指定文件路径
         * @param path 文件路径
         * @param content 要写入的字符串内容
         * @return bool 写入成功则返回 true，否则返回 false
         */
        static bool write_file(const std::filesystem::path& path, const std::string& content) noexcept;

        /**
         * @brief 创建文件
         * @details 创建指定文件路径的文件，如果文件路径不存在则创建
         * @param path 文件路径
         * @return bool 创建成功则返回 true，否则返回 false
         */
        static bool create_file(const std::filesystem::path& path) noexcept;

        /**
         * @brief 删除文件
         * @details 删除指定文件路径的文件
         * @param path 文件路径
         * @return bool 删除成功则返回 true，否则返回 false
         */
        static bool delete_file(const std::filesystem::path& path) noexcept;

        /**
         * @brief 强制删除文件
         * @details 强制删除指定文件路径的文件，如果文件不存在则返回 false
         * @param path 文件路径
         * @return bool 删除成功则返回 true，否则返回 false
         */
        static bool force_delete_file(const std::filesystem::path& path) noexcept;

        /**
         * @brief 检查文件是否存在
         * @details 检查指定文件路径的文件是否存在
         * @param path 文件路径
         * @return bool 文件存在则返回 true，否则返回 false
         */
        static bool file_exists(const std::filesystem::path& path) noexcept;

        /**
         * @brief 检查文件路径是否存在
         * @details 检查指定文件路径是否存在
         * @param path 文件路径
         * @return bool 文件路径存在则返回 true，否则返回 false
         */
        static bool dir_exists(const std::filesystem::path& path) noexcept;
    };

}  // namespace error_system::utils
