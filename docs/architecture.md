# 🏗️ 架构设计

---

## 📐 分层架构

```
⚙️ Config  ── 全局配置 + per-code 覆盖 + 通知模式
  ↑
🧩 Core    ── error_code_t · error_context_t · result_t<T>
  ↑           error_registry_t · error_exception_t
🌐 Domain  ── 6 大系统域
🔌 Plugin  ── i_error_plugin_t · plugin_registry_t · error_router_plugin_t

🛠️ Utils ──── 被所有层使用
```

> 💡 **核心原则**：下层不依赖上层，`error_code_t` 不感知 Plugin。

---

## 📦 各层职责

### ⚙️ Config 层

| 类 | 职责 |
|------|------|
| `error_config_t` | 堆栈阈值、per-code 覆盖、验证开关、源位置、通知模式、格式化器 |

### 🧩 Core 层

| 类 | 职责 |
|------|------|
| `error_code_t` | 64 位错误码存储与字段解析，五参便捷构造 |
| `error_level_t` | 5 级错误等级枚举 |
| `error_builder_t` | 编译期构建工厂（v2.2 兼容层） |
| `error_context_t` | 错误上下文：码 + 消息 + 负载 + 因果链 + 堆栈 + 源位置 |
| `error_registry_t` | 单例注册表，按子系统/名称查询 |
| `result_t<T>` | 类 Rust Result，零异常 |
| `error_exception_t` | `std::exception` 封装 |

### 🔌 Plugin 层

| 类 | 职责 |
|------|------|
| `i_error_plugin_t` | 插件抽象接口 |
| `plugin_registry_t` | 单例注册表，RCU 快照无锁读取，`async_queue_t` 异步通知 |
| `error_router_plugin_t` | 按码/域/模块组路由分发 |

### 🛠️ Utils 层

| 类/工具 | 职责 |
|------|------|
| `async_queue_t` | 异步工作队列模板 |
| `string_utils_t` | 哈希、格式化、分割、修剪 |
| `json_dict_t` | JSON 解析与点路径访问 |
| `file_utils_t` | 跨平台文件操作 |
| `stack_trace_utils_t` | 跨平台堆栈跟踪 |
| `source_location_t` | 源位置封装 |
| `operator<<` | `error_context_t` 输出流 |

---

## 🎯 关键设计决策

### 1. 🔢 64 位位移 + 掩码

```cpp
static constexpr uint32_t LEVEL_SHIFT = 56;
static constexpr uint64_t LEVEL_MASK = 0xFULL;

constexpr error_level_t get_level() const noexcept {
    return static_cast<error_level_t>((code_ >> LEVEL_SHIFT) & LEVEL_MASK);
}
```

> ⚡ 单条指令完成字段提取，100% 避免位域未定义行为。

### 2. 🔥 constexpr 全链路

`error_code_t` 构造、`to_string()`、`from_string()`、`hash()` 均为 `constexpr`，编译期确定的值直接进入只读数据段。

### 3. 🔄 循环依赖打破

`plugin_registry.h` → `error_context.h`，若反向 include 则循环。

> 💡 **解法**：`error_context_t::__finalize_runtime_features()` 调用 `plugin_registry_t::instance()`，实现放在 `src/core/error_context.cc`，`.cc` 可安全 include `plugin_registry.h`。

### 4. 🧹 简化构造 API（v2.0）

移除 `code_with_location_t`，`error_context_t` 直接接受 `error_code_t`，源位置透明捕获。

### 5. 📎 多类型 payload

`with()` 模板版本通过 `if constexpr` 分派，支持 int/bool/double 等算术类型自动转换。

### 6. 🏰 全局单例注册表

- Meyer's singleton + `std::shared_mutex`
- 不持有插件所有权，调用方管理生命周期
- `notify_error()` 捕获每个插件的异常

### 7. 🔀 通知模式

| 模式 | 机制 | 场景 |
|------|------|------|
| ⚡ `sync` | `error_context_t` 构造时同步调用插件 | 日志、指标 |
| ⏳ `async_queue` | 通知推入 `async_queue_t`，工作线程异步消费 | 网络上报、持久化 |

### 8. 🎯 result_t\<T\> 零异常

- `std::get_if` + 静态哨兵值替代 `std::get`
- `value()` / `error()` / `expect()` / `unwrap()` 失败永不抛异常
- `map()` / `map_error()` 链式类型转换

### 9. 🔍 子系统索引（v2.1）

`subsystem_index_` 将 `get_errors_by_subsystem` 从 O(n) 优化为 O(1) 索引 + O(k) 遍历。

### 10. ⚡ RCU 快照（v2.3）

`plugin_registry_t` 用 `shared_ptr<const vector>` + `atomic_load/store` 实现无锁热路径。注册时持写锁深拷贝-修改-原子交换。

### 11. 💾 SSO 小负载（v2.3）

`error_context_t` 载荷使用 `array<pair<string,string>, 4>` 栈存储 + `unique_ptr<unordered_map>` 惰性溢出，≤4 项零堆分配。

### 12. ⏳ 异步队列模板

从 `plugin_registry_t` 抽离，支持死锁安全析构、背压控制、异常隔离、零 `std::function` 开销。

### 13. 🔍 自动堆栈 + Per-Code 覆盖

```
优先级: is_stacktrace_enabled → per-code 覆盖 → 全局阈值
```

### 14. 🤖 代码生成

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

---

## 🔧 编译配置

| CMake 选项 | 默认 | 说明 |
|------|:---:|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | ✅ ON | 堆栈追踪 + per-code API |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | ✅ ON | 错误码注册校验 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | ✅ ON | 源位置追踪 |
| `ERROR_SYSTEM_ENABLE_LTO` | OFF | LTO 优化 |
| `ERROR_SYSTEM_ENABLE_ASAN` | OFF | AddressSanitizer |
| `ERROR_SYSTEM_ENABLE_UBSAN` | OFF | UBSan |
| `ERROR_SYSTEM_WARNINGS_AS_ERRORS` | ✅ ON | Warning → Error |
| `BUILD_SHARED_LIBS` | OFF | 动态库构建 |

---

## 🧪 测试架构

- **框架**：GoogleTest
- **覆盖**：15 文件 · 271 用例 · Core/Plugin/Utils/Config/Domain 全模块
- **性能基线**：`tests/perf/` 下 3 个 benchmark

```bash
# 🧪 单元测试
cd build && ctest --output-on-failure

# ⚡ 性能测试
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DERROR_SYSTEM_BUILD_PERF_TESTS=ON
cmake --build build-perf -j
./build-perf/tests/perf/error_context_benchmark

# 🏭 生产构建
cmake -S . -B build-prod -DCMAKE_BUILD_TYPE=Release -DERROR_SYSTEM_ENABLE_LTO=ON
cmake --build build-prod -j
```