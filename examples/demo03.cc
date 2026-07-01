/**
 * @file demo03.cc
 * @brief 插件系统 API 全览
 * @details 演示 plugin_registry_t、i_error_plugin_t、error_router_plugin_t、
 *          error_dedup_sampler_t、通知模式（sync/async/sync_deferred）的全部公共 API。
 *          每个示例只做一件事，标题即 API 名。
 */

#include <atomic>
#include <iostream>
#include <unordered_map>

#include "error_system.h"
#include "error_system/plugin/error_dedup_sampler.h"
#include "payment_service_errors.h"
#include "redis_component_errors.h"
#include "trade_service_errors.h"
#include "user_service_errors.h"

using namespace error_system::core;
using namespace error_system::plugin;
using namespace error_system::config;
using namespace error_system::domain;

namespace {

/** @brief 打印小节标题 */
void section(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

}  // namespace

/**
 * @brief 自定义日志插件：打印所有级别错误
 * @details 继承 i_error_plugin_t，实现 name()/on_error()/min_level() 三个纯虚函数
 */
class log_plugin_t : public i_error_plugin_t {
public:
    std::string_view name() const noexcept override { return "logger"; }

    error_level_t min_level() const noexcept override {
        return error_level_t::debug;  // 接收所有级别
    }

    void on_error(const error_context_t& context) noexcept override {
        std::cerr << "[LOG] " << context.message << std::endl;
    }
};

/** @brief 统计插件：用 atomic 计数各错误码出现次数 */
class stats_plugin_t : public i_error_plugin_t {
    std::unordered_map<uint64_t, std::atomic<int>> counters_;

public:
    std::string_view name() const noexcept override { return "stats"; }

    error_level_t min_level() const noexcept override {
        return error_level_t::error;  // 只统计 error 及以上
    }

    void on_error(const error_context_t& context) noexcept override {
        ++counters_[context.get_code().get_code()];
    }

    int total() const noexcept {
        int sum = 0;
        for (const auto& [_, c] : counters_) { sum += c.load(); }
        return sum;
    }
};

int main() {
    std::cout << "===== Demo 3: 插件系统 API 全览 =====" << std::endl;

    auto& registry = plugin_registry_t::instance();

    // ============================================================
    // 一、插件注册：3 种方式
    // ============================================================
    section("1.1 register_plugin_ref 注册引用（栈对象）");
    log_plugin_t logger;
    registry.register_plugin_ref(logger);
    std::cout << "  插件数: " << registry.size() << std::endl;

    section("1.2 register_plugin 注册 unique_ptr（堆对象）");
    auto stats_ptr = std::make_unique<stats_plugin_t>();
    stats_plugin_t* stats_ref = stats_ptr.get();
    registry.register_plugin(std::move(stats_ptr));
    std::cout << "  插件数: " << registry.size() << std::endl;

    section("1.3 min_level 级别过滤效果");
    std::cout << "  logger min_level=debug (所有级别)" << std::endl;
    std::cout << "  stats  min_level=error (error+fatal)" << std::endl;

    // ============================================================
    // 二、插件查询
    // ============================================================
    section("2.1 size() 插件数量");
    std::cout << "  " << registry.size() << std::endl;

    section("2.2 empty() 是否为空");
    std::cout << "  empty = " << registry.empty() << std::endl;

    section("2.3 is_initialized() 是否已初始化");
    std::cout << "  is_initialized = " << plugin_registry_t::is_initialized() << std::endl;

    // ============================================================
    // 三、错误通知触发
    // ============================================================
    section("3.1 触发 error 级别（logger + stats 都收到）");
    error_context_t ctx_err{biz::trade_errors::ERR_ORDER_NOT_FOUND, "订单错误"};
    std::cout << "  stats 统计总数: " << stats_ref->total() << std::endl;

    section("3.2 触发 info 级别（仅 logger 收到，stats 过滤）");
    error_context_t ctx_info{biz::user_errors::ERR_TOKEN_EXPIRED, "Token 过期"};
    std::cout << "  stats 统计总数: " << stats_ref->total() << " (应不变)" << std::endl;

    // ============================================================
    // 四、插件注销
    // ============================================================
    section("4.1 unregister_plugin 按名称注销");
    registry.unregister_plugin("logger");
    std::cout << "  注销后插件数: " << registry.size() << std::endl;

    section("4.2 clear 清空所有插件");
    registry.clear();
    std::cout << "  清空后插件数: " << registry.size() << std::endl;

    // ============================================================
    // 五、错误路由插件
    // ============================================================
    section("5.1 error_router_plugin_t 按错误码路由");
    error_router_plugin_t::instance().register_handler_by_code(
        biz::trade_errors::ERR_ORDER_NOT_FOUND,
        [](const error_context_t& ctx) {
            std::cout << "  [路由] 订单错误: " << ctx.message << std::endl;
        });
    registry.register_plugin_ref(error_router_plugin_t::instance());
    error_context_t ctx_route{biz::trade_errors::ERR_ORDER_NOT_FOUND, "路由测试"};
    std::cout << "  (上方应出现路由输出)" << std::endl;

    section("5.2 register_handler_by_domain 按域路由");
    error_router_plugin_t::instance().register_handler_by_domain(
        system_domain_t::middleware,
        [](const error_context_t& ctx) {
            std::cout << "  [中间件域] " << ctx.message << std::endl;
        });
    error_context_t ctx_mid{infra::redis_errors::ERR_POOL_EXHAUSTED, "Redis 故障"};
    std::cout << "  (上方应出现中间件域输出)" << std::endl;

    section("5.3 unregister_handler_by_code 注销路由");
    error_router_plugin_t::instance().unregister_handler_by_code(biz::trade_errors::ERR_ORDER_NOT_FOUND);
    error_context_t ctx_unroute{biz::trade_errors::ERR_ORDER_NOT_FOUND, "不再路由"};
    std::cout << "  (上方不应出现路由输出)" << std::endl;

    registry.unregister_plugin("router");
    error_router_plugin_t::instance().unregister_handler_by_domain(system_domain_t::middleware);

    // ============================================================
    // 六、通知模式：sync / async / sync_deferred
    // ============================================================
    log_plugin_t logger2;
    registry.register_plugin_ref(logger2);

    section("6.1 sync 同步模式（默认）");
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);
    std::cout << "  触发错误时立即通知" << std::endl;
    error_context_t ctx_sync{biz::trade_errors::ERR_ORDER_NOT_FOUND, "同步通知"};

