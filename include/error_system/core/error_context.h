#pragma once
#include "error_system/config/error_config.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_level.h"
#include "error_system/core/error_registry.h"
#include "error_system/utils/source_location.h"
#include "error_system/utils/stack_trace_utils.h"
#include "error_system/utils/string_utils.h"
#include <array>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using error_system::config::error_config_t;

namespace error_system::plugin {
    class plugin_registry_t;
}  // namespace error_system::plugin

/**
 * @file error_context.h
 * @brief 错误上下文数据类定义
 * @details 定义错误上下文数据结构，封装错误码、消息、结构化负载、因果链、
 *          堆栈跟踪和源位置信息，提供统一的错误信息建模和序列化能力
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-11
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    /**
     * @brief 通知所有已注册插件
     * @details 实现在 plugin_registry_t::notify_error()
     * @param context 错误上下文
     */
    void notify_plugins(const error_context_t& context) noexcept;

    /**
     * @brief 错误上下文数据类
     * @details 封装完整的错误上下文信息，提供字段解析、序列化和因果链包装功能。
     *          构造时自动根据全局配置完成：错误码校验、堆栈捕获、源位置记录和插件通知。
     *          payload 采用 SSO（Small Size Optimization），≤4 项时栈上存储零堆分配。
     *
     * @note 构造成功码（sign=1）的上下文时，跳过所有运行时特性以提升性能。
     *
     * @example 基础用法
     * error_context_t context(ERR_DB_TIMEOUT, "连接超时: {}ms", 3000);
     * context.with("host", "192.168.1.1")
     *        .with("retry_count", "3");
     * std::cout << context.to_string() << "\n";
     */
    struct error_context_t {
        /**
         * @brief SSO（Small Size Optimization）负载上限
         * @details payload 项数 ≤ 4 时存于栈上 std::array，零堆分配；
         *          超过后自动溢出到 std::unordered_map
         */
        static constexpr size_t PAYLOAD_SSO_CAPACITY = 4;

        private:
        error_code_t code_{};
        /**
         * @brief 缓存的元数据指针
         * @details 构造时从 error_registry_t 查询并缓存，避免 to_string/to_json 重复加锁查询
         */
        const error_metadata_t* metadata_{nullptr};

        /**
         * @brief SSO payload：≤4 项时的栈上存储
         */
        std::array<std::pair<std::string, std::string>, PAYLOAD_SSO_CAPACITY> payload_small_{};
        /**
         * @brief 溢出 payload：超过 SSO 容量后的堆存储，惰性初始化
         */
        std::unique_ptr<std::unordered_map<std::string, std::string>> payload_overflow_;
        /**
         * @brief 当前 payload 项数
         */
        uint8_t payload_count_{0};

        public:
        std::string message{};
        std::shared_ptr<error_context_t> cause{nullptr};
        std::vector<std::string> stack_frames{};
        const char* file_name{nullptr};
        const char* function_name{nullptr};
        uint32_t line_number{0};
        /**
         * @brief 源位置信息
         * @details 构造时自动通过 source_location_t::current() 捕获调用位置
         */
        utils::source_location_t source_location_{};

        /**
         * @brief 执行运行时特性初始化
         * @details 根据全局配置依次完成：错误码校验 → 堆栈捕获 → 源位置记录 → 插件通知
         */
        void __finalize_runtime_features() noexcept;

        /**
         * @brief 校验错误码是否已注册
         * @details 若未注册且校验开关开启，则将 code 替换为 fatal 级别的未注册标记
         */
        void __fill_validation_fields() noexcept;
        
        /**
         * @brief 捕获当前调用栈
         * @details 仅在 ERROR_SYSTEM_ENABLE_STACKTRACE 宏开启且等级达到阈值时执行
         */
        void __fill_stacktrace() noexcept;
        
        /**
         * @brief 填充源位置信息
         * @details 从构造时捕获的 source_location_ 成员提取文件名、函数名和行号
         * @param short_filename_enabled 是否使用短文件名模式
         */
        void __fill_source_location(bool short_filename_enabled) noexcept;

        public: 
        constexpr error_context_t() noexcept = default;

        /**
         * @brief 拷贝构造函数
         * @details 深拷贝 SSO payload 和溢出 map
         */
        error_context_t(const error_context_t& other) noexcept
            : code_(other.code_), metadata_(other.metadata_),
            payload_count_(other.payload_count_),
            message(other.message) {
            if constexpr (error_config_t::STACKTRACE_ENABLED) {
                stack_frames = other.stack_frames;
            }
            if constexpr (error_config_t::LOCATION_ENABLED) {
                file_name = other.file_name;
                function_name = other.function_name;
                line_number = other.line_number;
                source_location_ = other.source_location_;
            }
            try {
                for (size_t i = 0; i < payload_count_ && i < PAYLOAD_SSO_CAPACITY; ++i) {
                    payload_small_[i] = other.payload_small_[i];
                }
                if (other.payload_overflow_) {
                    payload_overflow_ = std::make_unique<std::unordered_map<std::string, std::string>>(
                        *other.payload_overflow_);
                }
                if (other.cause) {
                    cause = std::make_shared<error_context_t>(*other.cause);
                }
            } catch (...) {
                std::fprintf(stderr, "[error_context] copy constructor: std::bad_alloc\n");
            }
        }
        error_context_t& operator=(const error_context_t&) = delete;
        error_context_t& operator=(error_context_t&&) = delete;
        error_context_t(error_context_t&&) noexcept = default;

        /**
         * @brief 构造函数（直接接受错误码，自动捕获调用位置和堆栈）
         * @details 在构造时自动完成以下操作：
         *          1. 格式化消息字符串
         *          2. 捕获源位置（文件名、函数名、行号），由 source_location_t::current() 提供
         *          3. 根据全局配置校验错误码、抓取堆栈、通知插件
         *          若错误码 sign=1（成功），则跳过步骤 3
         * @param code 错误码
         * @param message_format 错误信息格式化字符串（支持 {} 占位符）
         * @param args 格式化参数列表
         *
         * @example
         * // 简单消息
         * error_context_t context(ERR_CONN_FAILED, "数据库连接失败");
         *
         * // 格式化消息
         * error_context_t context(ERR_TIMEOUT, "请求超时: {}ms, 重试: {}次", 3000, 2);
         */
        template <typename... Args>
        error_context_t(error_code_t code, std::string message_format, Args&&... args) noexcept
            : code_(code),
            metadata_(error_registry_t::instance().get_info(code)),
            message(utils::string_utils_t::format(message_format, std::forward<Args>(args)...)) {
            if constexpr (error_config_t::LOCATION_ENABLED) {
                source_location_ = utils::source_location_t::current();
            }
            if (is_success()) {
                return;
            }
            __finalize_runtime_features();
        }

        /**
         * @brief 从标准异常创建错误上下文
         * @details 从 std::exception 或派生类创建错误上下文，提取错误消息
         * @param code 错误码
         * @param ex 标准异常对象
         * @return error_context_t 错误上下文
         */
        static error_context_t from_exception(error_code_t code, const std::exception& ex) noexcept {
            return error_context_t(code, ex.what());
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
         * @brief 转换为人类可读字符串
         * @details 从 error_registry_t 获取元数据，根据 enable_text_output 配置决定
         *          输出子系统/模块名称或原始 ID。包含：源位置、错误等级、系统域、
         *          子系统/模块、错误编号、消息、描述、payload、堆栈和因果链
         * @return std::string 格式化的错误上下文字符串
         */
        std::string to_string() const noexcept;

        /**
         * @brief 转换为 JSON 字符串
         * @details 生成包含 code、message、payload、stack_frames、cause 等字段的 JSON
         * @return std::string 错误上下文的 JSON 表示
         */
        std::string to_json() const noexcept;

        /**
         * @brief 转换为紧凑二进制字符串
         * @details 使用小端序编码，适合高性能 RPC 或持久化存储
         * @return std::string 错误上下文的二进制表示
         */
        std::string to_binary() const noexcept;

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