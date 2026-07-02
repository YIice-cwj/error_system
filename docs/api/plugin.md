# 🔌 Plugin 层 API

> `error_system::plugin`

---

## 🔌 i_error_plugin_t

插件抽象接口，所有自定义插件必须继承。注册到 `plugin_registry_t` 后，每次构造 `error_context_t` 时自动收到 `on_error()` 回调。通过 `min_level()` 在框架侧快速丢弃低等级事件，避免回调开销。

```cpp
class i_error_plugin_t {
public:
    virtual ~i_error_plugin_t() noexcept = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual core::error_level_t min_level() const noexcept { return core::error_level_t::debug; }
    virtual void on_error(const core::error_context_t& context) noexcept = 0;
};
```

| 方法 | 说明 |
|------|------|
| `name()` | 插件唯一标识（同名注册会替换旧插件），`[[nodiscard]]` |
| `min_level()` | 低于此等级的事件被框架级过滤（默认 `debug` = 接收所有） |
| `on_error()` | 错误事件回调，必须 `noexcept`，异常不会传播 |

---

## 📋 plugin_registry_t

插件单例注册表，支持 **同步** / **异步队列** / **同步延迟** 三种通知模式。热路径 RCU 快照零拷贝无锁读取（`shared_ptr<const vector>` + `atomic_load/store`）。

```cpp
class plugin_registry_t {
public:
    static plugin_registry_t& instance() noexcept;
    [[nodiscard]] static bool is_initialized() noexcept;  // 单例是否已初始化（instance() 调用后为 true）
    // 注册：转移所有权 / 非持有引用（单例、栈对象）
    void register_plugin(std::unique_ptr<i_error_plugin_t> plugin) noexcept;
    void register_plugin_ref(i_error_plugin_t& plugin) noexcept;
    void unregister_plugin(std::string_view name) noexcept;
    // 同步通知（RCU 快照，无锁读取，回调异常被捕获）
    void notify_error(const core::error_context_t& context) noexcept;
    // 异步入队 / 延迟入队 / flush
    void enqueue_notification(const core::error_context_t& context) noexcept;
    void enqueue_deferred_notification(const core::error_context_t& context) noexcept;
    void flush_deferred_notifications() noexcept;
    [[nodiscard]] size_t pending_deferred_notifications() const noexcept;
    size_t clear_deferred_notifications() noexcept;
    // 延迟缓冲控制（0 = 无限制）
    void set_deferred_buffer_size(size_t max_size) noexcept;
    [[nodiscard]] size_t get_deferred_buffer_size() const noexcept;
    [[nodiscard]] bool deferred_buffer_overflowed() const noexcept;
    [[nodiscard]] size_t size() const noexcept; [[nodiscard]] bool empty() const noexcept; void clear() noexcept;
    // 异步队列背压控制（0 = 无限制）
    [[nodiscard]] size_t pending_notifications() const noexcept;
    void set_max_queue_size(size_t max_size) noexcept;
    [[nodiscard]] size_t get_max_queue_size() const noexcept;
};
```

### 通知模式

| 模式 | 行为 | 丢失风险 | 适用 |
|------|------|:---:|------|
| `sync` | 构造 `error_context_t` 时同步调用 `on_error()` | 无 | 日志、指标（轻量） |
| `async_queue` | 通知推入后台队列，工作线程异步消费 | 队列满时拒绝入队 | 网络上报、持久化（可能阻塞） |
| `sync_deferred` | 通知累积到线程本地缓冲，显式 `flush_deferred_notifications()` 触发 | 缓冲满时丢弃（标记 overflow） | 请求处理批处理、事务边界累积 |

