#pragma once

/**
 * @file error_context_initializer.h
 * @brief 错误上下文初始化器
 * @details 从 error_context_t 拆分而来，单一职责：在 error_context_t 构造时
 *          根据全局配置完成错误码校验、堆栈捕获、源位置记录和插件通知。
 *          本头文件仅前向声明 error_context_t（不包含 error_context.h），
 *          error_context.h 在类定义前包含本文件以避免循环依赖。
 * @author yiice
 * @version 2.3.0
 * @date 2026-06-27
 * @copyright Copyright (c) 2026
 */
namespace error_system::core {

    struct error_context_t;

    /**
     * @brief 错误上下文初始化器
     * @details 封装 error_context_t 构造后的运行时特性初始化逻辑。
     *          类仅包含静态成员，禁止实例化。通过 error_context_t 的 friend 声明
     *          访问其私有成员 code_ 与 metadata_。
     */
    class error_context_initializer_t {
    private:
        static void fill_validation_fields_(error_context_t& ctx) noexcept;
        static void fill_stacktrace_(error_context_t& ctx) noexcept;
        static void fill_source_location_(error_context_t& ctx, bool short_filename_enabled) noexcept;

    public:
        error_context_initializer_t() = delete;
        ~error_context_initializer_t() = delete;
        error_context_initializer_t(const error_context_initializer_t&) = delete;
        error_context_initializer_t& operator=(const error_context_initializer_t&) = delete;
        error_context_initializer_t(error_context_initializer_t&&) = delete;
        error_context_initializer_t& operator=(error_context_initializer_t&&) = delete;

        /**
         * @brief 执行运行时特性初始化
         * @details 替代原 error_context_t::finalize_runtime_features_()。
         *          根据全局配置依次完成：错误码校验 → 堆栈捕获 → 源位置记录 → 插件通知。
         *          成功码上下文（sign=1）由调用方在调用前自行跳过。
         * @param ctx 待初始化的错误上下文
         */
        static void initialize(error_context_t& ctx) noexcept;
    };

}  // namespace error_system::core
