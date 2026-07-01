/**
 * @file demo02.cc
 * @brief result_t<T> 结果类型 API 全览
 * @details 演示 result_t<T> 的全部公共 API：构造、取值、映射、链式、匹配、上下文传播。
 *          覆盖主模板 result_t<T> 与特化 result_t<void>。每个示例只做一件事。
 */

#include <iostream>
#include <string>

#include "error_system.h"
#include "payment_service_errors.h"
#include "trade_service_errors.h"

using namespace error_system::core;
using namespace error_system::domain;

namespace {

/** @brief 打印小节标题 */
void section(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

/** @brief 模拟订单查询：成功返回金额，失败返回错误 */
result_t<int> query_order(int order_id) {
    if (order_id <= 0) {
        return result_t<int>::make_error(biz::trade_errors::ERR_ORDER_NOT_FOUND, "无效的订单ID");
    }
    if (order_id == 404) {
        return result_t<int>::make_error(biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单不存在");
    }
    return result_t<int>{order_id * 1000};
}

/** @brief 模拟支付处理 */
result_t<std::string> process_payment(int user_id, int amount) {
    if (amount <= 0) {
        return result_t<std::string>::make_error(biz::payment_errors::ERR_INSUFFICIENT_BALANCE, "金额无效");
    }
    if (user_id == 999) {
        return result_t<std::string>::make_error(biz::payment_errors::ERR_ACCOUNT_FROZEN, "账户已被冻结");
    }
    return result_t<std::string>{"PAY_" + std::to_string(user_id) + "_" + std::to_string(amount)};
}

}  // namespace

int main() {
    std::cout << "===== Demo 2: result_t<T> API 全览 =====" << std::endl;

    // ============================================================
    // 一、构造：4 种方式
    // ============================================================
    section("1.1 构造：result_t(T value) 成功值");
    result_t<int> r1{42};
    std::cout << "  is_success=" << r1.is_success() << " value=" << r1.value() << std::endl;

    section("1.2 构造：result_t(error_context) 错误上下文");
    error_context_t ctx{biz::trade_errors::ERR_ORDER_NOT_FOUND, "构造错误"};
    result_t<int> r2{ctx};
    std::cout << "  is_error=" << r2.is_error() << " message=" << r2.error().message << std::endl;

    section("1.3 工厂：make_success(value) 显式成功");
    auto r3 = result_t<int>::make_success(2024);
    std::cout << "  value=" << r3.value() << std::endl;

    section("1.4 工厂：make_error(code, message) 显式错误");
    auto r4 = result_t<int>::make_error(biz::trade_errors::ERR_ORDER_NOT_FOUND, "工厂构造错误");
    std::cout << "  is_error=" << r4.is_error() << " message=" << r4.error().message << std::endl;

    // ============================================================
    // 二、状态判断
    // ============================================================
    section("2.1 is_success / is_error");
    std::cout << "  r1: is_success=" << r1.is_success() << " is_error=" << r1.is_error() << std::endl;

    section("2.2 operator bool()");
    if (r1) { std::cout << "  r1 转 bool 为 true" << std::endl; }
    if (!r4) { std::cout << "  r4 转 bool 为 false" << std::endl; }

    // ============================================================
    // 三、取值：5 种方式
    // ============================================================
    section("3.1 value() 取成功值（错误时返回哨兵）");
    std::cout << "  r1.value() = " << r1.value() << std::endl;

    section("3.2 value_pointer() 安全指针（错误返回 nullptr）");
    if (auto* p = r1.value_pointer()) {
        std::cout << "  r1.value_pointer() = " << *p << std::endl;
    }
    if (r4.value_pointer() == nullptr) {
        std::cout << "  r4.value_pointer() = nullptr (错误状态)" << std::endl;
    }

    section("3.3 value_or(default) 失败时返回默认值");
    std::cout << "  r1.value_or(-1) = " << r1.value_or(-1) << std::endl;
    std::cout << "  r4.value_or(-1) = " << r4.value_or(-1) << " (使用默认值)" << std::endl;

    section("3.4 operator* 解引用（等价 value()）");
    std::cout << "  *r1 = " << *r1 << std::endl;
    *r1 = 100;
    std::cout << "  修改后 *r1 = " << *r1 << std::endl;

    section("3.5 operator-> 成员访问");
    result_t<std::string> rs{std::string("hello")};
    std::cout << "  rs->size() = " << rs->size() << std::endl;

    // ============================================================
    // 四、错误访问
    // ============================================================
    section("4.1 error() 获取错误上下文");
    std::cout << "  r4.error().message = " << r4.error().message << std::endl;

    section("4.2 error() 可变引用（可修改错误）");
    r4.error().with("extra", "附加信息");
    std::cout << "  附加 payload 后: " << r4.error().payload_size() << " 项" << std::endl;

    // ============================================================
    // 五、map：值映射
    // ============================================================
    section("5.1 map 成功时转换值类型");
    auto m1 = query_order(123).map([](int amount) -> std::string {
        return "金额: " + std::to_string(amount) + " 元";
    });
    std::cout << "  " << m1.value() << std::endl;

    section("5.2 map 错误时透传错误");
    auto m2 = query_order(404).map([](int amount) -> std::string {
        return "金额: " + std::to_string(amount);
    });
    std::cout << "  is_error=" << m2.is_error() << " message=" << m2.error().message << std::endl;

    // ============================================================
    // 六、map_error：错误映射
    // ============================================================
    section("6.1 map_error 错误时转换错误上下文");
    auto me1 = query_order(404).map_error([](const error_context_t& e) -> error_context_t {
        error_context_t wrapped{biz::trade_errors::ERR_CART_IS_EMPTY, "下游订单服务故障"};
        wrapped.with("cause", e.message);
        return wrapped;
    });
    std::cout << "  转换后: " << me1.error().message << std::endl;

    section("6.2 map_error 成功时保持原值");
    auto me2 = query_order(123).map_error([](const error_context_t&) -> error_context_t {
        return error_context_t{biz::trade_errors::ERR_CART_IS_EMPTY, "不应被调用"};
    });
    std::cout << "  value=" << me2.value() << " (成功路径未触发)" << std::endl;

    // ============================================================
    // 七、and_then：链式操作
    // ============================================================
    section("7.1 and_then 成功时执行下一步");
    auto chain1 = query_order(123).and_then([](int amount) -> result_t<std::string> {
        return result_t<std::string>{"金额=" + std::to_string(amount)};
    });
    std::cout << "  " << chain1.value() << std::endl;

    section("7.2 and_then 错误时跳过执行");
    bool called = false;
    auto chain2 = query_order(404).and_then([&called]([[maybe_unused]] int amount) -> result_t<std::string> {
        called = true;
        return result_t<std::string>{"不应执行"};
    });
    std::cout << "  is_error=" << chain2.is_error() << " called=" << called << std::endl;

    section("7.3 and_then 多层链式（查订单 → 支付）");
    int user_id = 100;
    auto chain3 = query_order(123).and_then([user_id](int amount) -> result_t<std::string> {
        return process_payment(user_id, amount);
    });
    std::cout << "  " << chain3.value() << std::endl;

    // ============================================================
    // 八、or_else：错误恢复
    // ============================================================
    section("8.1 or_else 错误时恢复");
    auto rec1 = query_order(404).or_else([](const error_context_t& e) -> result_t<int> {
        std::cout << "  捕获错误: " << e.message << ", 返回默认值 0" << std::endl;
        return result_t<int>{0};
    });
    std::cout << "  恢复后 value=" << rec1.value() << std::endl;

    section("8.2 or_else 成功时跳过恢复");
    auto rec2 = query_order(123).or_else([](const error_context_t&) -> result_t<int> {
        return result_t<int>{-1};
    });
    std::cout << "  value=" << rec2.value() << " (成功路径未触发)" << std::endl;

    // ============================================================
    // 九、match：模式匹配
    // ============================================================
    section("9.1 match 强制处理成功/错误两条路径");
    auto login = [](bool ok) -> result_t<std::string> {
        if (ok) { return result_t<std::string>::make_success("token-abc"); }
        return result_t<std::string>::make_error(biz::trade_errors::ERR_ORDER_NOT_FOUND, "登录失败");
    };

    auto msg_ok = login(true).match(
        [](const std::string& token) { return "成功: token=" + token; },
        [](const error_context_t& e) { return "失败: " + std::string(e.message); });
    std::cout << "  " << msg_ok << std::endl;

    auto msg_err = login(false).match(
        [](const std::string& token) { return "成功: token=" + token; },
        [](const error_context_t& e) { return "失败: " + std::string(e.message); });
    std::cout << "  " << msg_err << std::endl;

    // ============================================================
    // 十、context：传播时附加上下文（新增 API）
    // ============================================================
    section("10.1 context(k,v) 错误传播时追加 payload");
    auto with_ctx = query_order(404).context("host", "192.168.1.1").context("port", 3306);
    std::cout << "  payload 项数: " << with_ctx.error().payload_size() << std::endl;
    std::cout << "  host = " << with_ctx.error().get_payload_value("host").value_or("?") << std::endl;

    section("10.2 context 成功路径无操作");
    auto ok_ctx = query_order(123).context("host", "192.168.1.1");
    std::cout << "  is_success=" << ok_ctx.is_success() << " value=" << ok_ctx.value() << std::endl;

    section("10.3 context 链式传播（多层调用栈）");
    auto outer = query_order(404)
                     .context("layer", "inner")
                     .context("layer", "middle")
                     .context("layer", "outer");
    std::cout << "  最终 payload 项数: " << outer.error().payload_size() << std::endl;

    // ============================================================
    // 十一、ERROR_SYSTEM_TRY 宏：早返回
    // ============================================================
    section("11.1 ERROR_SYSTEM_TRY 保留成功值并早返回错误");
    auto try_demo = [](int id) -> result_t<int> {
        ERROR_SYSTEM_TRY(amount, query_order(id));
        return result_t<int>{amount.value() * 2};
    };
    std::cout << "  成功: " << try_demo(123).value() << std::endl;
    std::cout << "  失败: " << try_demo(404).error().message << std::endl;

    section("11.2 ERROR_SYSTEM_TRY_DISCARD 丢弃成功值（仅传播错误）");
    auto try_discard = [](int id) -> result_t<int> {
        ERROR_SYSTEM_TRY_DISCARD(query_order(id));
        return result_t<int>{0};
    };
    std::cout << "  成功: " << try_discard(123).value() << " (丢弃查询值)" << std::endl;
    std::cout << "  失败: " << try_discard(404).error().message << std::endl;

    // ============================================================
    // 十二、result_t<void> 特化
    // ============================================================
    section("12.1 result_t<void> 默认构造（成功）");
    result_t<void> void_ok;
    std::cout << "  is_success=" << void_ok.is_success() << std::endl;

    section("12.2 result_t<void> make_error");
    auto void_err = result_t<void>::make_error(biz::trade_errors::ERR_CART_IS_EMPTY, "购物车为空");
    std::cout << "  is_error=" << void_err.is_error() << " message=" << void_err.error().message << std::endl;

    section("12.3 result_t<void> make_success");
    auto void_success = result_t<void>::make_success();
    std::cout << "  is_success=" << void_success.is_success() << std::endl;

    section("12.4 result_t<void> and_then 链式");
    auto void_chain = result_t<void>::make_success().and_then([]() -> result_t<void> {
        std::cout << "  第一步执行" << std::endl;
        return result_t<void>::make_success();
    });
    std::cout << "  链式结果: " << void_chain.is_success() << std::endl;

    section("12.5 result_t<void> context 附加上下文");
    auto void_ctx = result_t<void>::make_error(biz::trade_errors::ERR_CART_IS_EMPTY, "空")
                        .context("user_id", "8848");
    std::cout << "  payload 项数: " << void_ctx.error().payload_size() << std::endl;

    section("12.6 result_t<void> or_else 错误恢复");
    auto void_recover = result_t<void>::make_error(biz::trade_errors::ERR_CART_IS_EMPTY, "失败")
                            .or_else([](const error_context_t&) -> result_t<void> {
                                return result_t<void>::make_success();
                            });
    std::cout << "  恢复后: " << void_recover.is_success() << std::endl;

    return 0;
}
