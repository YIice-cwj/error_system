# Plugin 层 API 参考

> 命名空间: `error_system::plugin`

---

## i_error_plugin_t

插件抽象接口，所有自定义插件必须继承此接口。

```cpp
class i_error_plugin_t {
public:
    virtual ~i_error_plugin_t() = default;

    virtual void on_error(const error_context_t& context) = 0;
    virtual std::string get_plugin_name() const = 0;
};
```

### 方法说明

| 方法 | 说明 |
|------|------|
| `on_error()` | 错误事件回调，每当 `error_context_t` 构造时触发 |
| `get_plugin_name()` | 返回插件名称，用于标识和调试 |

### 自定义插件示例

```cpp
class logging_plugin_t : public i_error_plugin_t {
public:
    void on_error(const error_context_t& context) override {
        std::cerr << "[LOG] " << context.to_string() << std::endl;
    }

    std::string get_plugin_name() const override {
        return "logging_plugin";
    }
};

class metrics_plugin_t : public i_error_plugin_t {
public:
    void on_error(const error_context_t& context) override {
        auto level = context.code.get_level();
        if (level == error_level_t::error || level == error_level_t::fatal) {
            // 上报监控指标
            metrics_collector::increment("error_count", {
                {"level", to_string(level)},
                {"domain", std::to_string(context.code.get_domain())}
            });
        }
    }

    std::string get_plugin_name() const override {
        return "metrics_plugin";
    }
};
```

---

## plugin_registry_t

插件单例注册表，负责管理所有插件的生命周期和错误事件广播。

### 核心 API

```cpp
class plugin_registry_t {
public:
    static plugin_registry_t& instance() noexcept;

    void register_plugin(i_error_plugin_t* plugin);
    void unregister_plugin(i_error_plugin_t* plugin);

    void notify_all(const error_context_t& context) const;

    size_t size() const noexcept;
    bool empty() const noexcept;
    void clear() noexcept;
};
```

### 线程安全

`plugin_registry_t` 使用 `std::shared_mutex` 保护插件列表：
- `register_plugin()` / `unregister_plugin()`：写锁
- `notify_all()`：读锁

### 使用示例

```cpp
using namespace error_system::plugin;

// 创建插件实例
auto logger = std::make_unique<logging_plugin_t>();
auto metrics = std::make_unique<metrics_plugin_t>();

// 注册到全局注册表
auto& registry = plugin_registry_t::instance();
registry.register_plugin(logger.get());
registry.register_plugin(metrics.get());

// 构造 error_context_t 时会自动通知所有插件
error_context_t ctx(code, "错误信息");
// logger->on_error(ctx) 和 metrics->on_error(ctx) 被自动调用

// 注销插件
registry.unregister_plugin(logger.get());

// 清空所有插件
registry.clear();
```

### 生命周期管理

`plugin_registry_t` **不持有插件所有权**，调用方负责对象生命周期：

```cpp
// 正确：插件生命周期由调用方管理
auto plugin = std::make_unique<my_plugin_t>();
registry.register_plugin(plugin.get());
// ... 使用 ...
registry.unregister_plugin(plugin.get());
// plugin 在此处安全销毁

// 错误：注册临时对象地址
registry.register_plugin(&temporary_plugin);  // 危险！临时对象销毁后悬空指针
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/plugin/plugin_registry_test.cc` | 10 | 注册、注销、通知、清空、线程安全 |

---

## error_router_plugin_t

错误路由插件，按错误码、系统域或模块组将错误分发到不同的处理函数。

```cpp
class error_router_plugin_t : public i_error_plugin_t {
public:
    using handler_t = std::function<void(const error_context_t&)>;

    void add_route_by_code(error_code_t code, handler_t handler);
    void add_route_by_domain(uint8_t domain, handler_t handler);
    void add_route_by_module_group(std::string_view group, handler_t handler);

    void on_error(const error_context_t& context) override;
    std::string get_plugin_name() const override;
};
```

### 使用示例

```cpp
using namespace error_system::plugin;

auto router = std::make_unique<error_router_plugin_t>();

// 按错误码路由
router->add_route_by_code(ERR_DB_CONNECTION_TIMEOUT, [](const error_context_t& ctx) {
    // 发送数据库告警
    alert_system::send("db_alert", ctx.to_string());
});

// 按系统域路由
router->add_route_by_domain(
    static_cast<uint8_t>(system_domain_t::database),
    [](const error_context_t& ctx) {
        // 记录数据库错误日志
        db_logger::error(ctx.to_json());
    }
);

// 按模块组路由
router->add_route_by_module_group("payment", [](const error_context_t& ctx) {
    // 上报支付模块错误
    payment_metrics::record_error(ctx);
});

// 注册到全局注册表
plugin_registry_t::instance().register_plugin(router.get());
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
    void on_error(const error_context_t& context) override {
        // 处理错误
    }

    std::string get_plugin_name() const override {
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
    void on_error(const error_context_t& context) override {
        ++error_count_;

        std::lock_guard<std::mutex> lock(mutex_);
        // 访问共享数据
        shared_data_.push_back(context.code.get_raw_code());
    }

    std::string get_plugin_name() const override {
        return "thread_safe_plugin";
    }
};
```

### 3. 性能优化建议

- **避免在 on_error() 中执行耗时操作**：插件回调不应阻塞错误处理流程
- **使用异步处理**：将耗时操作（如网络请求）放入异步队列
- **过滤不感兴趣的错误**：在 `on_error()` 开头快速返回，减少不必要的处理

```cpp
class efficient_plugin_t : public i_error_plugin_t {
public:
    void on_error(const error_context_t& context) override {
        // 快速过滤
        if (context.code.get_level() < error_level_t::error) {
            return;
        }

        // 异步处理
        async_queue_.enqueue(context.to_json());
    }

    std::string get_plugin_name() const override {
        return "efficient_plugin";
    }
};
```
