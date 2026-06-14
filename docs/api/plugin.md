# Plugin 层 API 参考

> 命名空间: `error_system::plugin`

---

## i_error_plugin_t

插件抽象接口，所有自定义插件必须继承此接口。

```cpp
class i_error_plugin_t {
public:
    virtual ~i_error_plugin_t() = default;

    virtual void on_error(const error_context_t& context) noexcept = 0;
    virtual std::string_view name() const noexcept = 0;

    // v2.1 新增：插件关心的最低错误等级，低于此等级的 error_context_t 不会通知到此插件
    virtual core::error_level_t min_level() const noexcept { return core::error_level_t::debug; }
};
```

### 方法说明

| 方法 | 说明 |
|------|------|
| `on_error()` | 错误事件回调，每当 `error_context_t` 构造时触发 |
| `name()` | 返回插件名称，用于标识和注销 |
| `min_level()` | v2.1 新增：插件关心的最低错误等级，默认 debug（接收所有） |

---

## plugin_registry_t

插件单例注册表，负责管理所有插件的生命周期和错误事件广播。

支持**同步**和**异步队列**两种通知模式，通过 `error_config_t::set_notify_mode()` 切换。
内部使用 **RCU（Read-Copy-Update）快照**机制，`notify_error()` 热路径零拷贝无锁读取插件列表。
异步模式基于 `async_queue_t` 模版类实现。

### 核心 API

```cpp
class plugin_registry_t {
public:
    static plugin_registry_t& instance() noexcept;

    void register_plugin(i_error_plugin_t* plugin) noexcept;
    void unregister_plugin(std::string_view name) noexcept;

    // 同步通知：RCU 快照无锁读取，依次调用每个插件的 on_error()
    void notify_error(const core::error_context_t& context) noexcept;

    // 异步入队（v2.0 新增）：将 error_context_t 副本推入 async_queue_t 后台队列
    // v2.3：底层使用 async_queue_t 模版类，支持背压控制
    void enqueue_notification(const core::error_context_t& context) noexcept;

    size_t size() const noexcept;
    bool empty() const noexcept;
    void clear() noexcept;

    // v2.0 新增：获取异步队列待处理通知数
    size_t pending_notifications() const noexcept;
};
```

### 通知模式对比

| 模式 | 行为 | 适用场景 |
|------|------|----------|
| `sync`（默认） | `error_context_t` 构造时同步调用所有插件 `on_error()`，RCU 快照零拷贝 | 日志、指标等轻量插件 |
| `async_queue` | 通知推入 `async_queue_t` 后台队列，独立工作线程异步消费 | 网络上报、持久化等可能阻塞的插件 |

### 异步模式内部机制

```
error_context_t 构造
  → notify_error(context)                     // 仅通知 min_level() <= context 等级的插件
  → enqueue_notification(context)             // 复制 context → shared_ptr
  → async_queue_.enqueue(copy)               // 推入 async_queue_t
  → cv.notify_one()                          // 唤醒工作线程

async_queue_t::__worker_loop()
  → wait(cv)                                  // 阻塞等待
  → pop 队列 → processor_(item)               // 调用 processor
  → plugin_registry_t::instance().notify_error(*context)  // 异步通知
```

- 工作线程首次 `enqueue()` 时自动启动，`async_queue_t` 析构时自动 join
- 支持 `set_max_size()` 背压控制，队列满时拒绝入队
- 处理器异常被捕获隔离，工作线程不回退出
- 工作线程内 `notify_error()` 调用仍为同步（依次调用插件），但已与调用方解耦

### 使用示例

```cpp
// 同步模式（默认）
auto& registry = plugin_registry_t::instance();
registry.register_plugin(logger.get());
registry.register_plugin(metrics.get());

// 构造 error_context_t 时会同步通知所有插件
error_context_t ctx(code, "错误信息");

// 切换为异步模式
error_config_t::set_notify_mode(error_config_t::notify_mode_t::async_queue);
// 此后 error_context_t 构造时通知被推入后台队列，不阻塞调用方

// 查看队列积压
size_t pending = registry.pending_notifications();

// 注销插件
registry.unregister_plugin(logger->name());
```

### 线程安全

- 同步模式：快照模式（snapshot-based），在共享读锁下复制插件列表，锁外依次执行回调
- 异步模式：队列操作受 `std::mutex` + `std::condition_variable` 保护，工作线程单线程消费
- `register_plugin()` / `unregister_plugin()`：独占写锁，与通知互斥

### 生命周期管理

`plugin_registry_t` **不持有插件所有权**，调用方负责对象生命周期：

```cpp
// 正确：插件生命周期由调用方管理
auto plugin = std::make_unique<my_plugin_t>();
registry.register_plugin(plugin.get());
// ... 使用 ...
registry.unregister_plugin(plugin->name());
// plugin 在此处安全销毁

// 错误：注册临时对象地址
registry.register_plugin(&temporary_plugin);  // 悬空指针！
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/plugin/plugin_registry_test.cc` | 13 | 注册、注销、通知、清空、并发安全 |

---

## error_router_plugin_t

错误路由插件，按错误码、系统域或模块组将错误分发到不同的处理函数。

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

### 路由优先级

当多个路由规则匹配时，按以下优先级处理：
1. 错误码精确匹配（最高优先级）
2. 模块组匹配
3. 系统域匹配（最低优先级）

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/plugin/error_router_plugin_test.cc` | 7 | 按码路由、按域路由、按模块组路由、优先级 |

---

## 插件开发指南

### 1. 最小插件实现

```cpp
class minimal_plugin_t : public i_error_plugin_t {
public:
    void on_error(const error_context_t& context) noexcept override {
        // 处理错误
    }
    std::string_view name() const noexcept override {
        return "minimal_plugin";
    }
};
```

### 2. 线程安全注意事项

`on_error()` 可能在多线程环境下被调用，插件实现必须保证线程安全：

```cpp
class thread_safe_plugin_t : public i_error_plugin_t {
    mutable std::mutex mutex_;
    std::atomic<size_t> error_count_{0};

public:
    void on_error(const error_context_t& context) noexcept override {
        ++error_count_;
        std::lock_guard<std::mutex> lock(mutex_);
        shared_data_.push_back(context.code.get_code());
    }
    std::string_view name() const noexcept override {
        return "thread_safe_plugin";
    }
};
```

### 3. 性能优化建议

- **同步模式**：避免在 `on_error()` 中执行耗时操作（如网络 I/O），插件回调不应阻塞错误处理流程
- **异步模式**：切换 `notify_mode` 为 `async_queue`，通知与调用方解耦，但插件 `on_error()` 仍在工作线程中同步执行
- **快速过滤**：重写 `min_level()` 或在 `on_error()` 开头根据错误等级或错误码快速返回，减少不必要处理

```cpp
class efficient_plugin_t : public i_error_plugin_t {
public:
    // v2.1：框架级过滤，低于 error 等级的 error_context_t 不会调用 on_error()
    core::error_level_t min_level() const noexcept override { return core::error_level_t::error; }

    void on_error(const error_context_t& context) noexcept override {
        // 异步上报
        report_queue_.enqueue(context.to_json());
    }
    std::string_view name() const noexcept override {
        return "efficient_plugin";
    }
};
```