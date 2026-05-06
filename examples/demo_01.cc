#include "error_system/core/error_builder.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_level.h"
#include "error_system/core/result.h"
#include "error_system/domain/system_domain.h"
#include "error_system/module/ai_module.h"
#include "error_system/subsystem/ai_llm_subsystem.h"
#include "error_system/traits/module_dispatcher.h"
#include "error_system/traits/subsystem_dispatcher.h"

#include <iostream>
#include <string>

namespace demo {

    using namespace error_system;

    // =======================================================================
    // 第 1 部分：错误码构建与解析
    // =======================================================================

    /**
     * @brief 演示如何构建和解析错误码
     */
    void demo_error_code() {
        std::cout << "\n========== 1. 错误码构建与解析 ==========\n";

        // 使用 error_builder_t 构建错误码
        auto code = core::error_builder_t::make_error_code(core::error_level_t::error,
                                                           domain::system_domain_t::ai,
                                                           subsystem::ai_llm_subsystem_t::text_generation,
                                                           module::ai_module_t::inference_engine,
                                                           404);

        std::cout << "原始错误码: 0x" << std::hex << code.get_code() << std::dec << "\n";
        std::cout << "符号位: " << static_cast<int>(code.get_sign()) << "\n";
        std::cout << "错误等级: " << core::to_string(code.get_level()) << "\n";
        std::cout << "系统域: " << domain::SYSTEM_DOMAIN_STRING[static_cast<int>(code.get_system())] << "\n";
        std::cout << "子系统值: 0x" << std::hex << code.get_subsys() << std::dec << "\n";
        std::cout << "模块值: 0x" << std::hex << code.get_module() << std::dec << "\n";
        std::cout << "错误编号: " << code.get_number() << "\n";
    }

    // =======================================================================
    // 第 2 部分：错误上下文与错误链
    // =======================================================================

    /**
     * @brief 演示错误上下文的创建和错误链包装
     */
    void demo_error_context() {
        std::cout << "\n========== 2. 错误上下文与错误链 ==========\n";

        // 创建根错误
        auto root_code = core::error_builder_t::make_error_code(
            core::error_level_t::error, domain::system_domain_t::kernel, 1, 3, 1);
        core::error_context_t root_ctx(root_code, "数据库连接超时");

        // 创建上层错误，包装根错误
        auto wrap_code = core::error_builder_t::make_error_code(
            core::error_level_t::fatal, domain::system_domain_t::application, 0x0101, 0x0301, 100);
        core::error_context_t wrap_ctx(wrap_code, "用户请求处理失败");

        // 构建错误链
        core::error_context_t chained = wrap_ctx.wrap(root_ctx);

        // 打印错误链
        std::cout << "错误链: " << chained.to_string() << "\n";
    }

    // =======================================================================
    // 第 3 部分：result_t 结果类型
    // =======================================================================

