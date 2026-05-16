#include "error_system.h"
#include "generated/trade_service_errors.h"
#include <iostream>
using namespace error_system::core;
using namespace error_system::config;
using namespace error_system::domain;

int main() {
    error_context_t ctx(biz::trade_errors::ERR_ORDER_NOT_FOUND, "用户点击了结算按钮，但校验失败");
    ctx.with("user_id", "8848").with("action", "checkout");
    std::cout << ctx << std::endl;
    return 0;
}