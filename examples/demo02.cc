#include <iostream>

#include "error_system.h"
#include "payment_service_errors.h"
#include "trade_service_errors.h"

using namespace error_system::core;
using namespace error_system::domain;

namespace {

/** @brief 模拟订单查询 */
result_t<int> query_order(int order_id) {
    if (order_id <= 0) {
        return result_t<int>{error_context_t(located_code_t{biz::trade_errors::ERR_ORDER_NOT_FOUND}, "无效的订单ID")};
    }
    if (order_id == 404) {
        return result_t<int>::make_error(biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单不存在");
    }
    return result_t<int>{order_id * 1000};
}

/** @brief 模拟支付处理 */
result_t<std::string> process_payment(int user_id, int amount) {
    if (amount <= 0) {
        return result_t<std::string>::make_error(
            biz::payment_errors::ERR_INSUFFICIENT_BALANCE, "无效的支付金额");
    }
    if (user_id == 999) {
        return result_t<std::string>::make_error(
            biz::payment_errors::ERR_ACCOUNT_FROZEN, "用户账户已被冻结");
    }
    return result_t<std::string>{std::string("PAY_") + std::to_string(user_id) + "_" + std::to_string(amount)};
}

}

int main() {
    std::cout << "===== Demo 2: result_t 错误处理 =====" << std::endl;

    std::cout << "\n--- 1. 成功的结果 ---" << std::endl;
    auto result1 = query_order(123);
    if (result1.is_success()) {
        std::cout << "查询成功，订单金额: " << result1.value() << std::endl;
    }

    std::cout << "\n--- 2. 错误的结果 ---" << std::endl;
    auto result2 = query_order(404);
    if (result2.is_error()) {
        std::cout << "查询失败: " << error_context_serializer_t::to_string(result2.error()) << std::endl;
    }

    std::cout << "\n--- 3. and_then 链式操作 ---" << std::endl;
    auto result3 = query_order(123).and_then([](int amount) -> result_t<std::string> {
        if (amount > 5000) {
            return result_t<std::string>::make_error(
                error_code_t(error_level_t::warn, system_domain_t::application, subsystem_id_t{0}, module_id_t{0}, error_number_t{1}),
                "订单金额超过限额");
        }
        return result_t<std::string>{std::string("订单金额正常: " + std::to_string(amount))};
    });

    if (result3.is_success()) {
        std::cout << result3.value() << std::endl;
    } else {
        std::cout << "处理失败: " << error_context_serializer_t::to_string(result3.error()) << std::endl;
    }

    std::cout << "\n--- 4. or_else 错误恢复 ---" << std::endl;
    auto result4 = query_order(404).or_else([](const error_context_t& error_context) -> result_t<int> {
        std::cout << "捕获错误: " << error_context.message << std::endl;
        std::cout << "返回默认值 0" << std::endl;
        return result_t<int>{0};
    });
    std::cout << "最终结果: " << result4.value() << std::endl;

    std::cout << "\n--- 5. value() 取值 ---" << std::endl;
    auto result5 = query_order(456);
    int amount5 = result5.value();
    std::cout << "value 取值: " << amount5 << " (已确认成功)" << std::endl;

    std::cout << "\n--- 6. operator bool() 条件判断 ---" << std::endl;
    auto result6 = query_order(789);
    if (result6) {
        std::cout << "operator bool: 成功, value = " << result6.value() << std::endl;
    }

    auto result6b = query_order(404);
    if (!result6b) {
        std::cout << "operator bool: 失败, error = " << result6b.error().message << std::endl;
    }

    std::cout << "\n--- 7. value_pointer() 安全指针访问 ---" << std::endl;
    auto result7 = query_order(123);
    if (auto* val = result7.value_pointer()) {
        std::cout << "value_pointer 取值: " << *val << std::endl;
    }

    auto result7b = query_order(404);
    if (result7b.value_pointer() == nullptr) {
        std::cout << "value_pointer 返回 nullptr (错误状态)" << std::endl;
    }

    std::cout << "\n--- 8. value_or() 提供默认值 ---" << std::endl;
    auto result8 = query_order(123);
    int default_val = -1;
    std::cout << "成功: value_or(-1) = " << result8.value_or(default_val) << std::endl;

    auto result8b = query_order(404);
    std::cout << "失败: value_or(-1) = " << result8b.value_or(default_val) << " (使用默认值)" << std::endl;

    std::cout << "\n--- 8b. value() / value_or() 安全取值 ---" << std::endl;
    auto result8c = query_order(100);
    std::cout << "成功: value() = " << result8c.value() << std::endl;
    std::cout << "成功: value_or(-1) = " << result8c.value_or(-1) << std::endl;

    auto result8d = query_order(404);
    std::cout << "失败: value() = " << result8d.value() << " (失败时返回 int 默认值 0)" << std::endl;
    std::cout << "失败: value_or(-1) = " << result8d.value_or(-1) << " (使用传入默认值)" << std::endl;

    auto success_result = result_t<int>::make_success(2024);
    std::cout << "make_success(2024): value() = " << success_result.value() << std::endl;

    std::cout << "\n--- 9. map() 值映射转换 ---" << std::endl;
    auto result9 = query_order(100)
        .map([](int amount) -> std::string {
            return "金额: " + std::to_string(amount) + " 元";
        });
    std::cout << "map 结果: " << result9.value() << std::endl;

    auto result9b = query_order(404)
        .map([](int amount) -> std::string {
            return "金额: " + std::to_string(amount) + " 元";
        });
    if (result9b.is_error()) {
        std::cout << "map 传递错误: " << result9b.error().message << std::endl;
    }

    std::cout << "\n--- 10. map_error() 错误映射转换 ---" << std::endl;
    auto result10 = query_order(404)
        .map_error([](const error_context_t& context) -> error_context_t {
            error_context_t wrapped(located_code_t{biz::trade_errors::ERR_CART_IS_EMPTY}, "下游订单服务故障");
            wrapped.with("cause", context.message)
                .with("original_code", std::to_string(context.get_code().get_code()));
            return wrapped;
        });
    std::cout << "map_error 包装后: " << error_context_serializer_t::to_string(result10.error()) << std::endl;

    std::cout << "\n--- 11. 复杂链式操作: 下单 -> 支付 ---" << std::endl;
    int user_id = 100;
    int order_id = 123;

    auto final_result =
        query_order(order_id)
            .and_then([user_id](int amount) -> result_t<std::string> {
                std::cout << "订单查询成功，金额: " << amount << std::endl;
                return process_payment(user_id, amount);
            })
            .or_else([](const error_context_t& error_context) -> result_t<std::string> {
                std::cout << "流程失败: " << error_context_serializer_t::to_string(error_context) << std::endl;
                return result_t<std::string>::make_error(
                    error_code_t(error_level_t::error, system_domain_t::application, subsystem_id_t{0}, module_id_t{0}, error_number_t{2}),
                    "交易流程失败");
            });

    if (final_result.is_success()) {
        std::cout << "支付成功，流水号: " << final_result.value() << std::endl;
    }

    std::cout << "\n--- 12. result_t<void> 空值结果 ---" << std::endl;
    result_t<void> void_ok;
    if (void_ok.is_success()) {
        std::cout << "void result 成功 (默认构造)" << std::endl;
    }

    result_t<void> void_err = result_t<void>::make_error(biz::trade_errors::ERR_CART_IS_EMPTY, "购物车为空");
    if (void_err.is_error()) {
        std::cout << "void result 错误: " << void_err.error().message << std::endl;
    }

    if (void_ok) {
        std::cout << "void result 成功检查通过" << std::endl;
    }

    return 0;
}
