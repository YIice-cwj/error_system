#include "error_system/core/error_context.h"

#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "error_system/config/feature_flags.h"
#include "error_system/core/error_context_serializer.h"
#include "error_system/utils/source_location.h"

/**
 * @file error_context.cc
 * @brief 错误上下文实现
 * @details 定义错误上下文的核心实现，包括状态判断、因果链包装、payload 管理和比较运算。
 *          序列化实现见 error_context_serializer_*.cc，运行时特性初始化见 error_context_initializer.cc。
 *          payload 采用 SSO（Small Size Optimization），≤4 项时栈上存储零堆分配。
 * @author yiice
 * @version 2.5.0
 * @date 2026-06-28
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 拷贝构造函数
     * @details 深拷贝 SSO payload、溢出 map 和因果链。
     *          POD 字段在初始化列表完成，string/容器拷贝放入 try 块，
     *          避免 bad_alloc 突破 noexcept 触发 terminate（规范 14）。
     *          分配失败时对象处于部分构造状态，但调用方仍可安全析构。
     */
    error_context_t::error_context_t(const error_context_t& other) noexcept
        : code_(other.code_),
        payload_count_(other.payload_count_),
        source_location(other.source_location), file_name(other.file_name) {
        try {
            copy_basic_fields_(other);
            copy_sso_payload_(other);
            copy_overflow_payload_(other);
            copy_cause_(other);
            copy_stack_frames_(other);
            repair_source_location_pointers_();
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_context] copy constructor: std::bad_alloc\n");
        }
    }

    /**
     * @brief 移动赋值运算符
     * @details 提供移动赋值以允许复用变量（此前为 = delete 导致必须每次声明新变量）。
     *          自赋值安全：先释放自身溢出存储与 cause，再从源对象移动。
     * @param other 源对象（移动后处于有效但未指定状态）
     * @return error_context_t& 自身引用
     */
    error_context_t& error_context_t::operator=(error_context_t&& other) noexcept {
        if (this != &other) {
            code_ = other.code_;
            metadata_ = std::move(other.metadata_);
            payload_count_ = other.payload_count_;
            payload_small_ = std::move(other.payload_small_);
            payload_overflow_ = std::move(other.payload_overflow_);
            message = std::move(other.message);
            source_location = other.source_location;
            file_name = other.file_name;
            cause = std::move(other.cause);
            if constexpr (error_system::config::feature_flags_t::STACKTRACE_ENABLED) {
                stack_frames = std::move(other.stack_frames);
            }
            loc_file_storage_ = std::move(other.loc_file_storage_);
            loc_func_storage_ = std::move(other.loc_func_storage_);
            repair_source_location_pointers_();
            other.payload_count_ = 0;
            other.file_name = nullptr;
            other.source_location = utils::source_location_t{};
        }
        return *this;
    }

    /**
     * @brief 移动构造函数
     * @details 显式清零源对象 payload_count_，避免移动后源对象状态不一致
     */
    error_context_t::error_context_t(error_context_t&& other) noexcept
        : code_(other.code_),
        metadata_(std::move(other.metadata_)),
        payload_count_(other.payload_count_),
        payload_small_(std::move(other.payload_small_)),
        payload_overflow_(std::move(other.payload_overflow_)),
        loc_file_storage_(std::move(other.loc_file_storage_)),
        loc_func_storage_(std::move(other.loc_func_storage_)),
        message(std::move(other.message)),
        source_location(other.source_location), file_name(other.file_name),
        cause(std::move(other.cause)),
        stack_frames(std::move(other.stack_frames)) {
        other.payload_count_ = 0;
        other.file_name = nullptr;
        repair_source_location_pointers_();
        other.source_location = utils::source_location_t{};
    }

    /**
     * @brief 拷贝基本字段
     */
    void error_context_t::copy_basic_fields_(const error_context_t& other) {
        metadata_ = other.metadata_;
        loc_file_storage_ = other.loc_file_storage_;
        loc_func_storage_ = other.loc_func_storage_;
        message = other.message;
    }

    /**
     * @brief 拷贝 SSO payload
     */
    void error_context_t::copy_sso_payload_(const error_context_t& other) {
        for (size_t i = 0; i < payload_count_ && i < PAYLOAD_SSO_CAPACITY; ++i) {
            payload_small_[i] = other.payload_small_[i];
        }
    }

    /**
     * @brief 拷贝溢出 payload
     */
    void error_context_t::copy_overflow_payload_(const error_context_t& other) {
        if (other.payload_overflow_) {
            payload_overflow_ = std::make_unique<std::unordered_map<std::string, std::string>>(
                *other.payload_overflow_);
        }
    }

    /**
     * @brief 拷贝因果链
     */
    void error_context_t::copy_cause_(const error_context_t& other) {
        if (other.cause) {
            cause = other.cause;
        }
    }

    /**
     * @brief 拷贝堆栈帧
     */
    void error_context_t::copy_stack_frames_(const error_context_t& other) {
        if constexpr (error_system::config::feature_flags_t::STACKTRACE_ENABLED) {
            stack_frames = other.stack_frames;
        }
    }

    /**
     * @brief 修复源位置指针
     */
    void error_context_t::repair_source_location_pointers_() noexcept {
        if (!loc_file_storage_.empty()) {
            file_name = loc_file_storage_.c_str();
            source_location = utils::source_location_t(
                loc_file_storage_.c_str(), loc_func_storage_.c_str(), source_location.line());
        }
    }

    void error_context_t::apply_source_location_(const utils::source_location_t& location) noexcept {
        if constexpr (error_system::config::feature_flags_t::LOCATION_ENABLED) {
            source_location = location;
        }
    }

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
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_context] wrap: std::bad_alloc\n");
        }
        return new_code_context;
    }

    error_context_t error_context_t::wrap(error_context_t&& underlying) const noexcept {
        error_context_t new_code_context = *this;
        try {
            new_code_context.cause = std::make_shared<error_context_t>(std::move(underlying));
        } catch (const std::bad_alloc&) {
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
        } catch (const std::bad_alloc&) {
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
        } catch (const std::bad_alloc&) {
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
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[error_context] get_payload_value: std::bad_alloc\n");
        }
        return std::nullopt;
    }

    /**
     * @brief 严格相等比较（含 cause 链与 stack_frames 深比较）
     * @details operator== 仅比较 code/message/payload；本方法额外比较 cause 链与堆栈，
     *          适用于完整状态比对（如测试断言、缓存键）。
     */
    bool error_context_t::equals_strict(const error_context_t& other) const noexcept {
        if (!(*this == other)) {
            return false;
        }
        if constexpr (error_system::config::feature_flags_t::STACKTRACE_ENABLED) {
            if (stack_frames != other.stack_frames) {
                return false;
            }
        }
        if (cause && other.cause) {
            return cause->equals_strict(*other.cause);
        }
        return !cause && !other.cause;
    }

    /**
     * @brief 渲染为可读文本（便捷方法）
     * @details 委托给 error_context_serializer_t::to_string()，避免调用方再 include serializer 头文件
     */
    std::string error_context_t::to_string() const noexcept {
        return error_context_serializer_t::to_string(*this);
    }

    /**
     * @brief 序列化为 JSON 字符串（便捷方法）
     */
    std::string error_context_t::to_json() const noexcept {
        return error_context_serializer_t::to_json(*this);
    }

    /**
     * @brief 序列化为紧凑二进制字符串（便捷方法）
     */
    std::string error_context_t::to_binary() const noexcept {
        return error_context_serializer_t::to_binary(*this);
    }

    /**
     * @brief 聚合多个错误上下文为一个
     * @details 主错误取第一个，其余以 joined_error_N 为键作为 payload 附加。
     *          std::to_string 与 string 拼接可能抛 bad_alloc，用 try-catch 包裹，
     *          失败时停止追加后续错误（已追加的保留），符合规范 14。
     */
    error_context_t join_errors(std::vector<error_context_t>&& errors) noexcept {
        if (errors.empty()) {
            return error_context_t{};
        }
        if (errors.size() == 1) {
            return std::move(errors[0]);
        }
        error_context_t primary = std::move(errors[0]);
        try {
            for (size_t i = 1; i < errors.size(); ++i) {
                std::string key = "joined_error_" + std::to_string(i);
                primary.with(std::move(key), errors[i].message);
            }
        } catch (const std::bad_alloc&) {
            std::fprintf(stderr, "[join_errors] std::bad_alloc\n");
        }
        return primary;
    }

}  // namespace error_system::core
