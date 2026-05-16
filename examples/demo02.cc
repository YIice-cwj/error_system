#include "error_system.h"
#include "trade_service_errors.h"
#include "payment_service_errors.h"
#include <iostream>

using namespace error_system::core;
using namespace error_system::domain;

// 模拟订单查询
result_t<int> query_order(int order_id) {
    if (order_id <= 0) {
        return error_context_t(
            biz::trade_errors::ERR_ORDER_NOT_FOUND,
            "无效的订单ID: {}", order_id
        );
    }
    if (order_id == 404) {
        return error_context_t(
            biz::trade_errors::ERR_ORDER_NOT_FOUND,
            "订单不存在: {}", order_id
        );
    }
    return order_id * 1000;  // 返回订单金额
}

// 模拟支付处理
result_t<std::string> process_payment(int user_id, int amount) {
    if (amount <= 0) {
        return error_context_t(
            biz::payment_errors::ERR_INSUFFICIENT_BALANCE,
            "无效的支付金额: {}", amount
        );
    }
    if (user_id == 999) {
        return error_context_t(
            biz::payment_errors::ERR_ACCOUNT_FROZEN,
            "用户账户已被冻结: {}", user_id
        );
    }
    return std::string("PAY_") + std::to_string(user_id) + "_" + std::to_string(amount);
}

int main() {
    std::cout << "===== Demo 2: result_t 错误处理 =====" << std::endl;
    
    // 1. 成功的结果
    std::cout << "\n--- 1. 成功的结果 ---" << std::endl;
    auto result1 = query_order(123);
    if (result1.is_success()) {
        std::cout << "查询成功，订单金额: " << result1.value() << std::endl;
    }
    
    // 2. 错误的结果
    std::cout << "\n--- 2. 错误的结果 ---" << std::endl;
    auto result2 = query_order(404);
    if (result2.is_error()) {
        std::cout << "查询失败: " << result2.error().to_string() << std::endl;
    }
    
    // 3. and_then 链式操作
    std::cout << "\n--- 3. and_then 链式操作 ---" << std::endl;
    auto result3 = query_order(123)
        .and_then([](int amount) -> result_t<std::string> {
            if (amount > 5000) {
                return error_context_t(
                    error_builder_t::make_error_code(
                        error_level_t::warn, system_domain_t::application, 0, 0, 1
                    ),
                    "订单金额超过限额: {}", amount
                );
            }
            return std::string("订单金额正常: " + std::to_string(amount));
        });
    
    if (result3.is_success()) {
        std::cout << result3.value() << std::endl;
    } else {
        std::cout << "处理失败: " << result3.error().to_string() << std::endl;
    }
    
    // 4. or_else 错误恢复
    std::cout << "\n--- 4. or_else 错误恢复 ---" << std::endl;
    auto result4 = query_order(404)
        .or_else([](const error_context_t& err) -> result_t<int> {
            std::cout << "捕获错误: " << err.message << std::endl;
            std::cout << "返回默认值 0" << std::endl;
            return 0;  // 返回默认值
        });
    std::cout << "最终结果: " << result4.value() << std::endl;
    
    // 5. 复杂链式操作
    std::cout << "\n--- 5. 复杂链式操作: 下单 -> 支付 ---" << std::endl;
    int user_id = 100;
    int order_id = 123;
    
    auto final_result = query_order(order_id)
        .and_then([user_id](int amount) -> result_t<std::string> {
            std::cout << "订单查询成功，金额: " << amount << std::endl;
            return process_payment(user_id, amount);
        })
        .or_else([](const error_context_t& err) -> result_t<std::string> {
            std::cout << "流程失败: " << err.to_string() << std::endl;
            return error_context_t(
                error_builder_t::make_error_code(
                    error_level_t::error, system_domain_t::application, 0, 0, 2
                ),
                "交易流程失败"
            );
        });
    
    if (final_result.is_success()) {
        std::cout << "支付成功，流水号: " << final_result.value() << std::endl;
    }
    
    // 6. void result 特化
    std::cout << "\n--- 6. void result 特化 ---" << std::endl;
    result_t<void> void_result;  // 成功
    if (void_result.is_success()) {
        std::cout << "void result 成功" << std::endl;
    }
    
    result_t<void> void_error = error_context_t(
        biz::trade_errors::ERR_CART_IS_EMPTY,
        "购物车为空"
    );
    if (void_error.is_error()) {
        std::cout << "void result 错误: " << void_error.error().message << std::endl;
    }
    
    return 0;
}
