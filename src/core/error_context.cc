#include "error_system/core/error_context.h"

/**
 * @file error_context.cc
 * @brief 错误上下文实现
 * @details 定义错误上下文的核心实现，包括状态判断、因果链包装、payload 管理和比较运算。
 *          序列化实现见 error_context_serializer.cc，运行时特性初始化见 error_context_initializer.cc。
 *          payload 采用 SSO（Small Size Optimization），≤4 项时栈上存储零堆分配。
 * @author yiice
 * @version 2.4.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    bool error_context_t::is_error() const noexcept {
        return code_.is_error_code();
    }

    bool error_context_t::is_success() const noexcept {
        return code_.is_success_code();
    }

    error_context_t error_context_t::wrap(const error_context_t& underlying) const noexcept {
        error_context_t new_code_context = *this;
        try {
            new_code_context.cause = std::make_shared<error_context_t>(underlying);
        } catch (...) {
            std::fprintf(stderr, "[error_context] wrap: std::bad_alloc\n");
        }
        return new_code_context;
    }

    error_context_t error_context_t::wrap(error_context_t&& underlying) const noexcept {
        error_context_t new_code_context = *this;
        try {
            new_code_context.cause = std::make_shared<error_context_t>(std::move(underlying));
        } catch (...) {
            std::fprintf(stderr, "[error_context] wrap(&&): std::bad_alloc\n");
        }
        return new_code_context;
    }

    error_context_t& error_context_t::with(const std::string& key, const std::string& value) noexcept {
        return insert_or_update_payload_(key, value);
    }

    error_context_t& error_context_t::with(const char* key, const char* value) noexcept {
        return with(std::string_view(key != nullptr ? key : ""), std::string_view(value != nullptr ? value : ""));
    }

    error_context_t& error_context_t::with(std::string_view key, std::string_view value) noexcept {
        try {
            return insert_or_update_payload_(std::string(key), std::string(value));
        } catch (...) {
            std::fprintf(stderr, "[error_context] with(string_view): std::bad_alloc\n");
            return *this;
        }
    }

    error_context_t& error_context_t::with(std::string&& key, std::string&& value) noexcept {
        return insert_or_update_payload_(std::move(key), std::move(value));
    }

    error_context_t& error_context_t::with_batch(
        std::initializer_list<std::pair<const std::string, std::string>> items) noexcept {
        for (const auto& [key, value] : items) {
            with(key, value);
        }
        return *this;
    }

    bool error_context_t::operator==(const error_context_t& other) const noexcept {
        if (code_.get_code() != other.code_.get_code() || message != other.message) {
            return false;
        }
        const size_t this_size = payload_size();
        if (this_size != other.payload_size()) {
            return false;
        }
        bool equal = true;
        for_each_payload([&](const std::string& key, const std::string& value) {
            if (!equal) {
                return;
            }
            const std::string* other_value = other.find_payload_(key);
            if (other_value == nullptr || *other_value != value) {
                equal = false;
            }
        });
        return equal;
    }

    std::vector<std::pair<std::string, std::string>> error_context_t::get_payload() const noexcept {
        std::vector<std::pair<std::string, std::string>> result;
        try {
            result.reserve(payload_size());
            for_each_payload([&](const std::string& key, const std::string& value) {
                result.emplace_back(key, value);
            });
        } catch (...) {
            std::fprintf(stderr, "[error_context] get_payload: std::bad_alloc\n");
        }
        return result;
    }

    std::optional<std::string> error_context_t::get_payload_value(const std::string& key) const noexcept {
        try {
            for (size_t i = 0; i < payload_count_ && i < PAYLOAD_SSO_CAPACITY; ++i) {
                if (payload_small_[i].first == key) {
                    return payload_small_[i].second;
                }
            }
            if (payload_overflow_) {
                auto it = payload_overflow_->find(key);
                if (it != payload_overflow_->end()) {
                    return it->second;
                }
            }
        } catch (...) {
            std::fprintf(stderr, "[error_context] get_payload_value: std::bad_alloc\n");
        }
        return std::nullopt;
    }

}  // namespace error_system::core
