#pragma once
/**
 * @file trade_service_errors.h
 * @brief 🚀 自动生成的错误码定义，请勿手动修改！
 * @note Service: trade, Subsystem ID: 101
 */

#include "error_system/core/error_registry.h"
#include "error_system/domain/system_domain.h"

namespace biz::trade_errors {

    // 模块: 订单模块
    DEFINE_ERROR_CODE(ERR_ORDER_NOT_FOUND,
                      error_system::core::error_level_t::error,
                      error_system::domain::system_domain_t::application,
                      101,  // subsystem: trade
                      1,    // module: order
                      1,
                      "订单不存在或已删除");

    // 模块: 支付模块
    DEFINE_ERROR_CODE(ERR_PAYMENT_TIMEOUT,
                      error_system::core::error_level_t::warn,
                      error_system::domain::system_domain_t::application,
                      101,  // subsystem: trade
                      2,    // module: payment
                      1,
                      "支付网关连接超时");

}  // namespace biz::trade_errors