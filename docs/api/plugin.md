# 🔌 Plugin 层 API

> `error_system::plugin`

---

## 🧬 i_error_plugin_t

插件抽象接口，所有自定义插件必须继承。

```cpp
class i_error_plugin_t {
public:
    virtual ~i_error_plugin_t() = default;
    virtual void on_error(const error_context_t& context) noexcept = 0;
    virtual std::string_view name() const noexcept = 0;

    // 🔽 最低错误等级过滤（默认 debug = 接收所有）
    virtual core::error_level_t min_level() const noexcept {
        return core::error_level_t::debug;
    }
};
```

| 方法 | 说明 |
|------|------|
| `on_error()` | 错误事件回调 |
| `name()` | 插件唯一标识 |
| `min_level()` | 低于此等级的通知被框架级过滤 |

---

## 📡 plugin_registry_t

插件单例注册表，支持 **同步** 和 **异步队列** 两种通知模式。

> ⚡ 热路径 RCU 快照零拷贝无锁读取

### API

```cpp
class plugin_registry_t {
public:
    static plugin_registry_t& instance() noexcept;

    void register_plugin(i_error_plugin_t* plugin) noexcept;
    void unregister_plugin(std::string_view name) noexcept;

    // ⚡ 同步通知（RCU 快照，无锁读取）
    void notify_error(const core::error_context_t& context) noexcept;

    // ⏳ 异步入队（推入 async_queue_t，独立工作线程消费）
    void enqueue_notification(const core::error_context_t& context) noexcept;

    size_t size() const noexcept;
    bool empty() const noexcept;
    void clear() noexcept;
    size_t pending_notifications() const noexcept;

    // 🚦 背压控制（0 = 无限制）
    void set_max_queue_size(size_t max_size) noexcept;
    size_t get_max_queue_size() const noexcept;
};
```

### 🔀 通知模式

| 模式 | 行为 | 适用 |
|------|------|------|
| `sync` | 构造 `error_context_t` 时同步调用 `on_error()` | 日志、指标（轻量） |
| `async_queue` | 通知推入后台队列，工作线程异步消费 | 网络上报、持久化（可能阻塞） |

### ⏳ 异步内部流程

```
error_context_t 构造
 → notify_error()              → 仅通知等级匹配的插件
 → enqueue_notification()      → shared_ptr 副本推入 async_queue_t
 → 工作线程 wait() → pop → 处理 → notify_error()
```

- 工作线程首次 `enqueue()` 自动启动，析构自动 join
- 队列满时拒绝入队
- 处理器异常隔离，工作线程不退出

### 📝 使用示例

```cpp
auto& registry = plugin_registry_t::instance();
registry.register_plugin(logger.get());

// 切换异步模式
error_config_t::set_notify_mode(error_config_t::notify_mode_t::async_queue);
registry.set_max_queue_size(10000);
```

> ⚠️ `plugin_registry_t` 不持有插件所有权，调用方管理生命周期，避免注册临时对象。

---

## 🚦 error_router_plugin_t

按错误码 / 系统域 / 模块组将错误路由到不同处理函数。

### API

```cpp
class error_router_plugin_t : public i_error_plugin_t {
public:
    using handler_t = std::function<void(const error_context_t&)>;
    static error_router_plugin_t& instance() noexcept;

    void register_handler_by_code(error_code_t code, handler_t handler);
    void register_handler_by_domain(uint8_t domain, handler_t handler);
    void register_handler_by_module_group(uint16_t module_group_id, handler_t handler);

    void on_error(const error_context_t& context) noexcept override;
    std::string_view name() const noexcept override;
};
```

### 🎯 路由优先级

1. 🥇 错误码精确匹配
2. 🥈 模块组匹配
3. 🥉 系统域匹配

---

## 📖 插件开发指南

### 最小实现

```cpp
class my_plugin_t : public i_error_plugin_t {
    void on_error(const error_context_t& ctx) noexcept override { /* ... */ }
    std::string_view name() const noexcept override { return "my_plugin"; }
};
```

### 🔒 线程安全

`on_error()` 可能被多线程并发调用，需自行保证线程安全。

### ⚡ 性能建议

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
        async_sender_.send(ctx.to_json());
    }
    std::string_view name() const noexcept override { return "efficient"; }
};
```