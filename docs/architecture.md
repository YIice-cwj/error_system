# 架构设计

---

## 分层架构

```
Config   ── 全局配置 + per-code 覆盖 + 通知模式
            feature_flags_t · stacktrace_config_t · formatter_config_t
  ↑
Core     ── error_code_t · error_context_t · result_t<T>
            error_context_serializer_t · error_context_initializer_t
            error_registry_t · error_builder_t · error_exception_t
  ↑
Domain   ── 6 大系统域
Plugin   ── i_error_plugin_t · plugin_registry_t · error_router_plugin_t
            async_notification_channel_t

Utils    ── 被所有层使用
            async_queue_t · string_utils_t · string_format_t
            json_dict_t · json_lexer_t · file_utils_t
            stack_trace_utils_t · source_location_t
```

> **核心原则**：下层不依赖上层，`error_code_t` 不感知 Plugin。

---

## 各层职责

### Config 层

原 `error_config_t` 已按 SRP 拆分为三个独立配置类：

| 类 | 职责 |
|------|------|
| `feature_flags_t` | 编译期特性开关 + 运行时布尔标志位（堆栈/验证/位置/短文件名/文本输出/通知模式） |
| `stacktrace_config_t` | 堆栈追踪全局阈值 + per-code 覆盖配置 |
| `formatter_config_t` | 自定义格式化器配置（线程安全 set/get） |

### Core 层

原 `error_context_t` 已按 SRP 拆分为三个组件：

| 类 | 职责 |
|------|------|
| `error_code_t` | 64 位错误码存储与字段解析，五参便捷构造 |
| `error_level_t` | 5 级错误等级枚举 |
| `error_builder_t` | 编译期构建工厂（v2.2 兼容层） |
| `error_context_t` | 错误上下文数据类：码 + 消息 + 负载 + 因果链 + 堆栈 + 源位置 |
| `error_context_serializer_t` | 序列化器：to_string / to_json / to_binary |
| `error_context_initializer_t` | 初始化器：错误码校验、堆栈捕获、插件通知 |
| `error_registry_t` | 单例注册表，按子系统/名称查询 |
| `result_t<T>` | 类 Rust Result，零异常错误传递 |
| `error_exception_t` | `std::exception` 封装 |

### Plugin 层

| 类 | 职责 |
|------|------|
| `i_error_plugin_t` | 插件抽象接口 |
| `plugin_registry_t` | 单例注册表，RCU 快照无锁读取，`async_queue_t` 异步通知 |
| `async_notification_channel_t` | 异步通知通道（封装 async_queue_t，注入回调打破循环依赖） |
| `error_router_plugin_t` | 按码/域/模块组路由分发 |

### Utils 层

| 类/工具 | 职责 |
|------|------|
| `async_queue_t` | 异步工作队列模板（单生产者-单消费者） |
| `string_utils_t` | 哈希、格式化、分割、修剪、JSON 转义 |
| `string_format_t` | `{}` 占位符格式化 |
| `json_dict_t` | JSON 解析与点路径访问 |
| `json_lexer_t` | JSON 词法分析器（支持 UTF-16 代理对） |
| `file_utils_t` | 跨平台文件操作（含 OOM 防护） |
| `stack_trace_utils_t` | 跨平台堆栈跟踪 |
| `source_location_t` | 源位置封装 |
| `operator<<` | `error_context_t` 输出流 |

---

## 关键设计决策

### 1. 64 位位移 + 掩码

```cpp
static constexpr uint32_t LEVEL_SHIFT = 56;
static constexpr uint64_t LEVEL_MASK = 0xFULL;

constexpr error_level_t get_level() const noexcept {
    return static_cast<error_level_t>((code_ >> LEVEL_SHIFT) & LEVEL_MASK);
}
```

> 单条指令完成字段提取，100% 避免位域未定义行为。

### 2. constexpr 全链路

`error_code_t` 构造、`to_string()`、`from_string()`、`hash()` 均为 `constexpr`，编译期确定的值直接进入只读数据段。

### 3. 循环依赖打破

存在两条潜在循环依赖：

