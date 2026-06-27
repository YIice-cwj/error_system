# Plugin 层 API

> `error_system::plugin`

---

## i_error_plugin_t

插件抽象接口，所有自定义插件必须继承。

```cpp
class i_error_plugin_t {
public:
    virtual ~i_error_plugin_t() noexcept = default;

    // 插件唯一标识（同名注册会替换旧插件）
    virtual std::string_view name() const noexcept = 0;

    // 错误事件回调（必须 noexcept，异常不会传播）
    virtual void on_error(const core::error_context_t& context) noexcept = 0;

    // 最低错误等级过滤（默认 debug = 接收所有）
    virtual core::error_level_t min_level() const noexcept {
        return core::error_level_t::debug;
    }
};
```

| 方法 | 说明 |
|------|------|
| `on_error()` | 错误事件回调，必须 `noexcept` |
| `name()` | 插件唯一标识 |
| `min_level()` | 低于此等级的通知被框架级过滤 |

---

## plugin_registry_t

插件单例注册表，支持 **同步** 和 **异步队列** 两种通知模式。

> 热路径 RCU 快照零拷贝无锁读取（`shared_ptr<const vector>` + `atomic_load/store`）。

### API

```cpp
class plugin_registry_t {
public:
    static plugin_registry_t& instance() noexcept;

    // 转移所有权：注册表接管插件生命周期
    void register_plugin(std::unique_ptr<i_error_plugin_t> plugin) noexcept;
    // 非持有引用：适用于单例、栈对象等场景
    void register_plugin_ref(i_error_plugin_t& plugin) noexcept;
    void unregister_plugin(std::string_view name) noexcept;

    // 同步通知（RCU 快照，无锁读取，回调异常被捕获不会传播）
    void notify_error(const core::error_context_t& context) noexcept;

    // 异步入队（推入 async_queue_t，独立工作线程消费）
    void enqueue_notification(const core::error_context_t& context) noexcept;

    size_t size() const noexcept;
    bool empty() const noexcept;
    void clear() noexcept;

    // 队列状态
    size_t pending_notifications() const noexcept;

    // 背压控制（0 = 无限制）
    void set_max_queue_size(size_t max_size) noexcept;
    size_t get_max_queue_size() const noexcept;
};
```

### 通知模式

| 模式 | 行为 | 适用 |
|------|------|------|
| `sync` | 构造 `error_context_t` 时同步调用 `on_error()` | 日志、指标（轻量） |
| `async_queue` | 通知推入后台队列，工作线程异步消费 | 网络上报、持久化（可能阻塞） |

切换通知模式：

```cpp
using namespace error_system::config;
feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::async_queue);
```

### 异步内部流程

```
error_context_t 构造
 → notify_error()              → 仅通知等级匹配的插件（sync 模式）
 → enqueue_notification()      → shared_ptr 副本推入 async_queue_t（async 模式）
 → 工作线程 wait() → pop → 处理 → notify_error()
```

- 工作线程首次 `enqueue()` 自动启动，析构自动 join
- 队列满时拒绝入队（背压控制）
- 处理器异常隔离，工作线程不退出
- `notify_error()` 中：复制回调指针 → 释放锁 → 调用 `on_error()`，避免持锁执行用户代码导致死锁

### 使用示例

```cpp
auto& registry = plugin::plugin_registry_t::instance();

// 转移所有权：注册表接管插件生命周期
registry.register_plugin(std::make_unique<my_plugin_t>());

// 非持有引用：适用于单例、栈对象
my_plugin_t plugin;
registry.register_plugin_ref(plugin);

// 切换异步模式 + 背压控制
feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::async_queue);
registry.set_max_queue_size(10000);
```

> `register_plugin(unique_ptr)` 接管插件所有权，注销/替换时自动释放。
> `register_plugin_ref()` 不持有所有权，调用方负责生命周期，避免注册临时对象。

---

## async_notification_channel_t

异步通知通道 — 封装 `async_queue_t`，负责错误上下文的异步入队与转发。

> 单一职责：不感知具体插件注册表实现，通过构造时注入通知回调打破循环依赖。

```cpp
class async_notification_channel_t {
public:
    using notify_callback_t = std::function<void(const core::error_context_t&)>;

    explicit async_notification_channel_t(notify_callback_t callback) noexcept;
    ~async_notification_channel_t() noexcept;

    async_notification_channel_t(const async_notification_channel_t&) = delete;
    async_notification_channel_t& operator=(const async_notification_channel_t&) = delete;
    async_notification_channel_t(async_notification_channel_t&&) = delete;
    async_notification_channel_t& operator=(async_notification_channel_t&&) = delete;

    void enqueue_notification(const core::error_context_t& context) noexcept;
    size_t pending_notifications() const noexcept;
    void set_max_queue_size(size_t max_size) noexcept;
    size_t get_max_queue_size() const noexcept;
};
```

---

## error_router_plugin_t

按错误码 / 系统域 / 模块组将错误路由到不同处理函数。

### API

```cpp
class error_router_plugin_t : public i_error_plugin_t {
public:
    using handler_t = std::function<void(const error_context_t&)>;
    static error_router_plugin_t& instance() noexcept;

    void register_handler_by_code(const core::error_code_t& code, handler_t handler) noexcept;
    void register_handler_by_domain(uint8_t domain, handler_t handler);
    void register_handler_by_module_group(uint16_t module_group_id, handler_t handler);

    void on_error(const error_context_t& context) noexcept override;
    std::string_view name() const noexcept override;
};
```

### 路由优先级

1. 错误码精确匹配
2. 模块组匹配
3. 系统域匹配

---

## 插件开发指南

### 最小实现

```cpp
class my_plugin_t : public i_error_plugin_t {
    void on_error(const error_context_t& ctx) noexcept override { /* ... */ }
    std::string_view name() const noexcept override { return "my_plugin"; }
};
```

### 线程安全

`on_error()` 可能被多线程并发调用，需自行保证线程安全。建议使用 `std::atomic` 或锁保护内部状态。

### 性能建议

| 建议 | 做法 |
|------|------|
| 避免阻塞 | 同步模式不在 `on_error()` 中执行 I/O |
| 解耦调用 | 切换 `notify_mode` 为 `async_queue` |
| 快速过滤 | 重写 `min_level()` 框架级丢弃低等级事件 |

```cpp
class efficient_plugin_t : public i_error_plugin_t {
    core::error_level_t min_level() const noexcept override {
        return core::error_level_t::error;  // 仅接收 error 及以上
    }
    void on_error(const error_context_t& ctx) noexcept override {
        // 序列化通过独立的 serializer 类，不在 ctx 上调用
        async_sender_.send(error_context_serializer_t::to_json(ctx));
    }
    std::string_view name() const noexcept override { return "efficient"; }
};
```

### 统计插件示例

```cpp
class stats_plugin_t : public i_error_plugin_t {
    std::unordered_map<uint64_t, std::atomic<int>> counters_;

public:
    std::string_view name() const noexcept override { return "stats"; }
    error_level_t min_level() const noexcept override { return error_level_t::error; }

    void on_error(const error_context_t& ctx) noexcept override {
        ++counters_[ctx.get_code().get_code()];
    }

    int total_count() const noexcept {
        int sum = 0;
        for (const auto& [_, cnt] : counters_) sum += cnt.load();
        return sum;
    }
};
```
