# Plugin 层 API 文档

> 命名空间：`error_system::plugin`

Plugin 层提供可扩展的插件系统，允许在不修改核心代码的前提下接入日志、统计、告警等外部能力。

---

## 工作原理

每当 `error_context_t` 以非零错误码构造时，核心层会自动调用 `plugin_registry_t::instance().notify_error()`，将错误上下文广播给所有已注册的插件。

```
error_context_t 构造（code != 0）
        ↓
 __notify_plugins()          ← 打破循环依赖的桥接函数
        ↓
 plugin_registry_t::notify_error()
        ↓
 i_error_plugin_t::on_error()  × N  ← 依次通知每个插件
```

> **注意**：插件收到的 `error_context_t` 可能包含 `stack_frames`（如果错误等级满足 `error_config::get_stacktrace_level()` 阈值且堆栈追踪功能已开启），插件可以利用堆栈信息进行更详细的日志记录或监控。

---

## i_error_plugin_t

**头文件**：`error_system/plugin/i_error_plugin.h`

所有插件必须继承的抽象接口。

```cpp
class i_error_plugin_t {
public:
    virtual ~i_error_plugin_t() = default;

    // 插件唯一标识，同名插件注册时会替换旧插件
    virtual std::string_view name() const noexcept = 0;

    // 错误事件回调，error_context_t 创建时触发
    // context 包含完整的错误信息：code, message, payload, stack_frames, cause chain
    virtual void on_error(const core::error_context_t& context) noexcept = 0;
};
```

---

## plugin_registry_t

**头文件**：`error_system/plugin/plugin_registry.h`

插件单例注册表，管理多个插件的生命周期绑定与事件广播。使用 `std::shared_mutex` 保证线程安全的并发访问。

> **注意**：不持有插件所有权，调用方需保证插件对象在使用期间始终有效。

### 方法

```cpp
// 获取单例
static plugin_registry_t& instance() noexcept;

// 注册插件（同名则替换，nullptr 无效）
void register_plugin(i_error_plugin_t* plugin) noexcept;

// 注销插件（按名称查找，未找到则无操作）
void unregister_plugin(std::string_view name) noexcept;

// 通知所有插件（内部调用，每个插件的异常不会向外传播）
void notify_error(const core::error_context_t& context) noexcept;

// 获取插件数量
size_t size() const noexcept;

// 是否没有插件
bool empty() const noexcept;

// 清空所有已注册插件
void clear() noexcept;
```

---

## 实现日志插件示例

```cpp
#include "error_system/plugin/i_error_plugin.h"
#include "error_system/plugin/plugin_registry.h"
#include <iostream>

class log_plugin_t : public error_system::plugin::i_error_plugin_t {
public:
    std::string_view name() const noexcept override {
        return "logger";
    }

    void on_error(const error_system::core::error_context_t& ctx) noexcept override {
        std::cerr << "[LOG] " << ctx.to_string() << "\n";
    }
};
```

## 实现统计插件示例

```cpp
#include "error_system/plugin/i_error_plugin.h"
#include <unordered_map>
#include <atomic>

class stats_plugin_t : public error_system::plugin::i_error_plugin_t {
    std::unordered_map<uint64_t, std::atomic<int>> counters_;

public:
    std::string_view name() const noexcept override {
        return "stats";
    }

    void on_error(const error_system::core::error_context_t& ctx) noexcept override {
        ++counters_[ctx.code.get_code()];
    }

    int count(uint64_t code) const noexcept {
        auto it = counters_.find(code);
        return it != counters_.end() ? it->second.load() : 0;
    }
};
```

## 实现带堆栈分析的监控插件示例

