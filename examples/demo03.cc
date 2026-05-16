#include "error_system.h"
#include "trade_service_errors.h"
#include "payment_service_errors.h"
#include "user_service_errors.h"
#include "redis_component_errors.h"
#include <iostream>
#include <unordered_map>
#include <atomic>

using namespace error_system::core;
using namespace error_system::plugin;
using namespace error_system::domain;

// 自定义日志插件
class log_plugin_t : public i_error_plugin_t {
public:
    std::string_view name() const noexcept override {
        return "logger";
    }

    void on_error(const error_context_t& ctx) noexcept override {
        std::cerr << "[LOG] [" << to_string(ctx.code.get_level()) << "] " 
                  << ctx.to_string() << std::endl;
    }
};

// 统计插件
class stats_plugin_t : public i_error_plugin_t {
    std::unordered_map<uint64_t, std::atomic<int>> counters_;

public:
    std::string_view name() const noexcept override {
        return "stats";
    }

    void on_error(const error_context_t& ctx) noexcept override {
        ++counters_[ctx.code.get_code()];
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

// 告警插件（只处理严重错误）
class alert_plugin_t : public i_error_plugin_t {
public:
    std::string_view name() const noexcept override {
        return "alert";
    }

    void on_error(const error_context_t& ctx) noexcept override {
        if (ctx.code.get_level() >= error_level_t::error) {
            std::cerr << "[ALERT] 严重错误需要立即处理!" << std::endl;
            std::cerr << "        " << ctx.message << std::endl;
        }
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
    registry.register_plugin(&logger);
    registry.register_plugin(&stats);
    registry.register_plugin(&alert);

    std::cout << "已注册 " << registry.size() << " 个插件" << std::endl;

    // 2. 创建错误（自动触发插件）
    std::cout << "\n--- 2. 创建错误触发插件 ---" << std::endl;
    
    std::cout << "\n>> 创建 Warn 级别错误:" << std::endl;
    error_context_t ctx1(biz::user_errors::ERR_PASSWORD_MISMATCH, "密码错误");
    
    std::cout << "\n>> 创建 Error 级别错误:" << std::endl;
    error_context_t ctx2(biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单不存在");
    
    std::cout << "\n>> 创建 Fatal 级别错误:" << std::endl;
    error_context_t ctx3(biz::payment_errors::ERR_ACCOUNT_FROZEN, "账户冻结");

    // 3. 查看统计
    std::cout << "\n--- 3. 错误统计 ---" << std::endl;
    stats.print_stats();

    // 4. 使用错误路由插件
    std::cout << "\n--- 4. 错误路由插件 ---" << std::endl;
    
    // 注册路由处理器
    error_router_plugin_t::instance().register_handler_by_code(
        biz::trade_errors::ERR_ORDER_NOT_FOUND,
        [](const error_context_t& ctx) {
            std::cout << "[路由] 特定错误处理: " << ctx.message << std::endl;
        }
    );

    error_router_plugin_t::instance().register_handler_by_domain(
        system_domain_t::database,
        [](const error_context_t& ctx) {
            std::cout << "[路由] 数据库域错误: " << ctx.to_string() << std::endl;
        }
    );

    // 将路由插件注册到全局注册表
    registry.register_plugin(&error_router_plugin_t::instance());

    std::cout << "\n>> 触发特定错误码路由:" << std::endl;
    error_context_t ctx4(biz::trade_errors::ERR_ORDER_NOT_FOUND, "测试路由");

    std::cout << "\n>> 触发数据库域路由:" << std::endl;
    error_context_t ctx5(infra::redis_errors::ERR_POOL_EXHAUSTED, "Redis故障");

    // 5. 动态注销插件
    std::cout << "\n--- 5. 动态注销插件 ---" << std::endl;
    registry.unregister_plugin("alert");
    std::cout << "注销后插件数量: " << registry.size() << std::endl;

    std::cout << "\n>> 创建新错误（alert插件已注销）:" << std::endl;
    error_context_t ctx6(biz::payment_errors::ERR_WX_PAY_TIMEOUT, "支付超时");

    // 6. 清空所有插件
    std::cout << "\n--- 6. 清空所有插件 ---" << std::endl;
    registry.clear();
    std::cout << "清空后插件数量: " << registry.size() << std::endl;

    return 0;
}
