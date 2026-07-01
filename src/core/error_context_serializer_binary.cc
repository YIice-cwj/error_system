#include "error_system/core/error_context_serializer.h"
#include "error_context_serializer_internal.h"

#include <optional>

#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"

using error_system::config::feature_flags_t;

/**
 * @file error_context_serializer_binary.cc
 * @brief 错误上下文序列化器 - 二进制格式实现
 * @details 实现 error_context_serializer_t 的二进制序列化（to_binary / to_binary_impl_）
 *          与反序列化（from_binary / from_binary_node_）。
 *          从 error_context_serializer.cc 拆分而来，仅包含二进制格式相关的辅助函数与逻辑。
 *          使用小端序编码，顶层包含魔数与版本号；cause 链通过递归追加。
 * @author yiice
 * @version 1.0.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {
    namespace {

        /**
         * @brief 以小端序写入整数到缓冲区
         * @tparam T 整数类型
         */
        template <typename T>
        void write_little_endian(std::string& buffer, T value) noexcept {
            static_assert(std::is_integral_v<T>, "T must be an integral type");
            for (size_t i = 0; i < sizeof(T); ++i) {
                buffer.push_back(static_cast<char>((value >> (i * 8)) & 0xFF));
            }
        }

        /**
         * @brief 写入长度前缀字符串（4 字节小端长度 + 字符串字节）
         */
        void write_string_len_prefixed(std::string& buffer, const std::string& text) noexcept {
            const size_t string_size = text.size();
            if (string_size > 0xFFFFFFFFULL) {
                std::fprintf(stderr, "[error_context_serializer] write_string_len_prefixed: string too long, truncated\n");
            }
            const uint32_t length = static_cast<uint32_t>(string_size > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : string_size);
            write_little_endian(buffer, length);
            try {
                buffer.append(text.data(), length);
            } catch (const std::bad_alloc&) {
                std::fprintf(stderr, "[error_context_serializer] to_binary: write_string append failed\n");
            }
        }

        /**
         * @brief 写入源位置（has_location 标记 + file/func/line）
         */
        void write_location_binary(std::string& buffer, const error_context_t& context) noexcept {
            uint8_t has_location = 0;
            if constexpr (feature_flags_t::LOCATION_ENABLED) {
                if (feature_flags_t::is_source_location_enabled() && context.file_name != nullptr) {
                    has_location = 1;
                }
            }
            buffer.push_back(static_cast<char>(has_location));
            if (has_location) {
                write_string_len_prefixed(buffer, context.file_name);
                write_string_len_prefixed(buffer, context.source_location.function_name());
                write_little_endian(buffer, context.source_location.line());
            }
        }

        /**
         * @brief 写入 payload（4 字节计数 + 各 key/value 长度前缀字符串）
         */
        void write_payload_binary(std::string& buffer, const error_context_t& context) noexcept {
            const size_t total = context.payload_size();
            if (total > 0xFFFFFFFFULL) {
                std::fprintf(stderr, "[error_context_serializer] write_payload_binary: payload count overflow, truncated\n");
            }
            write_little_endian(buffer, static_cast<uint32_t>(total > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : total));
            context.for_each_payload([&](const std::string& key, const std::string& value) {
                write_string_len_prefixed(buffer, key);
                write_string_len_prefixed(buffer, value);
            });
        }

        /**
         * @brief 从数据视图读取小端序整数
         * @details 读取失败（数据不足）时返回 false 且不修改 offset
         */
        template <typename T>
        bool read_little_endian(std::string_view data, size_t& offset, T& output) noexcept {
            static_assert(std::is_integral_v<T>, "T must be an integral type");
            if (offset + sizeof(T) > data.size()) {
                return false;
            }
            T value = 0;
            for (size_t i = 0; i < sizeof(T); ++i) {
                value |= static_cast<T>(static_cast<uint8_t>(data[offset + i])) << (i * 8);
            }
            offset += sizeof(T);
            output = value;
            return true;
        }

        /**
         * @brief 读取单字节标记
         */
        bool read_byte(std::string_view data, size_t& offset, uint8_t& output) noexcept {
            if (offset >= data.size()) {
                return false;
            }
            output = static_cast<uint8_t>(data[offset]);
            ++offset;
            return true;
        }

        /**
         * @brief 读取长度前缀字符串（4 字节小端长度 + 字符串字节）
         */
        bool read_string_len_prefixed(std::string_view data, size_t& offset, std::string& output) noexcept {
            uint32_t length = 0;
            if (!read_little_endian(data, offset, length)) {
                return false;
            }
            if (offset + length > data.size()) {
                return false;
            }
            try {
                output.assign(data.data() + offset, length);
            } catch (const std::bad_alloc&) {
                std::fprintf(stderr, "[error_context_serializer] read_string_len_prefixed: std::bad_alloc\n");
                return false;
            }
            offset += length;
            return true;
        }

    }  // namespace

    std::string error_context_serializer_t::to_binary(const error_context_t& context) noexcept {
        return to_binary_impl_(context, 0);
    }

    std::string error_context_serializer_t::to_binary_impl_(const error_context_t& context, size_t depth) noexcept {
        std::string buf;
        const size_t total_payload = context.payload_size();
        try {
            buf.reserve(128 + context.message.size() + total_payload * 24);
            if (depth == 0) {
                write_little_endian(buf, BINARY_MAGIC);
                buf.push_back(static_cast<char>(BINARY_VERSION));
            }
            write_little_endian(buf, context.code_.get_code());
            write_string_len_prefixed(buf, context.message);
            write_location_binary(buf, context);
            write_payload_binary(buf, context);

            if (context.cause && depth < MAX_CAUSE_DEPTH) {
                buf.push_back(1);
                std::string cause_binary = to_binary_impl_(*context.cause, depth + 1);
                write_string_len_prefixed(buf, cause_binary);
            } else {
                buf.push_back(0);
            }
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_context_serializer] to_binary: std::bad_alloc\n");
        }
        return buf;
    }

    /**
     * @brief 从二进制数据反序列化错误上下文（顶层入口）
     * @details 校验魔数与版本号，调用 from_binary_node_ 解析节点，
     *          并拒绝顶层尾随数据。任何校验失败返回 std::nullopt。
     */
    std::optional<error_context_t> error_context_serializer_t::from_binary(std::string_view data) noexcept {
        size_t offset = 0;
        uint32_t magic = 0;
        if (!read_little_endian(data, offset, magic)) {
            return std::nullopt;
        }
        if (magic != BINARY_MAGIC) {
            return std::nullopt;
        }
        uint8_t version = 0;
        if (!read_byte(data, offset, version)) {
            return std::nullopt;
        }
        if (version != BINARY_VERSION) {
            return std::nullopt;
        }
        auto result = from_binary_node_(data, offset);
        if (!result) {
            return std::nullopt;
        }
        if (offset != data.size()) {
            return std::nullopt;
        }
        return result;
    }

    bool error_context_serializer_t::parse_binary_location_field_(
        error_context_t& context, std::string_view data, size_t& offset) noexcept {
        uint8_t has_location = 0;
        if (!read_byte(data, offset, has_location)) {
            return false;
        }
        if (has_location == 0) {
            return true;
        }
        std::string file;
        std::string func;
        uint32_t line = 0;
        if (!read_string_len_prefixed(data, offset, file) ||
            !read_string_len_prefixed(data, offset, func) ||
            !read_little_endian(data, offset, line)) {
            return false;
        }
        context.loc_file_storage_ = std::move(file);
        context.loc_func_storage_ = std::move(func);
        context.file_name = context.loc_file_storage_.c_str();
        context.source_location = utils::source_location_t(
            context.loc_file_storage_.c_str(), context.loc_func_storage_.c_str(), line);
        return true;
    }

    bool error_context_serializer_t::parse_binary_payload_field_(
        error_context_t& context, std::string_view data, size_t& offset) noexcept {
        uint32_t payload_count = 0;
        if (!read_little_endian(data, offset, payload_count)) {
            return false;
        }
        if (payload_count > MAX_PAYLOAD_ITEMS) {
            return false;
        }
        for (uint32_t i = 0; i < payload_count; ++i) {
            std::string key;
            std::string value;
            if (!read_string_len_prefixed(data, offset, key) ||
                !read_string_len_prefixed(data, offset, value)) {
                return false;
            }
            context.insert_or_update_payload_(std::move(key), std::move(value));
        }
        return true;
    }

    bool error_context_serializer_t::parse_binary_cause_field_(
        error_context_t& context, std::string_view data, size_t& offset) noexcept {
        uint8_t has_cause = 0;
        if (!read_byte(data, offset, has_cause)) {
            return false;
        }
        if (has_cause == 0) {
            return true;
        }
        std::string cause_blob;
        if (!read_string_len_prefixed(data, offset, cause_blob)) {
            return false;
        }
        size_t cause_offset = 0;
        auto cause_ctx = from_binary_node_(cause_blob, cause_offset);
        if (!cause_ctx) {
            return false;
        }
        if (cause_offset != cause_blob.size()) {
            return false;
        }
        try {
            context.cause = std::make_shared<error_context_t>(std::move(*cause_ctx));
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_context_serializer] parse_binary_cause_field_: cause make_shared failed\n");
            return false;
        }
        return true;
    }

    std::optional<error_context_t> error_context_serializer_t::from_binary_node_(
        std::string_view data, size_t& offset) noexcept {
        error_context_t context;

        uint64_t raw_code = 0;
        if (!read_little_endian(data, offset, raw_code)) {
            return std::nullopt;
        }
        context.code_ = error_code_t{raw_code};

        if (!read_string_len_prefixed(data, offset, context.message)) {
            return std::nullopt;
        }

        if (auto info = error_registry_t::instance().get_info(context.code_)) {
            context.metadata_ = *info;
        }

        if (!parse_binary_location_field_(context, data, offset)) {
            return std::nullopt;
        }

        if (!parse_binary_payload_field_(context, data, offset)) {
            return std::nullopt;
        }

        if (!parse_binary_cause_field_(context, data, offset)) {
            return std::nullopt;
        }

        return context;
    }

}  // namespace error_system::core