```cpp
#include "error_system/plugin/i_error_plugin.h"
#include <iostream>

class stacktrace_plugin_t : public error_system::plugin::i_error_plugin_t {
public:
    std::string_view name() const noexcept override {
        return "stacktrace_analyzer";
    }

    void on_error(const error_system::core::error_context_t& ctx) noexcept override {
        // 只处理包含堆栈的严重错误
        if (ctx.code.get_level() >= error_system::core::error_level_t::error &&
            !ctx.stack_frames.empty()) {
            std::cerr << "[ALERT] 严重错误: " << ctx.message << "\n";
            std::cerr << "[STACK] 调用栈:\n";
            for (const auto& frame : ctx.stack_frames) {
                std::cerr << "  " << frame << "\n";
            }
        }
    }
};
```

## 注册与使用

```cpp
// 程序启动时初始化（保证对象生命周期覆盖整个运行期）
log_plugin_t  logger;
stats_plugin_t stats;
stacktrace_plugin_t analyzer;

auto& reg = plugin_registry_t::instance();
reg.register_plugin(&logger);
reg.register_plugin(&stats);
reg.register_plugin(&analyzer);

// 之后任何 error_context_t 创建都会自动触发
error_context_t ctx{some_error_code, "操作失败"};
// → logger 打印日志
// → stats 计数 +1
// → analyzer 分析堆栈（如果包含）

// 动态注销
reg.unregister_plugin("logger");

// 清空所有插件
reg.clear();
```

---

## error_router_plugin_t

**头文件**：`error_system/plugin/error_router_plugin.h`

错误路由插件，提供按错误码、模块组 ID 或系统域路由错误事件的能力。继承自 `i_error_plugin_t`，使用单例模式管理。

### 处理优先级

当错误事件发生时，按以下优先级匹配处理函数：
1. **特定错误码处理函数** - 精确匹配错误码
2. **模块组处理函数** - 匹配错误码所属的模块组
3. **系统域处理函数** - 匹配错误码所属的系统域

### 方法

```cpp
// 获取单例
static error_router_plugin_t& instance() noexcept;

// 按错误码注册处理函数
void register_handler_by_code(core::code_t code, error_handler_t handler) noexcept;

// 按模块组 ID 注册处理函数
void register_handler_by_module_group_id(core::module_group_id_t module_group_id,
                                         error_handler_t handler) noexcept;

// 按系统域注册处理函数
void register_handler_by_domain(domain::system_domain_t domain, error_handler_t handler) noexcept;

// 注销处理函数
void unregister_handler_by_code(core::code_t code) noexcept;
void unregister_handler_by_module_group_id(core::module_group_id_t module_group_id) noexcept;
void unregister_handler_by_domain(domain::system_domain_t domain) noexcept;
```

### 使用示例

```cpp
#include "error_system/plugin/error_router_plugin.h"
#include <iostream>

using namespace error_system::plugin;

// 注册特定错误码的处理函数
error_router_plugin_t::instance().register_handler_by_code(
    some_error_code,
    [](const core::error_context_t& ctx) {
        std::cerr << "特定错误: " << ctx.message << "\n";
    }
);

// 按系统域注册处理函数
error_router_plugin_t::instance().register_handler_by_domain(
    domain::system_domain_t::database,
    [](const core::error_context_t& ctx) {
        std::cerr << "数据库错误: " << ctx.to_string() << "\n";
    }
);

// 将路由插件注册到全局插件注册表
plugin_registry_t::instance().register_plugin(&error_router_plugin_t::instance());

// 之后创建的错误上下文会自动路由到对应的处理函数
error_context_t ctx{db_error_code, "连接超时"};  // 触发数据库域处理函数
```

---

## 设计说明：循环依赖的打破

`plugin_registry.h` 依赖 `i_error_plugin.h`，后者依赖 `error_context.h`。若 `error_context.h` 直接 include `plugin_registry.h` 则产生循环。

解决方案：在 `error_context.h` 内仅声明自由函数 `__notify_plugins()`，其实现放在 `src/core/error_context.cc` 中，该 `.cc` 文件可以安全地 include `plugin_registry.h`：

```
error_context.h   ←  i_error_plugin.h  ←  error_context.h  ✗ 循环
     ↓ 声明 __notify_plugins()
error_context.cc  →  plugin_registry.h  →  i_error_plugin.h  ✓ 无环
```
