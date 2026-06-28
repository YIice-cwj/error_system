#include <iostream>
#include <unordered_map>
#include <atomic>

#include "error_system.h"
#include "trade_service_errors.h"
#include "payment_service_errors.h"
#include "user_service_errors.h"
#include "redis_component_errors.h"

using namespace error_system::core;
using namespace error_system::plugin;
using namespace error_system::config;
using namespace error_system::domain;

// 自定义日志插件（记录所有级别）
class log_plugin_t : public i_error_plugin_t {
public:
    std::string_view name() const noexcept override {
        return "logger";
    }

    void on_error(const error_context_t& context) noexcept override {
        std::cerr << "[LOG] [" << to_string(context.get_code().get_level()) << "] "
                  << error_context_serializer_t::to_string(context) << std::endl;
    }

    // 记录所有级别，不限制
    error_level_t min_level() const noexcept override {
        return error_level_t::debug;
    }
};

// 统计插件（只记录 Error 及以上）
class stats_plugin_t : public i_error_plugin_t {
    std::unordered_map<uint64_t, std::atomic<int>> counters_;

public:
    std::string_view name() const noexcept override {
        return "stats";
    }

    error_level_t min_level() const noexcept override {
        return error_level_t::error;  // 只统计 Error 及更严重级别
    }

    void on_error(const error_context_t& context) noexcept override {
        ++counters_[context.get_code().get_code()];
    }

    int count(uint64_t code) const noexcept {
        auto it = counters_.find(code);
        return it != counters_.end() ? it->second.load() : 0;
    }

    void print_stats() const noexcept {
        std::cout << "\n=== 错误统计 ===" << std::endl;
        for (const auto& [code, count] : counters_) {
            std::cout << "  错误码 0x" << std::hex << code << std::dec
                      << ": " << count << " 次" << std::endl;
        }
    }
};

// 告警插件（只处理严重错误，Fatal 及以上）
class alert_plugin_t : public i_error_plugin_t {
public:
    std::string_view name() const noexcept override {
        return "alert";
    }

    error_level_t min_level() const noexcept override {
        return error_level_t::fatal;  // 只处理 Fatal 级别
    }

    void on_error(const error_context_t& context) noexcept override {
        std::cerr << "[ALERT] 严重错误需要立即处理!" << std::endl;
        std::cerr << "        " << context.message << std::endl;
    }
};