    /**
     * @brief 模拟一个可能失败的操作
     */
    core::result_t<int> divide(int a, int b) {
        if (b == 0) {
            auto code = core::error_builder_t::make_error_code(
                core::error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
            return core::error_context_t(code, "除数不能为零");
        }
        return a / b;
    }

    /**
     * @brief 演示 result_t 的使用方式
     */
    void demo_result() {
        std::cout << "\n========== 3. result_t 结果类型 ==========\n";

        // 成功场景
        auto ok = divide(10, 2);
        std::cout << "10 / 2: ";
        if (ok.is_success()) {
            std::cout << "成功，结果 = " << ok.value() << "\n";
        } else {
            std::cout << "失败，原因 = " << ok.error().message << "\n";
        }

        // 失败场景
        auto err = divide(10, 0);
        std::cout << "10 / 0: ";
        if (err.is_success()) {
            std::cout << "成功，结果 = " << err.value() << "\n";
        } else {
            std::cout << "失败，原因 = " << err.error().message << "\n";
        }
    }

    // =======================================================================
    // 第 4 部分：模块 traits 转换
    // =======================================================================

    /**
     * @brief 演示 module_traits 的枚举与字符串互转
     */
    void demo_module_traits() {
        std::cout << "\n========== 4. 模块 Traits 转换 ==========\n";

        // 枚举 -> 整数
        auto mod = module::ai_module_t::inference_engine;
        auto int_val = traits::module_traits<module::ai_module_t>::to_int(mod);
        std::cout << "枚举 inference_engine -> 整数: 0x" << std::hex << int_val << std::dec << "\n";

        // 整数 -> 枚举
        auto recovered = traits::module_traits<module::ai_module_t>::from_int(0x0803);
        std::cout << "整数 0x0803 -> 枚举: " << traits::module_traits<module::ai_module_t>::to_string(recovered)
                  << "\n";

        // 枚举 -> 字符串
        auto str = traits::module_traits<module::ai_module_t>::to_string(module::ai_module_t::vector_search);
        std::cout << "枚举 vector_search -> 字符串: " << str << "\n";

        // 字符串 -> 枚举
        auto from_str = traits::module_traits<module::ai_module_t>::from_string("fine_tuner");
        std::cout << "字符串 fine_tuner -> 枚举: 0x" << std::hex
                  << traits::module_traits<module::ai_module_t>::to_int(from_str) << std::dec << "\n";

        // 有效性校验
        std::cout << "0x0800 是否有效: " << (traits::module_traits<module::ai_module_t>::is_valid(0x0800) ? "是" : "否")
                  << "\n";
        std::cout << "0x0900 是否有效: " << (traits::module_traits<module::ai_module_t>::is_valid(0x0900) ? "是" : "否")
                  << "\n";
    }

    // =======================================================================
    // 第 5 部分：子系统 traits 转换
    // =======================================================================

    /**
     * @brief 演示 subsystem_traits 的枚举与字符串互转
     */
    void demo_subsystem_traits() {
        std::cout << "\n========== 5. 子系统 Traits 转换 ==========\n";

        // 枚举 -> 整数
        auto sub = subsystem::kernel_cpu_subsystem_t::scheduler;
        auto int_val = traits::subsystem_traits<subsystem::kernel_cpu_subsystem_t>::to_int(sub);
        std::cout << "枚举 scheduler -> 整数: 0x" << std::hex << int_val << std::dec << "\n";

        // 整数 -> 枚举
        auto recovered = traits::subsystem_traits<subsystem::kernel_cpu_subsystem_t>::from_int(0x0305);
        std::cout << "整数 0x0305 -> 枚举: "
                  << traits::subsystem_traits<subsystem::kernel_cpu_subsystem_t>::to_string(recovered) << "\n";

        // 枚举 -> 字符串
        auto str = traits::subsystem_traits<subsystem::kernel_cpu_subsystem_t>::to_string(
            subsystem::kernel_cpu_subsystem_t::thermal_manager);
        std::cout << "枚举 thermal_manager -> 字符串: " << str << "\n";

        // 字符串 -> 枚举
        auto from_str = traits::subsystem_traits<subsystem::kernel_cpu_subsystem_t>::from_string("irq_balancer");
        std::cout << "字符串 irq_balancer -> 枚举: 0x" << std::hex
                  << traits::subsystem_traits<subsystem::kernel_cpu_subsystem_t>::to_int(from_str) << std::dec << "\n";
    }

    // =======================================================================
    // 第 6 部分：分发器自动解析
    // =======================================================================

    /**
     * @brief 演示 module_dispatcher 和 subsystem_dispatcher 的自动解析
     */
    void demo_dispatcher() {
        std::cout << "\n========== 6. 分发器自动解析 ==========\n";

        // 根据数值自动解析模块名称
        std::cout << "模块 0x0803 名称: " << traits::resolve_module(0x0803) << "\n";
        std::cout << "模块 0x0B0C 名称: " << traits::resolve_module(0x0B0C) << "\n";
        std::cout << "模块 0xFFFF 名称: " << traits::resolve_module(0xFFFF) << "\n";

        // 根据数值自动解析子系统名称
        std::cout << "子系统 0x1006 名称: " << traits::resolve_subsystem(0x1006) << "\n";
        std::cout << "子系统 0x0302 名称: " << traits::resolve_subsystem(0x0302) << "\n";
        std::cout << "子系统 0xFFFF 名称: " << traits::resolve_subsystem(0xFFFF) << "\n";

        // 根据字符串反向解析枚举值
        std::cout << "ai/fine_tuner -> 0x" << std::hex << traits::module_from_string("ai", "fine_tuner") << std::dec
                  << "\n";
        std::cout << "kernel_cpu/scheduler -> 0x" << std::hex
                  << traits::subsystem_from_string("kernel_cpu", "scheduler") << std::dec << "\n";
    }

}  // namespace demo

// =======================================================================
// 主函数：按顺序调用各演示函数
// =======================================================================

int main() {
    std::cout << "error_system 功能演示\n";
    std::cout << "=====================";

    demo::demo_error_code();
    demo::demo_error_context();
    demo::demo_result();
    demo::demo_module_traits();
    demo::demo_subsystem_traits();
    demo::demo_dispatcher();

    std::cout << "\n演示结束\n";
    return 0;
}
