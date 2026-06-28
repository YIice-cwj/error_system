#pragma once
#include <cstdint>

/**
 * @file source_location.h
 * @brief 源文件位置
 * @details 定义源文件位置相关的操作，如获取源文件位置
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-07
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils {
    /**
     * @brief 提取源文件名
     * @param path 源文件路径
     * @return const char* 源文件名
     */
    [[nodiscard]] constexpr const char* extract_short_filename(const char* path) noexcept {
        const char* short_name = path;
        for (const char* ptr = path; *ptr != '\0'; ++ptr) {
            if (*ptr == '/' || *ptr == '\\') {
                short_name = ptr + 1;
            }
        }
        return short_name;
    }

    /**
     * @brief 源文件位置
     * @details 定义源文件位置相关的操作，如获取源文件位置
     */
    class source_location_t {
    private:
        const char* file_name_{"unknown"};

        const char* function_name_{"unknown"};

        uint32_t line_{0};

    public:
        constexpr source_location_t() noexcept = default;

        /**
         * @brief 从已有字符串构造源位置（用于反序列化）
         * @details 调用方必须保证 file 与 func 字符串的生命周期不短于本对象。
         *          主要供 error_context_serializer_t::from_binary / from_json 使用，
         *          由 error_context_t 内部的字符串存储持有实际数据。
         * @param file 源文件路径字符串
         * @param func 函数名字符串
         * @param line 行号
         */
        constexpr source_location_t(const char* file, const char* func, uint32_t line) noexcept
            : file_name_(file), function_name_(func), line_(line) {}

        ~source_location_t() noexcept = default;
        source_location_t(const source_location_t&) = default;
        source_location_t& operator=(const source_location_t&) = default;
        source_location_t(source_location_t&&) noexcept = default;
        source_location_t& operator=(source_location_t&&) noexcept = default;

        /**
         * @brief 获取源文件位置
         * @param file 源文件路径
         * @param func 函数名
         * @param line 行号
         * @return source_location_t 源文件位置
         */
        [[nodiscard]] static constexpr source_location_t current(
#if defined(__GNUC__) || defined(__clang__)
            const char* file = __builtin_FILE(),
            const char* func = __builtin_FUNCTION(),
            uint32_t line = __builtin_LINE()
#elif defined(_MSC_VER) && _MSC_VER >= 1926
            const char* file = __builtin_FILE(),
            const char* func = __builtin_FUNCTION(),
            uint32_t line = __builtin_LINE()
#else
            const char* file = "unknown", const char* func = "unknown", uint32_t line = 0
#endif
                ) noexcept {
            source_location_t source_location;
            source_location.file_name_ = file;
            source_location.function_name_ = func;
            source_location.line_ = line;
            return source_location;
        }

        /**
         * @brief 获取源文件路径
         * @return const char* 源文件路径
         */
        [[nodiscard]] constexpr const char* file_name() const noexcept { return file_name_; }

        /**
         * @brief 获取函数名
         * @return const char* 函数名
         */
        [[nodiscard]] constexpr const char* function_name() const noexcept { return function_name_; }

        /**
         * @brief 获取行号
         * @return uint32_t 行号
         */
        [[nodiscard]] constexpr uint32_t line() const noexcept { return line_; }
    };

}  // namespace error_system::utils