int main() {
    std::cout << "===== Demo 3: 插件系统 =====" << std::endl;

    // 1. 注册插件
    std::cout << "\n--- 1. 注册插件 ---" << std::endl;
    log_plugin_t logger;
    stats_plugin_t stats;
    alert_plugin_t alert;

    auto& registry = plugin_registry_t::instance();
    registry.register_plugin_ref(logger);
    registry.register_plugin_ref(stats);
    registry.register_plugin_ref(alert);

    std::cout << "已注册 " << registry.size() << " 个插件" << std::endl;
    std::cout << "  logger min_level = debug (所有级别)" << std::endl;
    std::cout << "  stats  min_level = error (Error 及以上)" << std::endl;
    std::cout << "  alert  min_level = fatal (Fatal 及以上)" << std::endl;

    // 2. min_level() 级别过滤效果
    std::cout << "\n--- 2. min_level() 级别过滤 ---" << std::endl;

    std::cout << "\n>> Info 级别错误 (stats 不统计，alert 不告警):" << std::endl;
    error_context_t ctx_info(biz::user_errors::ERR_TOKEN_EXPIRED, "Token 已过期");

    std::cout << "\n>> Warn 级别错误 (stats 不统计，alert 不告警):" << std::endl;
    error_context_t ctx_warn(biz::user_errors::ERR_PASSWORD_MISMATCH, "密码错误");

    std::cout << "\n>> Error 级别错误 (stats 统计，alert 不告警):" << std::endl;
    error_context_t ctx_error(biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单不存在");

    std::cout << "\n>> Fatal 级别错误 (stats 统计，alert 告警):" << std::endl;
    error_context_t ctx_fatal(biz::payment_errors::ERR_ACCOUNT_FROZEN, "账户冻结");

    // 3. 查看统计
    std::cout << "\n--- 3. 错误统计结果 ---" << std::endl;
    stats.print_stats();
    std::cout << "  预期: 仅 Error 和 Fatal 被统计 (2 条)" << std::endl;

    // 4. 异步通知模式 + 背压控制
    std::cout << "\n--- 4. 异步通知模式 + 背压控制 ---" << std::endl;

    // 启用异步通知模式
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::async_queue);
    std::cout << "通知模式已切换为: async_queue" << std::endl;

    // 设置背压上限
    registry.set_max_queue_size(100);
    std::cout << "最大队列大小: " << registry.get_max_queue_size() << std::endl;
    std::cout << "（队列满时，新通知将被丢弃，避免内存无限增长）" << std::endl;

    // 在异步模式下创建错误（通知被推入异步队列）
    std::cout << "\n>> 异步模式错误通知（进入队列）:" << std::endl;
    error_context_t ctx_async1(biz::trade_errors::ERR_ORDER_NOT_FOUND, "异步通知测试 #1");
    error_context_t ctx_async2(biz::trade_errors::ERR_CART_IS_EMPTY, "异步通知测试 #2");

    // 恢复同步模式
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);
    std::cout << "\n通知模式已恢复为: sync" << std::endl;

    // 4b. sync_deferred 延迟通知模式
    std::cout << "\n--- 4b. sync_deferred 延迟通知模式 ---" << std::endl;
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync_deferred);
    std::cout << "通知模式已切换为: sync_deferred（错误先入线程本地缓冲，flush 时统一通知）" << std::endl;

    registry.set_deferred_buffer_size(1024);
    std::cout << "延迟缓冲容量: " << registry.get_deferred_buffer_size() << std::endl;

    // 构造错误上下文：通知进入缓冲，插件此时不会被调用
    std::cout << "\n>> 构造 3 个错误上下文（缓冲中，插件尚未触发）:" << std::endl;
    error_context_t ctx_deferred1(biz::trade_errors::ERR_ORDER_NOT_FOUND, "延迟通知 #1");
    error_context_t ctx_deferred2(biz::trade_errors::ERR_ORDER_NOT_FOUND, "延迟通知 #2");
    error_context_t ctx_deferred3(biz::trade_errors::ERR_ORDER_NOT_FOUND, "延迟通知 #3");
    std::cout << "  待 flush 通知数: " << registry.pending_deferred_notifications() << std::endl;

    // flush 触发批量通知
    std::cout << "\n>> flush_deferred_notifications() 触发批量通知:" << std::endl;
    registry.flush_deferred_notifications();
    std::cout << "  flush 后待通知数: " << registry.pending_deferred_notifications() << std::endl;
    std::cout << "  预期: logger 打印 3 条日志，stats 统计 +3" << std::endl;

    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);
    std::cout << "\n通知模式已恢复为: sync" << std::endl;

    // 5. 使用错误路由插件
    std::cout << "\n--- 5. 错误路由插件 ---" << std::endl;

    error_router_plugin_t::instance().register_handler_by_code(
        biz::trade_errors::ERR_ORDER_NOT_FOUND,
        [](const error_context_t& context) {
            std::cout << "[路由] 特定错误处理: " << context.message << std::endl;
        }
    );

    error_router_plugin_t::instance().register_handler_by_domain(
        system_domain_t::middleware,
        [](const error_context_t& context) {
            std::cout << "[路由] 中间件域错误: " << error_context_serializer_t::to_string(context) << std::endl;
        }
    );

    registry.register_plugin_ref(error_router_plugin_t::instance());

    std::cout << "\n>> 触发特定错误码路由:" << std::endl;
    error_context_t ctx_route1(biz::trade_errors::ERR_ORDER_NOT_FOUND, "测试特定路由");

    std::cout << "\n>> 触发中间件域路由:" << std::endl;
    error_context_t ctx_route2(infra::redis_errors::ERR_POOL_EXHAUSTED, "Redis故障");

    // 6. 动态注销插件
    std::cout << "\n--- 6. 动态注销插件 ---" << std::endl;
    registry.unregister_plugin("alert");
    std::cout << "注销 alert 后插件数量: " << registry.size() << std::endl;

    std::cout << "\n>> Fatal 错误不再触发告警:" << std::endl;
    error_context_t ctx_fatal2(biz::payment_errors::ERR_WX_PAY_TIMEOUT, "支付超时（Fatal）");

    // 7. 清空所有插件
    std::cout << "\n--- 7. 清空所有插件 ---" << std::endl;
    registry.clear();
    std::cout << "清空后插件数量: " << registry.size() << std::endl;

    return 0;
}