**循环 1**：`plugin_registry.h` → `error_context.h`，若反向 include 则循环。
- **解法**：`error_context_initializer_t::initialize()` 调用 `plugin_registry_t::instance()`，实现放在 `src/core/error_context_initializer.cc`，`.cc` 可安全 include `plugin_registry.h`。

**循环 2**：`async_notification_channel_t` 需要调用 `plugin_registry_t::notify_error()`，但反向又会被 registry 持有。
- **解法**：通过构造时注入 `notify_callback_t` 回调，channel 不感知具体 registry 实现，遵循依赖倒置原则。

### 4. 简化构造 API（v2.0）

移除 `code_with_location_t`，引入 `located_code_t` 通过隐式转换从 `error_code_t` 构造时自动捕获调用者位置。`source_location_t::current()` 作为默认参数在调用点展开 `__builtin_FILE()`，捕获的是调用者位置而非库内部位置。

### 5. 多类型 payload

`with()` 模板版本通过 `if constexpr` 分派，支持 int/bool/double 等算术类型自动转换。

### 6. 全局单例注册表

- Meyer's singleton + `std::shared_mutex`
- `register_plugin(unique_ptr)` 接管所有权，`register_plugin_ref()` 非持有引用
- `notify_error()` 捕获每个插件的异常，回调在锁外执行

### 7. 通知模式

| 模式 | 机制 | 场景 |
|------|------|------|
| `sync` | `error_context_t` 构造时同步调用插件 | 日志、指标 |
| `async_queue` | 通知推入 `async_queue_t`，工作线程异步消费 | 网络上报、持久化 |

### 8. result_t\<T\> 零异常

- `std::variant<T, error_context_t>` + `std::get_if` + `thread_local` 哨兵值替代 `std::get`，防止跨线程污染
- `value()` / `error()` / `expect()` / `unwrap()` / `unwrap_or()` 失败永不抛异常
- `map()` / `map_error()` / `and_then()` / `or_else()` 全部 noexcept，try-catch 保护用户函数异常并转为 fatal 错误
- `match()` 不标记 noexcept：用户回调异常会传播给调用方（避免吞异常）
- 移动构造的 noexcept 性跟随 T：`noexcept(std::is_nothrow_move_constructible_v<T>)`

### 9. 子系统索引（v2.1）

`subsystem_index_` 将 `get_errors_by_subsystem` 从 O(n) 优化为 O(1) 索引 + O(k) 遍历。

### 10. RCU 快照（v2.3）

`plugin_registry_t` 用 `shared_ptr<const vector>` + `atomic_load/store` 实现无锁热路径。注册时持写锁深拷贝-修改-原子交换。读者持有 `shared_ptr` 共享所有权，避免 unregister 销毁插件后调用 `on_error` 触发 use-after-free。

### 11. SSO 小负载（v2.3）

`error_context_t` 载荷使用 `array<pair<string,string>, 4>` 栈存储 + `unique_ptr<unordered_map>` 惰性溢出，≤4 项零堆分配。`payload_count_` 仅计 SSO 部分，溢出后归零。

### 12. 异步队列模板

从 `plugin_registry_t` 抽离为 `async_queue_t<T, Processor>`，支持死锁安全析构、背压控制（`set_max_size` 加锁保护）、异常隔离、零 `std::function` 开销。

### 13. 自动堆栈 + Per-Code 覆盖

```
优先级: is_stacktrace_enabled → per-code 覆盖 → 全局阈值
```

### 14. 安全性设计

| 风险 | 防护措施 |
|------|------|
| `std::bad_alloc` | 所有 `noexcept` 函数内部 `try-catch` 捕获并记录到 stderr |
| 跨线程 result 哨兵污染 | `thread_local` 哨兵值，每线程独立 |
| RCU 快照 use-after-free | `shared_ptr` 共享所有权，读者持有插件引用 |
| 异步队列满 OOM | 背压控制 `set_max_queue_size` |
| 持锁调用用户回调死锁 | 复制回调指针 → 释放锁 → 调用 `on_error()` |
| 文件读取 OOM 攻击 | `MAX_READ_FILE_SIZE` 上限保护 |
| JSON 解析器孤立代理区码点 | `append_utf8()` 静默丢弃 0xD800-0xDFFF 范围 |
| 移动后源对象状态不一致 | 显式清零 `payload_count_` / `file_name` |
| `error_metadata_t` unregister 后悬垂 | `error_registry_t::get_info()` 返回值深拷贝 |