    section("6.2 async_queue 异步模式 + set_max_queue_size 背压");
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::async_queue);
    registry.set_max_queue_size(100);
    std::cout << "  max_queue_size = " << registry.get_max_queue_size() << std::endl;
    std::cout << "  pending = " << registry.pending_notifications() << std::endl;
    error_context_t ctx_async{biz::trade_errors::ERR_ORDER_NOT_FOUND, "异步通知"};
    std::cout << "  入队后 pending = " << registry.pending_notifications() << std::endl;

    section("6.3 sync_deferred 延迟模式 + set_deferred_buffer_size");
    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync_deferred);
    registry.set_deferred_buffer_size(1024);
    std::cout << "  buffer_size = " << registry.get_deferred_buffer_size() << std::endl;

    section("6.4 pending_deferred_notifications 待通知数");
    error_context_t d1{biz::trade_errors::ERR_ORDER_NOT_FOUND, "延迟1"};
    error_context_t d2{biz::trade_errors::ERR_ORDER_NOT_FOUND, "延迟2"};
    std::cout << "  pending = " << registry.pending_deferred_notifications() << std::endl;

    section("6.5 flush_deferred_notifications 批量通知");
    registry.flush_deferred_notifications();
    std::cout << "  flush 后 pending = " << registry.pending_deferred_notifications() << std::endl;

    section("6.6 clear_deferred_notifications 清空缓冲");
    error_context_t d3{biz::trade_errors::ERR_ORDER_NOT_FOUND, "延迟3"};
    auto dropped = registry.clear_deferred_notifications();
    std::cout << "  清空丢弃数 = " << dropped << std::endl;

    section("6.7 deferred_buffer_overflowed 缓冲溢出标志");
    std::cout << "  overflowed = " << registry.deferred_buffer_overflowed() << std::endl;

    feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);
    registry.clear();

    // ============================================================
    // 七、错误去重采样器
    // ============================================================
    section("7.1 error_dedup_sampler_t 去重窗口");
    error_dedup_sampler_t sampler;
    sampler.set_dedup_window_ms(1000);  // 1 秒内同错误码去重
    std::cout << "  dedup_window_ms = 1000" << std::endl;

    section("7.2 set_sample_rate 采样率");
    sampler.set_sample_rate(0.5);  // 50% 采样
    std::cout << "  sample_rate = 0.5 (50% 采样)" << std::endl;

    section("7.3 should_be_forwarded 判定是否放行");
    error_context_t sample_ctx{biz::trade_errors::ERR_ORDER_NOT_FOUND, "采样测试"};
    bool forward = sampler.should_be_forwarded(sample_ctx);
    std::cout << "  should_be_forwarded = " << forward << std::endl;

    section("7.4 deduped_count / sampled_count / forwarded_count 统计");
    std::cout << "  deduped  = " << sampler.deduped_count() << std::endl;
    std::cout << "  sampled  = " << sampler.sampled_count() << std::endl;
    std::cout << "  forwarded= " << sampler.forwarded_count() << std::endl;

    section("7.5 reset_stats 重置统计");
    sampler.reset_stats();
    std::cout << "  重置后 forwarded = " << sampler.forwarded_count() << std::endl;

    section("7.6 clear_dedup_cache 清空去重缓存");
    sampler.clear_dedup_cache();
    std::cout << "  缓存已清空" << std::endl;

    return 0;
}
