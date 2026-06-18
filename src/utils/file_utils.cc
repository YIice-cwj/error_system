#include "error_system/utils/file_utils.h"
#include <fstream>

namespace error_system::utils {

    /**
     * @brief 读取文件内容
     * @details 从指定文件路径读取文件内容，返回文件内容的字符串表示
     * @param path 文件路径
     * @return std::optional<std::string> 文件内容的字符串表示，如果文件不存在则返回空可选
     */
    std::optional<std::string> file_utils_t::read_file(const std::filesystem::path& path) noexcept {
        std::error_code ec{};
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
            return std::nullopt;
        }

        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return std::nullopt;
        }

        try {
            std::string content{};
            file.seekg(0, std::ios::end);
            auto size = file.tellg();
            if (size < 0) {
                return std::nullopt;
            }

            content.resize(static_cast<size_t>(size));
            file.seekg(0, std::ios::beg);
            file.read(content.data(), static_cast<std::streamsize>(content.size()));

            return content;
        } catch (const std::bad_alloc&) {
            return std::nullopt;
        } catch (const std::length_error&) {
            return std::nullopt;
        }
    }

    /**
     * @brief 写入文件内容
     * @details 将指定字符串内容写入到指定文件路径
     * @param path 文件路径
     * @param content 要写入的字符串内容
     * @return bool 写入成功则返回 true，否则返回 false
     */
    bool file_utils_t::write_file(const std::filesystem::path& path, const std::string& content) noexcept {
        std::error_code error{};
        if (path.has_parent_path() && !std::filesystem::exists(path.parent_path(), error)) {
            std::filesystem::create_directories(path.parent_path(), error);
            if (error) {
                return false;
            }
        }

        std::ofstream file(path, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        return file.good();
    }

    /**
     * @brief 创建文件
     * @details 创建指定文件路径的文件，如果文件路径不存在则创建
     * @param path 文件路径
     * @return bool 创建成功则返回 true，否则返回 false
     */
    bool file_utils_t::create_file(const std::filesystem::path& path) noexcept {
        if (file_exists(path)) {
            return true;
        }

        std::error_code error{};
        if (path.has_parent_path() && !std::filesystem::exists(path.parent_path(), error)) {
            std::filesystem::create_directories(path.parent_path(), error);
            if (error) {
                return false;
            }
        }

        std::ofstream file(path);
        return file.is_open();
    }

    /**
     * @brief 删除文件
     * @details 删除指定文件路径的文件
     * @param path 文件路径
     * @return bool 删除成功则返回 true，否则返回 false
     */
    bool file_utils_t::delete_file(const std::filesystem::path& path) noexcept {
        std::error_code error{};
        return std::filesystem::remove(path, error) || !std::filesystem::exists(path, error);
    }

    /**
     * @brief 强制删除文件
     * @details 强制删除指定文件路径的文件，如果文件不存在则返回 false
     * @param path 文件路径
     * @return bool 删除成功则返回 true，否则返回 false
     */
    bool file_utils_t::force_delete_file(const std::filesystem::path& path) noexcept {
        std::error_code error{};
        return std::filesystem::remove_all(path, error) || !std::filesystem::exists(path, error);
    }

    /**
     * @brief 检查文件是否存在
     * @details 检查指定文件路径的文件是否存在
     * @param path 文件路径
     * @return bool 文件存在则返回 true，否则返回 false
     */
    bool file_utils_t::file_exists(const std::filesystem::path& path) noexcept {
        std::error_code error{};
        return std::filesystem::exists(path, error) && std::filesystem::is_regular_file(path, error);
    }

    /**
     * @brief 检查文件路径是否存在
     * @details 检查指定文件路径是否存在
     * @param path 文件路径
     * @return bool 文件路径存在则返回 true，否则返回 false
     */
    bool file_utils_t::dir_exists(const std::filesystem::path& path) noexcept {
        std::error_code error{};
        return std::filesystem::exists(path, error) && std::filesystem::is_directory(path, error);
    }

}  // namespace error_system::utils