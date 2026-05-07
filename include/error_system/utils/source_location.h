#pragma once
#include <cstdint>

/**
 * @file source_location.h
 * @brief 源文件位置
 * @details 定义源文件位置相关的操作，如获取源文件位置
 * @author yiice
 * @version 1.0.0
 * @date 2026-05-07
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils {
    namespace {
        /**
         * @brief 提取源文件名
         * @param path 源文件路径
         * @return const char* 源文件名
         */
        constexpr const char* extract_short_filename(const char* path) noexcept {
            const char* short_name = path;
            for (const char* p = path; *p != '\0'; ++p) {
                if (*p == '/' || *p == '\\') {
                    short_name = p + 1;
                }
            }
            return short_name;
        }
    }  // namespace

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
         * @brief 获取源文件位置
         * @param file 源文件路径
         * @param func 函数名
         * @param line 行号
         * @return source_location_t 源文件位置
         */
        static constexpr source_location_t current(
#if defined(__GNUC__) || defined(__clang__)
            const char* file = __builtin_FILE(),
            const char* func = __builtin_FUNCTION(),
            uint32_t line = __builtin_LINE()
#elif defined(_MSC_VER) && _MSC_VER >= 1926
            const char* file = __builtin_FILE(),
            const char* func = __builtin_FUNCSIG(),
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
        constexpr const char* file_name() const noexcept { return file_name_; }

        /**
         * @brief 获取函数名
         * @return const char* 函数名
         */
        constexpr const char* function_name() const noexcept { return function_name_; }

        /**
         * @brief 获取行号
         * @return uint32_t 行号
         */
        constexpr uint32_t line() const noexcept { return line_; }
    };

};  // namespace error_system::utils