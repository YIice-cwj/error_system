#pragma once
#include <cstdio>
#include <string>
#include <type_traits>

/**
 * @file error_context_serializer_internal.h
 * @brief 错误上下文序列化器内部共享工具（仅实现内部使用，不对外暴露）
 * @details 提供文本/JSON/二进制三种序列化实现共用的辅助函数。
 *          放置于 src/core/ 目录下，不随公共头文件安装。
 *          各 .cc 文件按需 include 本头文件以获取共用工具。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::core::detail {

    /**
     * @brief 将整数值以十进制形式追加到字符串末尾
     * @tparam T 整数类型
     * @param output 目标字符串
     * @param value 待追加的整数值
     * @note noexcept，分配失败时记录日志并保持字符串原状
     */
    template <typename T>
    void append_decimal(std::string& output, T value) noexcept {
        static_assert(std::is_integral_v<T>, "T must be an integral type");
        try {
            output.append(std::to_string(value));
        } catch (...) {
            std::fprintf(stderr, "[error_context_serializer] append_decimal: std::bad_alloc\n");
        }
    }

}  // namespace error_system::core::detail
