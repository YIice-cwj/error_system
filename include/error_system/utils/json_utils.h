#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

/**
 * @file json_utils.h
 * @brief JSON工具函数
 * @details 定义JSON相关的工具函数，用于解析、序列化、验证等操作
 * @author yiice
 * @version 1.0.0
 * @date 2026-04-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils {
    /**
     * @brief JSON字典类
     * @details 定义JSON字典相关的操作，如解析、序列化、验证等
     */
    class json_dict_t {
        private:
        std::unordered_map<std::string, std::string> dict_{};

        private:
        public:
        json_dict_t() noexcept = default;

        ~json_dict_t() noexcept = default;

        json_dict_t(const json_dict_t&) = default;

        json_dict_t& operator=(const json_dict_t&) = default;

        json_dict_t(json_dict_t&&) noexcept = default;

        json_dict_t& operator=(json_dict_t&&) noexcept = default;

        public:
        /**
         * @brief 获取JSON字典中的字符串值
         * @details 根据键获取JSON字典中的字符串值
         * @param key JSON键，格式为 "key1.key2"
         * @return std::optional<std::string> 字符串值，若键不存在则返回空可选
         */
        std::optional<std::string> operator[](const std::string& key) const noexcept;

        /**
         * @brief 获取JSON字典中的字符串值
         * @details 根据键获取JSON字典中的字符串值
         * @param key JSON键，格式为 "key1.key2"
         * @return std::optional<std::string> 字符串值，若键不存在则返回空可选
         */
        std::optional<std::string> get_value(const std::string& key) const noexcept;

        /**
         * @brief 获取JSON字典中的字符串值
         * @details 根据键获取JSON字典中的字符串值
         * @param key JSON键，格式为 "key1.key2"
         * @return std::optional<std::string> 字符串值，若键不存在则返回空可选
         */
        std::optional<std::string> get_value_or(const std::string& key,
                                                const std::string& default_value) const noexcept;

        /**
         * @brief 检查JSON字典是否包含指定键
         * @details 检查JSON字典是否包含指定键，若包含则返回true，否则返回false
         * @param key JSON键，格式为 "key1.key2"
         * @return bool 是否包含指定键
         */
        bool contains(const std::string& key) const noexcept;

        /**
         * @brief 检查JSON字典是否为空
         * @details 检查JSON字典是否为空，若为空则返回true，否则返回false
         * @return bool 是否为空
         */
        bool empty() const noexcept;

        /**
         * @brief 获取JSON字典的大小
         * @details 获取JSON字典的大小，即键值对的数量
         * @return size_t JSON字典的大小
         */
        size_t size() const noexcept;

        public:
        /**
         * @brief 从文件加载JSON字典
         * @details 从指定文件路径加载JSON字典，若文件不存在或解析失败则返回空可选
         * @param json_path JSON文件路径
         * @return std::optional<json_dict_t> JSON字典
         */
        static std::optional<json_dict_t> from_file(const std::filesystem::path& json_path) noexcept;

        /**
         * @brief 解析JSON字符串
         * @details 解析JSON字符串为JSON字典，若解析失败则返回空可选
         * @param json_content JSON字符串内容
         * @return std::optional<json_dict_t> 解析后的JSON字典，若解析失败则返回空可选
         */
        static std::optional<json_dict_t> parse(const std::string& json_content) noexcept;
    };

}  // namespace error_system::utils