三种通知模式的选择决策树详见 [决策树 · 1](../decision_tree.md#1-通知模式选择)。

### 内部流程

```
error_context_t 构造
 → notify_error()                  （sync 模式，仅通知等级匹配的插件）
 → enqueue_notification()          （async_queue 模式，shared_ptr 副本推入 async_queue_t）
 → enqueue_deferred_notification() （sync_deferred 模式，推入线程本地缓冲）
    → flush_deferred_notifications() → 遍历缓冲逐个 notify_error()
 → 工作线程 wait() → pop → 处理 → notify_error()  （仅 async_queue）
```

- `notify_error()` 复制回调指针后释放锁再调用 `on_error()`，避免持锁执行用户代码死锁
- async_queue 队列满拒绝入队（背压）；sync_deferred 缓冲满丢弃并标记 overflow；`flushing` 标志防止 flush 期间递归入队
- 工作线程首次 `enqueue()` 自动启动，析构自动 join；处理器异常隔离不退出（async_queue）

### 使用示例

```cpp
auto& registry = plugin::plugin_registry_t::instance();
registry.register_plugin(std::make_unique<my_plugin_t>());   // 转移所有权
my_plugin_t plugin; registry.register_plugin_ref(plugin);    // 非持有引用
// 异步模式 + 背压控制
feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::async_queue);
registry.set_max_queue_size(10000);
// 延迟模式 + flush
feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync_deferred);
registry.set_deferred_buffer_size(1024); registry.flush_deferred_notifications();
```

> ⚠️ `register_plugin(unique_ptr)` 接管所有权；`register_plugin_ref()` 不持有所有权，调用方负责生命周期。`sync_deferred` 模式下构造 `error_context_t` 会自动入队，无需手动调用 `enqueue_deferred_notification()`。

---

## 📨 async_notification_channel_t

异步通知通道 — 封装 `async_queue_t`，负责错误上下文的异步入队与转发。不感知具体插件注册表实现，通过构造时注入通知回调打破循环依赖；自动管理后台线程生命周期，支持背压控制（队列满时丢弃新通知）。

```cpp
class async_notification_channel_t {
public:
    using context_ptr_t = std::shared_ptr<core::error_context_t>;
    using notify_callback_t = std::function<void(const core::error_context_t&)>;
    explicit async_notification_channel_t(notify_callback_t callback) noexcept;  // ~()=default; 不可拷贝/移动
    void enqueue_notification(const core::error_context_t& context) noexcept;
    [[nodiscard]] size_t pending_notifications() const noexcept;
    void set_max_queue_size(size_t max_size) noexcept;       // 0 = 无限制
    [[nodiscard]] size_t get_max_queue_size() const noexcept;
};
```

---

## 🔀 error_router_plugin_t

按错误码 / 系统域 / 模块组将错误路由到不同处理函数。线程安全（`std::shared_mutex`）。

```cpp
class error_router_plugin_t : public i_error_plugin_t {
public:
    using error_handler_t = std::function<void(const core::error_context_t&)>;
    static error_router_plugin_t& instance() noexcept;
    std::string_view name() const noexcept override; void on_error(const core::error_context_t& context) noexcept override;
    // 注册 / 注销（按 错误码 / 模块组 / 系统域）
    void register_handler_by_code(const core::error_code_t& code, error_handler_t handler) noexcept;
    void register_handler_by_module_group_id(core::module_group_id_t module_group_id, error_handler_t handler) noexcept;
    void register_handler_by_domain(domain::system_domain_t domain, error_handler_t handler) noexcept;
    void unregister_handler_by_code(const core::error_code_t& code) noexcept;
    void unregister_handler_by_module_group_id(core::module_group_id_t module_group_id) noexcept;
    void unregister_handler_by_domain(domain::system_domain_t domain) noexcept;
};
```

### 路由优先级

1. 错误码精确匹配
2. 模块组匹配
3. 系统域匹配

路由插件自身是 `i_error_plugin_t`，需通过 `register_plugin_ref()` 注册到 `plugin_registry_t` 后才会被通知。

---

## 🎯 error_dedup_sampler_t

错误去重与采样器 — 对流入的错误上下文做去重与采样判定，不感知下游通知通道。线程安全，可被多个生产者线程共享调用。去重基于 `error_code_t::get_identity_code()`（忽略 sign/reserved 位），保证同业务错误的不同瞬态标记仍被去重。采样采用确定性计数器（每 N 个放行 1 个），避免随机数开销，适合高频热路径。

```cpp
class error_dedup_sampler_t {
public:
    using clock_t = std::chrono::steady_clock; using time_point_t = clock_t::time_point;
    error_dedup_sampler_t() noexcept = default; ~error_dedup_sampler_t() noexcept = default;  // 不可拷贝、不可移动
    void set_dedup_window_ms(uint64_t milliseconds) noexcept;           // 去重窗口(ms)，0 = 关闭（默认）
    void set_sample_rate(double rate) noexcept;                         // 采样率 [0.0, 1.0]
    [[nodiscard]] bool should_be_forwarded(const core::error_context_t& context) noexcept;  // 先采样再去重，两关通过才放行
    [[nodiscard]] uint64_t deduped_count() const noexcept;              // 被去重抑制
    [[nodiscard]] uint64_t sampled_count() const noexcept;              // 被采样抑制
    [[nodiscard]] uint64_t forwarded_count() const noexcept;            // 放行数
    void reset_stats() noexcept;                                        // 重置统计（建议仅无并发/测试）
    void clear_dedup_cache() noexcept; [[nodiscard]] size_t dedup_cache_size() const noexcept;  // 清除去重表
};
```

### 使用示例

```cpp
plugin::error_dedup_sampler_t sampler;
sampler.set_dedup_window_ms(1000);   // 同一 identity code 1s 内只放行一次
sampler.set_sample_rate(0.1);        // 放行 10%
if (sampler.should_be_forwarded(ctx)) { registry.enqueue_notification(ctx); }
// 查看抑制效果：deduped_count() / sampled_count() / forwarded_count()
```

> ⚠️ **注意**：`should_be_forwarded()` 非 `const`（内部更新计数与去重表）；`reset_stats()` 并发调用可能与 `should_be_forwarded()` 竞争，仅建议无并发/测试场景。

---

## 📖 插件开发指南

### 三步速查

1. **继承 `i_error_plugin_t`**：实现 `name()` 与 `on_error()`，可选重写 `min_level()` 做框架级过滤
2. **注册到 `plugin_registry_t`**：`register_plugin(unique_ptr)` 转移所有权，或 `register_plugin_ref()` 引用注册（单例、栈对象）
3. **保证线程安全**：`on_error()` 可能被多线程并发调用，使用 `std::atomic` 或锁保护内部状态

### 完整示例：统计插件

```cpp
class stats_plugin_t : public i_error_plugin_t {
    std::unordered_map<uint64_t, std::atomic<int>> counters_;
public:
    std::string_view name() const noexcept override { return "stats"; }
    core::error_level_t min_level() const noexcept override { return core::error_level_t::error; }
    void on_error(const core::error_context_t& ctx) noexcept override {
        ++counters_[ctx.get_code().get_code()];   // atomic 保证多线程安全
    }
};

// 注册使用
plugin::plugin_registry_t::instance().register_plugin(std::make_unique<stats_plugin_t>());
```
