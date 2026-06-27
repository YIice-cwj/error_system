#pragma once
#include <array>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "error_system/config/error_config.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_context_initializer.h"
#include "error_system/core/error_level.h"
#include "error_system/core/error_registry.h"
#include "error_system/utils/source_location.h"
#include "error_system/utils/string_format.h"

/**
 * @file error_context.h
 * @brief 错误上下文数据类定义
 * @details 定义错误上下文数据结构，封装错误码、消息、结构化负载、因果链、
 *          堆栈跟踪和源位置信息，提供统一的错误信息建模能力。
 *          序列化职责委托给 error_context_serializer_t，运行时特性初始化职责
 *          委托给 error_context_initializer_t，遵循单一职责原则。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-11
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 携带源位置的错误码包装
     * @details 通过隐式转换从 error_code_t 构造时，自动捕获调用者位置
     *          利用 source_location_t::current() 作为默认参数，在调用点展开 __builtin_FILE()
     * @note 构造函数不加 explicit，以支持 error_code_t → located_code_t 的隐式转换
     */
    struct located_code_t {
        error_code_t code;
        utils::source_location_t location;

        located_code_t(error_code_t c, utils::source_location_t loc = utils::source_location_t::current()) noexcept
            : code(c), location(loc) {}
    };

    /**
     * @brief 错误上下文数据类
     * @details 封装完整的错误上下文信息，提供字段解析和因果链包装功能。
     *          构造时通过 error_context_initializer_t 自动根据全局配置完成：
     *          错误码校验、堆栈捕获、源位置记录和插件通知。
     *          序列化（文本/JSON/二进制）由 error_context_serializer_t 提供。
     *          payload 采用 SSO（Small Size Optimization），≤4 项时栈上存储零堆分配。
     *
     * @note 构造成功码（sign=1）的上下文时，跳过所有运行时特性以提升性能。
     * @note source_location 通过 located_code_t 隐式转换在调用点捕获真实位置，
     *       而非构造函数体内调用 current()（那样会捕获库内部位置）。
     *
     * @example 基础用法
     * error_context_t context(ERR_DB_TIMEOUT, "连接超时: {}ms", 3000);
     * context.with("host", "192.168.1.1")
     *        .with("retry_count", "3");
     * std::cout << error_context_serializer_t::to_string(context) << "\n";
     */
    struct error_context_t {
        /**
         * @brief SSO（Small Size Optimization）负载上限
         * @details payload 项数 ≤ 4 时存于栈上 std::array，零堆分配；
         *          超过后自动溢出到 std::unordered_map
         */
        static constexpr size_t PAYLOAD_SSO_CAPACITY = 4;

    private:
        friend class error_context_serializer_t;
        friend class error_context_initializer_t;

        /**
         * @brief 查找 payload 值指针（避免 optional 拷贝）
         * @param key 键名
         * @return const std::string* 值指针，未找到返回 nullptr
         */
        const std::string* find_payload_(const std::string& key) const noexcept {
            for (size_t i = 0; i < payload_count_ && i < PAYLOAD_SSO_CAPACITY; ++i) {
                if (payload_small_[i].first == key) {
                    return &payload_small_[i].second;
                }
            }
            if (payload_overflow_) {
                auto it = payload_overflow_->find(key);
                if (it != payload_overflow_->end()) {
                    return &it->second;
                }
            }
            return nullptr;
        }

        error_code_t code_{};
        /**
         * @brief 缓存的元数据（值副本）
         * @details 构造时从 error_registry_t 查询并深拷贝，避免 unregister 后悬垂指针
         */
        error_metadata_t metadata_{};
        /**
         * @brief 当前 payload 项数（仅计 SSO 部分）
         */
        uint8_t payload_count_{0};

        /**
         * @brief SSO payload：≤4 项时的栈上存储
         */
        std::array<std::pair<std::string, std::string>, PAYLOAD_SSO_CAPACITY> payload_small_{};
        /**
         * @brief 溢出 payload：超过 SSO 容量后的堆存储，惰性初始化
         */
        std::unique_ptr<std::unordered_map<std::string, std::string>> payload_overflow_;

        /**
         * @brief 插入或更新 payload 字段（核心逻辑）
         * @details 统一处理 SSO 查找→溢出查找→SSO 追加→溢出迁移四个分支
         * @tparam K 键类型（支持 const string&/string&&/string_view）
         * @tparam V 值类型
         * @param key 键
         * @param value 值
         * @return error_context_t& 自身引用
         */
        template <typename K, typename V>
        error_context_t& insert_or_update_payload_(K&& key, V&& value) noexcept {
            try {
                const size_t sso_end = std::min<size_t>(payload_count_, PAYLOAD_SSO_CAPACITY);
                for (size_t i = 0; i < sso_end; ++i) {
                    if (payload_small_[i].first == key) {
                        payload_small_[i].second = std::forward<V>(value);
                        return *this;
                    }
                }

                if (payload_overflow_) {
                    payload_overflow_->insert_or_assign(std::forward<K>(key), std::forward<V>(value));
                    return *this;
                }

                if (payload_count_ < PAYLOAD_SSO_CAPACITY) {
                    payload_small_[payload_count_].first = std::forward<K>(key);
                    payload_small_[payload_count_].second = std::forward<V>(value);
                    ++payload_count_;
                    return *this;
                }

                auto overflow = std::make_unique<std::unordered_map<std::string, std::string>>();
                for (size_t i = 0; i < PAYLOAD_SSO_CAPACITY; ++i) {
                    overflow->emplace(payload_small_[i].first, payload_small_[i].second);
                }
                overflow->insert_or_assign(std::forward<K>(key), std::forward<V>(value));
                payload_overflow_ = std::move(overflow);
                for (size_t i = 0; i < PAYLOAD_SSO_CAPACITY; ++i) {
                    payload_small_[i] = {};
                }
                payload_count_ = 0;
            } catch (...) {
                std::fprintf(stderr, "[error_context] insert_or_update_payload_: std::bad_alloc\n");
            }
            return *this;
        }

    public:
        std::string message{};
        /**
         * @brief 源位置信息
         * @details 构造时通过 located_code_t 隐式转换捕获调用者真实位置
         */
        utils::source_location_t source_location{};
        /**
         * @brief 文件名（可能为短文件名，由 initializer 根据配置设置）
         * @details nullptr 表示未设置源位置
         */
        const char* file_name{nullptr};
        std::shared_ptr<error_context_t> cause{nullptr};
        std::vector<std::string> stack_frames{};

    public:
        constexpr error_context_t() noexcept = default;

        /**
         * @brief 拷贝构造函数
         * @details 深拷贝 SSO payload、溢出 map 和因果链。
         *          POD 字段在初始化列表完成，string/容器拷贝放入 try 块，
         *          避免 bad_alloc 突破 noexcept 触发 terminate（规范 14）。
         *          分配失败时对象处于部分构造状态，但调用方仍可安全析构。
         */
        error_context_t(const error_context_t& other) noexcept
            : code_(other.code_),
            payload_count_(other.payload_count_),
            source_location(other.source_location), file_name(other.file_name) {
            try {
                metadata_ = other.metadata_;
                message = other.message;
                for (size_t i = 0; i < payload_count_ && i < PAYLOAD_SSO_CAPACITY; ++i) {
                    payload_small_[i] = other.payload_small_[i];
                }
                if (other.payload_overflow_) {
                    payload_overflow_ = std::make_unique<std::unordered_map<std::string, std::string>>(
                        *other.payload_overflow_);
                }
                if (other.cause) {
                    cause = other.cause;
                }
                if constexpr (error_system::config::feature_flags_t::STACKTRACE_ENABLED) {
                    stack_frames = other.stack_frames;
                }
            } catch (...) {
                std::fprintf(stderr, "[error_context] copy constructor: std::bad_alloc\n");
            }
        }
        error_context_t& operator=(const error_context_t&) = delete;
        error_context_t& operator=(error_context_t&&) = delete;

        /**
         * @brief 移动构造函数
         * @details 显式清零源对象 payload_count_，避免移动后源对象状态不一致
         */
        error_context_t(error_context_t&& other) noexcept
            : code_(other.code_),
            metadata_(std::move(other.metadata_)),
            payload_count_(other.payload_count_),
            payload_small_(std::move(other.payload_small_)),
            payload_overflow_(std::move(other.payload_overflow_)),
            message(std::move(other.message)),
            source_location(other.source_location), file_name(other.file_name),
            cause(std::move(other.cause)),
            stack_frames(std::move(other.stack_frames)) {
            other.payload_count_ = 0;
            other.file_name = nullptr;
        }
        ~error_context_t() noexcept = default;

        /**
         * @brief 构造函数（接受 located_code_t，自动捕获调用位置和堆栈）
         * @details 在构造时自动完成以下操作：
         *          1. 格式化消息字符串
         *          2. 通过 located_code_t 隐式转换捕获源位置（调用者真实位置）
         *          3. 根据全局配置校验错误码、抓取堆栈、通知插件
         *          若错误码 sign=1（成功），则跳过步骤 3
         * @param lc 携带源位置的错误码（error_code_t 可隐式转换）
         * @param message_format 错误信息格式化字符串（支持 {} 占位符）
         * @param args 格式化参数列表
         *
         * @example
         * // 简单消息（error_code_t 隐式转换为 located_code_t，自动捕获位置）
         * error_context_t context(ERR_CONN_FAILED, "数据库连接失败");
         *
         * // 格式化消息
         * error_context_t context(ERR_TIMEOUT, "请求超时: {}ms, 重试: {}次", 3000, 2);
         */
        template <typename... Args>
        error_context_t(located_code_t lc, std::string message_format, Args&&... args) noexcept
            : code_(lc.code),
            message(utils::string_format_t::format(std::move(message_format), std::forward<Args>(args)...)) {
            try {
                auto info = error_registry_t::instance().get_info(lc.code);
                if (info) {
                    metadata_ = std::move(*info);
                }
            } catch (...) {
                std::fprintf(stderr, "[error_context] constructor: metadata copy failed\n");
            }
            if constexpr (error_system::config::feature_flags_t::LOCATION_ENABLED) {
                source_location = lc.location;
            }
            if (is_success()) {
                return;
            }
            error_context_initializer_t::initialize(*this);
        }

        /**
         * @brief 从标准异常创建错误上下文
         * @details 从 std::exception 或派生类创建错误上下文，提取错误消息
         * @param code 错误码
         * @param ex 标准异常对象
         * @param location 源位置（默认捕获调用者位置）
         * @return error_context_t 错误上下文
         */
        static error_context_t from_exception(error_code_t code, const std::exception& ex,
                                              utils::source_location_t location = utils::source_location_t::current()) noexcept {
            return error_context_t(located_code_t{code, location}, ex.what());
        }

        /**
         * @brief 获取错误码的只读引用
         * @return const error_code_t& 错误码只读引用
         */
        const error_code_t& get_code() const noexcept { return code_; }

        /**
         * @brief 检查错误上下文是否表示成功
         * @return bool 若 sign 位为 1（成功码）则返回 true
         */
        bool is_success() const noexcept;

        /**
         * @brief 检查错误上下文是否包含错误
         * @return bool 若 sign 位为 0（错误码）则返回 true
         */
        bool is_error() const noexcept;

        /**
         * @brief 包装底层错误为当前错误的直接原因
         * @param underlying 底层错误上下文
         * @return error_context_t 包含因果链的新错误上下文
         */
        error_context_t wrap(const error_context_t& underlying) const noexcept;

        /**
         * @brief 包装底层错误为当前错误的直接原因（移动语义版本）
         * @param underlying 底层错误上下文（将被移动）
         * @return error_context_t 包含因果链的新错误上下文
         */
        error_context_t wrap(error_context_t&& underlying) const noexcept;

        /**
         * @brief 添加字符串类型负载字段
         * @param key 字段名
         * @param value 字段值
         * @return error_context_t& 当前对象引用，支持链式调用
         */
        error_context_t& with(const std::string& key, const std::string& value) noexcept;

        /**
         * @brief 添加 C 字符串类型负载字段
         * @param key 字段名
         * @param value 字段值
         * @return error_context_t& 当前对象引用，支持链式调用
         */
        error_context_t& with(const char* key, const char* value) noexcept;

        /**
         * @brief 添加 string_view 类型负载字段
         * @param key 字段名
         * @param value 字段值
         * @return error_context_t& 当前对象引用，支持链式调用
         */
        error_context_t& with(std::string_view key, std::string_view value) noexcept;

        /**
         * @brief 添加移动语义负载字段
         * @param key 字段名
         * @param value 字段值
         * @return error_context_t& 当前对象引用，支持链式调用
         */
        error_context_t& with(std::string&& key, std::string&& value) noexcept;

        /**
         * @brief 批量添加负载字段
         * @details 使用 initializer_list 一次性添加多个 key-value 对，减少链式调用
         * @param items 键值对列表
         * @return error_context_t& 当前对象引用，支持链式调用
         *
         * @example
         * context.with_batch({
         *     {"host", "192.168.1.1"},
         *     {"port", "3306"},
         *     {"db_name", "users"}
         * });
         */
        error_context_t& with_batch(std::initializer_list<std::pair<const std::string, std::string>> items) noexcept;

        /**
         * @brief 相等比较运算符
         * @details 比较错误码、消息和负载是否完全一致
         * @param other 另一个错误上下文
         * @return bool 是否相等
         */
        bool operator==(const error_context_t& other) const noexcept;

        /**
         * @brief 不等比较运算符
         * @param other 另一个错误上下文
         * @return bool 是否不等
         */
        bool operator!=(const error_context_t& other) const noexcept { return !(*this == other); }

        /**
         * @brief 添加多类型负载字段（模板版本）
         * @details 根据 T 的类型自动选择转换方式：
         *          - bool → "true"/"false"
         *          - 算术类型（int, double 等）→ std::to_string
         *          - 其他类型 → static_cast<std::string>
         *          字符串类型（std::string, const char*, std::string_view）优先匹配非模板重载
         * @param key 字段名
         * @param value 字段值
         * @return error_context_t& 当前对象引用，支持链式调用
         *
         * @example
         * context.with("retry_count", 3)
         *        .with("is_retryable", false)
         *        .with("latency_ms", 150.5);
         */
        template <typename T>
        error_context_t& with(const std::string& key, T value) noexcept {
            try {
                if constexpr (std::is_same_v<T, bool>) {
                    return with(key, std::string(value ? "true" : "false"));
                } else if constexpr (std::is_arithmetic_v<T>) {
                    return with(key, std::to_string(value));
                } else {
                    return with(key, std::string(value));
                }
            } catch (...) {
                std::fprintf(stderr, "[error_context] with(const string&, T): std::bad_alloc\n");
                return *this;
            }
        }

        /**
         * @brief 添加自定义负载（const char* key）
         * @details 使用 const char* 作为 key 添加任意类型值到错误上下文
         * @param key 键名
         * @param value 值
         * @return error_context_t& 自身引用
         */
        template <typename T>
        error_context_t& with(const char* key, T value) noexcept {
            if (key == nullptr) {
                std::fprintf(stderr, "[error_context] with(const char*, T): key is nullptr\n");
                return *this;
            }
            try {
                return with(std::string(key), std::move(value));
            } catch (...) {
                std::fprintf(stderr, "[error_context] with(const char*, T): std::bad_alloc\n");
                return *this;
            }
        }

        /**
         * @brief 添加自定义负载（string_view key）
         * @details 使用 std::string_view 作为 key 添加任意类型值到错误上下文
         * @param key 键名
         * @param value 值
         * @return error_context_t& 自身引用
         */
        template <typename T>
        error_context_t& with(std::string_view key, T value) noexcept {
            try {
                return with(std::string(key), std::move(value));
            } catch (...) {
                std::fprintf(stderr, "[error_context] with(string_view, T): std::bad_alloc\n");
                return *this;
            }
        }

        /**
         * @brief 获取 payload 副本（SSO 安全）
         * @return std::vector<std::pair<std::string, std::string>> payload 键值对列表
         */
        std::vector<std::pair<std::string, std::string>> get_payload() const noexcept;

        /**
         * @brief 根据键获取 payload 值
         * @details 直接按 key 查找 payload 值，避免手动遍历 get_payload() 结果
         * @param key 键名
         * @return std::optional<std::string> 值，若键不存在则返回 std::nullopt
         */
        std::optional<std::string> get_payload_value(const std::string& key) const noexcept;

        /**
         * @brief 获取 payload 项数
         * @return size_t payload 项数
         */
        size_t payload_size() const noexcept {
            return payload_count_ + (payload_overflow_ ? payload_overflow_->size() : 0);
        }

        /**
         * @brief 判断 payload 是否为空
         * @return bool payload 为空时返回 true
         */
        bool is_payload_empty() const noexcept { return payload_count_ == 0 && !payload_overflow_; }

        /**
         * @brief 遍历所有 payload 项
         * @details 对 SSO 和溢出存储中的所有项依次调用 visitor
         * @param visitor 接受 (const std::string& key, const std::string& value) 的回调
         */
        template <typename Visitor>
        void for_each_payload(Visitor&& visitor) const noexcept {
            for (size_t i = 0; i < payload_count_ && i < PAYLOAD_SSO_CAPACITY; ++i) {
                visitor(payload_small_[i].first, payload_small_[i].second);
            }
            if (payload_overflow_) {
                for (const auto& [key, value] : *payload_overflow_) {
                    visitor(key, value);
                }
            }
        }

        /**
         * @brief 获取 C 风格错误描述字符串
         * @return const char* 错误消息的 C 字符串，生命周期与 error_context_t 对象绑定
         */
        const char* what() const noexcept { return message.c_str(); }
    };
}  // namespace error_system::core