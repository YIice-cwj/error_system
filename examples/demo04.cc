/**
 * @file demo04.cc
 * @brief 演示 error_exception_t 的异常传递机制
 * @details 项目规范要求所有函数 noexcept，生产代码应优先使用 result_t 模式。
 *          此处 throw 仅为演示 error_exception_t 的用法。
 */
#include <iostream>

#include "error_system.h"
#include "payment_service_errors.h"
#include "trade_service_errors.h"

using namespace error_system::core;
using namespace error_system::domain;

namespace {

/** @brief 模拟可能抛出异常的订单处理 */
void process_order(int order_id) {
    if (order_id <= 0) {
        auto code = error_code_t(error_level_t::error, system_domain_t::application, subsystem_id_t{101}, module_id_t{1}, error_number_t{1});
        error_context_t context(located_code_t{code}, "无效的订单ID: {}", order_id);
        throw error_exception_t(std::move(context));
    }

    if (order_id == 404) {
        throw error_exception_t(error_context_t(located_code_t{biz::trade_errors::ERR_ORDER_NOT_FOUND}, "订单不存在: {}", order_id));
    }

    std::cout << "订单 " << order_id << " 处理成功" << std::endl;
}

/** @brief 模拟可能抛出异常的支付处理 */
void process_payment(int user_id, int amount) {
    if (amount <= 0) {
        throw error_exception_t(
            error_context_t(located_code_t{biz::payment_errors::ERR_INSUFFICIENT_BALANCE}, "支付金额无效: {}", amount));
    }

    if (user_id == 999) {
        throw error_exception_t(
            error_context_t(located_code_t{biz::payment_errors::ERR_ACCOUNT_FROZEN}, "账户已被冻结，用户ID: {}", user_id));
    }

    std::cout << "用户 " << user_id << " 支付 " << amount << " 成功" << std::endl;
}

}

int main() {
    std::cout << "===== Demo 4: 异常处理 =====" << std::endl;

    std::cout << "\n--- 1. 基本异常捕获 ---" << std::endl;
    try {
        process_order(-1);
    } catch (const error_exception_t& exception) {
        std::cout << "捕获到异常:" << std::endl;
        std::cout << "  what(): " << exception.what() << std::endl;
        std::cout << "  code(): 0x" << std::hex << exception.code().get_code() << std::dec << std::endl;
    }

    std::cout << "\n--- 2. 作为 std::exception 捕获 ---" << std::endl;
    try {
        process_order(404);
    } catch (const std::exception& exception) {
        std::cout << "作为 std::exception 捕获: " << exception.what() << std::endl;
    }

    std::cout << "\n--- 3. 获取完整错误上下文 ---" << std::endl;
    try {
        process_payment(999, 100);
    } catch (const error_exception_t& exception) {
        std::cout << "异常信息:" << std::endl;
        std::cout << "  消息: " << exception.context().message << std::endl;
        std::cout << "  等级: " << static_cast<int>(exception.context().get_code().get_level()) << std::endl;
        std::cout << "  系统: " << static_cast<int>(exception.context().get_code().get_system()) << std::endl;
        std::cout << "  JSON: " << error_context_serializer_t::to_json(exception.context()) << std::endl;
    }

    std::cout << "\n--- 4. 嵌套异常处理 ---" << std::endl;
    try {
        try {
            process_order(404);
        } catch (const error_exception_t& inner) {
            auto code = error_code_t(error_level_t::fatal, system_domain_t::application, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
            error_context_t outer(located_code_t{code}, "订单服务调用失败");
            outer.with("inner_code", std::to_string(inner.code().get_code()));

            std::cout << "内部异常: " << inner.what() << std::endl;
            std::cout << "外部异常: " << error_context_serializer_t::to_string(outer) << std::endl;
            throw;
        }
    } catch (const error_exception_t& exception) {
        std::cout << "最终捕获: " << exception.what() << std::endl;
    }

    std::cout << "\n--- 5. 正常流程 ---" << std::endl;
    try {
        process_order(123);
        process_payment(100, 500);
        std::cout << "所有操作成功完成!" << std::endl;
    } catch (const error_exception_t& exception) {
        std::cout << "操作失败: " << exception.what() << std::endl;
    }

    std::cout << "\n--- 6. 异常与 result_t 结合 ---" << std::endl;
    auto safe_process = [](int order_id) -> result_t<int> {
        try {
            process_order(order_id);
            return result_t<int>(order_id * 100);
        } catch (const error_exception_t& exception) {
            return result_t<int>(exception.context());
        }
    };

    auto result1 = safe_process(123);
    if (result1.is_success()) {
        std::cout << "成功，结果: " << result1.value() << std::endl;
    }

    auto result2 = safe_process(404);
    if (result2.is_error()) {
        std::cout << "失败: " << result2.error().message << std::endl;
    }

    return 0;
}