### 15. SRP 拆分（v2.3）

按单一职责原则将过大的类拆分：

| 原类 | 拆分后 |
|------|------|
| `error_config_t` (9 个不相关配置) | `feature_flags_t` + `stacktrace_config_t` + `formatter_config_t` |
| `error_context_t` (God Struct，7+ 职责) | `error_context_t` (数据) + `error_context_serializer_t` (序列化) + `error_context_initializer_t` (初始化) |
| `string_utils_t` (混合字符串/格式化/JSON) | `string_utils_t` + `string_format_t` + `json_utils_t` + `json_lexer_t` |
| `error_registry_t` (注册+子系统映射+重复处理) | `error_registry_t` + `subsystem_module_registry_t` + `duplicate_policy_handler_t` |
| `plugin_registry_t` (注册+异步队列泄露到公共接口) | `plugin_registry_t` + `async_notification_channel_t` |

### 16. 代码生成

```
script/script_py/
├── generate_error_codes.py    # JSON → C++ 头文件
├── generate_error_dict.py     # 全局字典 + ID 冲突检测
├── generate_error_docs.py     # Markdown 文档
└── generate_all.py            # 统一入口
```

```json
// config/errors/trade_service_errors.json
{
    "namespace": "biz::trade_errors",
    "service_name": "交易服务",
    "domain": "application",
    "subsystem_id": 101,
    "modules": { "order": { "id": 1, "desc": "订单模块" } },
    "errors": [
        { "name": "ERR_ORDER_NOT_FOUND", "module": "order",
          "level": "error", "number": 1, "desc": "订单不存在" }
    ]
}
```

详见 [错误码自动生成指南](error_code_generation.md)。

---

## 编译配置

| CMake 选项 | 默认 | 说明 |
|------|:---:|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | ON | 堆栈追踪 + per-code API |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | ON | 错误码注册校验 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | ON | 源位置追踪 |
| `ERROR_SYSTEM_ENABLE_LTO` | OFF | LTO 优化 |
| `ERROR_SYSTEM_ENABLE_ASAN` | OFF | AddressSanitizer |
| `ERROR_SYSTEM_ENABLE_UBSAN` | OFF | UBSan |
| `ERROR_SYSTEM_WARNINGS_AS_ERRORS` | ON | Warning → Error |
| `BUILD_SHARED_LIBS` | OFF | 动态库构建 |

---

## 测试架构

- **框架**：GoogleTest
- **覆盖**：19 文件 · 294 用例 · Core/Plugin/Utils/Config/Domain 全模块
- **性能基线**：`tests/perf/` 下 3 个 benchmark

```bash
# 单元测试
cd build && ctest --output-on-failure

# 性能测试
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DERROR_SYSTEM_BUILD_PERF_TESTS=ON
cmake --build build-perf -j
./build-perf/tests/perf/error_context_benchmark

# 生产构建
cmake -S . -B build-prod -DCMAKE_BUILD_TYPE=Release -DERROR_SYSTEM_ENABLE_LTO=ON
cmake --build build-prod -j
```

### 测试覆盖要点

| 模块 | 关键测试 |
|------|------|
| `error_context_t` | SSO 溢出、因果链多层嵌套、payload 遍历、wrap 移动语义、序列化包含 cause |
| `result_t<T>` | move noexcept 传播、match 异常传播、and_then/or_else 链式、void 特化 |
| `json_lexer_t` | UTF-16 代理对（高+低组合）、孤立代理丢弃、非法 hex 拒绝 |
| `file_utils_t` | 文件大小上限拒绝、边界值接受 |
| `plugin_registry_t` | RCU 快照、异步队列背压、级别过滤 |
