#pragma once
#include <cstdlib>

#include <string>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include <cxxabi.h>
#include <execinfo.h>
#elif defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <mutex>
#include <sstream>
#endif
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <cxxabi.h>
#endif

/**
 * @file stack_trace_utils.h
 * @brief 堆栈跟踪工具
 * @details 提供当前线程的函数调用栈抓取功能
 * @author yiice
 * @version 2.3.0
 * @date 2026-05-03
 * @copyright Copyright (c) 2026
 */
namespace error_system::utils {

    class stack_trace_utils_t {
    public:
        stack_trace_utils_t() = delete;
        ~stack_trace_utils_t() noexcept = delete;
        stack_trace_utils_t(const stack_trace_utils_t&) = delete;
        stack_trace_utils_t& operator=(const stack_trace_utils_t&) = delete;
        stack_trace_utils_t(stack_trace_utils_t&&) = delete;
        stack_trace_utils_t& operator=(stack_trace_utils_t&&) = delete;

        /**
         * @brief 抓取当前线程的函数调用栈
         * @param skip_frames 需要跳过的顶部栈帧数（比如跳过抓取函数自身）
         * @param max_frames 最大抓取深度
         * @return std::vector<std::string> 每一层调用栈的可读字符串
         */
        static std::vector<std::string> generate(int skip_frames = 1, int max_frames = 16) noexcept;
    };

}  // namespace error_system::utils