# 架构设计文档

本文档描述错误码系统的整体架构、模块职责划分及关键设计决策。

---

## 总体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        Error System                              │
├──────────────┬────────────────┬──────────────────────────────────┤
│  Config 层   │  Core 层       │  Plugin 层                       │
│  全局配置    │  核心数据结构  │  可扩展插件（同步/异步）         │
├──────────────┴────────────────┴──────────────────────────────────┤
│  Domain 层                                                       │
│  预定义的 6 大系统域                                             │
├──────────────────────────────────────────────────────────────────┤
│  Memory 层                                                       │
│  线程局部对象池 (object_pool_t)，优化高频内存分配                │
├──────────────────────────────────────────────────────────────────┤
│  Utils 层                                                        │
│  通用工具 (字符串、JSON、文件、堆栈跟踪、源位置等)               │
└──────────────────────────────────────────────────────────────────┘
```

### 依赖关系

```
Utils  ←─────────────────────────────── 被所有层使用
  ↑
Config (error_config_t)
  ↑
Core (error_code_t, error_level_t, error_context_t, error_registry_t)
  ↑                  ↑
Domain             Plugin 层监听（同步或异步）
```

> 核心原则：**下层不依赖上层**，`error_code_t` 不感知 Plugin。

---

## 层次说明

### Config 层 (`include/error_system/config/`)

| 文件 | 类/结构 | 职责 |
|------|---------|------|
| `error_config.h` | `error_config_t` | 全局错误配置（堆栈阈值、per-code 覆盖、验证开关、源位置追踪、通知模式、格式化器） |

### Core 层 (`include/error_system/core/`)

| 文件 | 类/结构 | 职责 |
|------|---------|------|
| `error_code.h` | `error_code_t` | 64 位错误码的存储与字段解析（含 v2.0 五参便捷构造函数） |
| `error_level.h` | `error_level_t` | 错误等级枚举及转换函数 |
| `error_builder.h` | `error_builder_t` | 编译期错误码构建工厂 |
| `error_context.h` | `error_context_t` | 错误上下文（直接接受 error_code_t，多类型 payload，因果链，堆栈，源位置） |
| `error_registry.h` | `error_registry_t` | 错误码注册表（按子系统/模块组索引，按名查找） |
| `result.h` | `result_t<T>` | 类 Rust Result（`make_error`/`map`/`map_error`/`expect`/`operator bool`，零异常） |
| `error_exception.h` | `error_exception_t` | 基于 `std::exception` 的异常封装 |

### Domain 层 (`include/error_system/domain/`)

预定义 6 大系统域：`none`, `system`, `middleware`, `database`, `application`, `third_party`。

### Plugin 层 (`include/error_system/plugin/`)

| 文件 | 类 | 职责 |
|------|----|------|
| `i_error_plugin.h` | `i_error_plugin_t` | 插件抽象接口 |
| `plugin_registry.h` | `plugin_registry_t` | 插件单例注册表（支持同步/异步通知模式） |
| `error_router_plugin.h` | `error_router_plugin_t` | 错误路由插件，按码/域/模块组分发 |

### Memory 层 (`include/error_system/memory/`)

| 文件 | 类 | 职责 |
|------|----|------|
| `object_pool.h` | `object_pool_t<T, Capacity>` | 线程局部对象池，减少高频堆分配 |

### Utils 层 (`include/error_system/utils/`)

| 文件 | 类/工具 | 职责 |
|------|---------|------|
| `string_utils.h` | `string_utils_t` | 哈希、格式化、分割、修剪等 |
| `json_utils.h` | `json_dict_t` | JSON 字典加载与点路径访问 |
| `file_utils.h` | `file_utils` | 文件读写、创建、删除、存在性检查 |
| `stack_trace_utils.h` | `stack_trace_utils_t` | 跨平台堆栈跟踪（Linux/macOS/Windows） |
| `source_location.h` | `source_location_t` | 源文件位置信息封装 |
| `error_formatter.h` | `operator<<` | `error_context_t` 输出流运算符重载 |

---

## 关键设计决策

### 1. 64 位位移 + 掩码实现零开销

```cpp
class error_code_t {
    code_t code_{1ULL << SIGN_SHIFT};  // 默认 sign=1（成功）

    static constexpr uint32_t LEVEL_SHIFT = 56;
    static constexpr uint64_t LEVEL_MASK = 0xFULL;

    constexpr error_level_t get_level() const noexcept {
        return static_cast<error_level_t>((code_ >> LEVEL_SHIFT) & LEVEL_MASK);
    }
};
```

直接通过位移和掩码操作读取字段，编译器可优化到单条指令，100% 避免严格别名规则与位域排布未定义行为。

### 2. `constexpr` 全链路

`error_code_t` 便捷构造函数、`to_string()`、`from_string()`、`hash()` 均为 `constexpr`，错误码可在编译期完全确定，进入只读数据段。

### 3. error_context_t 中的循环依赖打破

`plugin_registry.h` → `i_error_plugin.h` → `error_context.h`，若 `error_context.h` 直接 include `plugin_registry.h` 则循环。

**解法**：`error_context_t::finalize_runtime_features()` 直接调用 `plugin_registry_t::instance()`，该实现放在 `src/core/error_context.cc` 中，该 `.cc` 可安全 include `plugin_registry.h`。

```
error_context.h      ── 不 include plugin_registry.h ──→  头文件层无环
error_context.cc     ── include plugin_registry.h     ──→  仅在 .cc 连接
```

### 4. 简化构造 API（v2.0）

v2.0 移除 `code_with_location_t` 中间层，`error_context_t` 直接接受 `error_code_t`：

```cpp
// v1.x
error_context_t ctx(code_with_location_t{ERR_FAIL}, "失败");

// v2.0
error_context_t ctx(ERR_FAIL, "失败");
```

源位置捕获透明化：`source_location_` 变为私有成员，构造时自动通过 `source_location_t::current()` 捕获。

### 5. 多类型 payload（v2.0）

`error_context_t::with()` 支持模板版本，自动转换算术类型：

```cpp
ctx.with("retry_count", 3)        // int → "3"
   .with("is_retryable", false)   // bool → "false"
   .with("latency_ms", 150.5);    // double → "150.500000"
```

内部通过 `if constexpr` 分派，避免 SFINAE 模板重定义问题。

### 6. 全局单例注册表模式

- **不持有所有权**：调用方负责对象生命周期
- **Meyer's singleton**：`static` 局部变量，C++11 保证线程安全的初始化
- **线程安全访问**：使用 `std::shared_mutex` 保护
- **异常安全**：`notify_error()` 捕获每个插件的异常

### 7. 可配置通知模式（v2.0）

```cpp
// 同步模式（默认）：error_context_t 构造时同步调用插件
error_config_t::set_notify_mode(error_config_t::notify_mode_t::sync);

// 异步队列模式：通知推入后台队列，独立工作线程消费
error_config_t::set_notify_mode(error_config_t::notify_mode_t::async_queue);
```

异步模式内部机制：
- 首次 `enqueue_notification()` 调用时自动启动后台工作线程
- `plugin_registry_t` 析构时自动停止线程，等待队列排空
- 通过 `pending_notifications()` 查询队列积压

### 8. result_t\<T\> 零异常错误传递

```cpp
// v2.0 推荐使用工厂函数，语义清晰，避免构造函数重载混淆
return result_t<Data>::make_error(ERR_FAIL, "原因");

// expect(): Debug 模式下断言，Release 返回哨兵值
int value = result.expect("should never fail here");

// value_pointer(): 安全获取（失败返回 nullptr）
if (auto* ptr = result.value_pointer()) { /* 安全使用 */ }

// value() / error(): 使用 std::get_if 内部实现，误调用返回静态哨兵值，永不抛异常

// map() / map_error(): 类型转换链式操作
auto str_result = result.map([](int v) { return std::to_string(v); });

// 链式操作
auto final = fetch()
    .and_then([](auto& d) { return process(d); })
    .or_else([](const auto& err) {
        log(err);
        return result_t<Data>::make_error(ERR_RECOVER, "已恢复");
    });
```

### 9. 子系统索引优化（v2.1）

`error_registry_t` 新增 `subsystem_index_`（`unordered_map<uint16_t, vector<module_group_id_t>>`），将 `get_errors_by_subsystem` 从 O(n) 遍历全部模块组优化为 O(1) 索引查找 + O(k) 遍历该子系统模块组。

索引维护：
- **register_error**: 首次注册某子系统下的模块组时，将该 `module_group_id` 加入索引
- **unregister_error**: 模块组内全部错误码注销后，自动级联清理 `subsystem_index_`
- **unregister_module**: 直接移除 `subsystem_index_` 中对应条目
- **unregister_all**: 一并清空所有索引

### 10. JSON 与二进制序列化（v2.1 增强）

- **`to_json()`**: `code` 字段使用 `identity_code`（与 `to_string()` 一致），含因果链递归输出
- **`to_binary()`**: 紧凑二进制格式，末尾新增 `has_cause` 标志 + 因果链递归序列化

### 11. 对象池优化因果链

`error_context_t::wrap()` 使用线程局部对象池分配 cause 节点，仅在池满时才回退到 `std::make_shared`，显著降低高频错误链场景下的堆分配压力。

### 12. 源位置追踪

`error_context_t` 构造时自动将 `source_location_t::current()` 捕获到 `source_location_` 私有成员中，`fill_source_location()` 从中提取文件名、函数名和行号。支持完整路径和短文件名两种模式。

### 13. 自动堆栈跟踪 + Per-Code 覆盖（v2.0）

```
优先级链: is_stacktrace_enabled → per-code 覆盖 → 全局阈值
```

```cpp
// 全局：WARN 及以上触发
error_config_t::set_stacktrace_level(error_level_t::warn);

// Per-code 覆盖：ERR_CRITICAL 总是捕获（含 DEBUG）
error_config_t::set_per_code_stacktrace_level(
    ERR_CRITICAL.get_identity_code(), error_level_t::debug);

// Per-code 覆盖：ERR_RATE_LIMIT 永不捕获
error_config_t::set_per_code_stacktrace_level(
    ERR_RATE_LIMIT.get_identity_code(), error_level_t::fatal);
```

### 14. 错误码验证

若 `is_validation_enabled()` 为 `true` 且错误码未在 `error_registry_t` 中注册，则 `error_context_t` 构造时自动将错误码替换为 `fatal` 级别的未注册错误码。

### 15. 全局错误配置

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `min_stacktrace_level_` | `error` | 自动捕获堆栈的最低等级 |
| `per_code_stacktrace_map_` | 空 | per-code 堆栈等级覆盖（优先级 > 全局） |
| `enable_stacktrace_` | `true` | 堆栈追踪总开关 |
| `enable_validation_` | `true` | 错误码验证开关 |
| `enable_source_location_` | `true` | 源位置追踪开关 |
| `enable_short_filename_` | `true` | 短文件名模式 |
| `enable_text_output_` | `true` | 文本输出模式（false 时输出数字 ID） |
| `notify_mode_` | `sync` | 插件通知模式 |
| `custom_formatter_` | `nullptr` | 自定义格式化回调 |

### 16. DEFINE_ERROR_CODE 宏（v2.0 扩展）

```cpp
// v2.0 签名：新增子系统/模块名称参数
DEFINE_ERROR_CODE(
    ERR_DB_CONNECTION_TIMEOUT,
    error_level_t::error, system_domain_t::database,
    1, 1, 0x0001,
    "数据库连接超时",
    "数据库服务",   // 子系统名称 → to_string() 可读输出
    "连接管理"       // 模块名称     → to_string() 可读输出
);
// to_string() 输出: [ERROR][数据库服务][连接管理][1] 数据库连接超时
```

### 17. 查询 API 简化（v2.0）

| 方法 | v1.x | v2.0 |
|------|------|------|
| `get_info()` | `optional<reference_wrapper<metadata>>` | `const metadata_t*`（nullptr = 未注册） |
| `get_errors_by_subsystem()` | 不存在 | 按子系统 ID 查询所有错误码（v2.1 引入 subsystem_index_ O(1) 索引） |
| `find_by_name()` | 不存在 | 按名称查找错误码 |

### 18. error_code_t 便捷构造函数（v2.0）

```cpp
// 替代 error_builder_t::make_error_code()
error_code_t code(error_level_t::error, system_domain_t::database, 1, 2, 0x0010);
```
`constexpr` 支持编译期常量构造，零运行时开销。

### 19. 代码生成工具

```
script/script_py/
├── generate_error_codes.py    # 单个 JSON → C++ 头文件
├── generate_error_dict.py     # 全局字典 + ID 冲突检测
├── generate_error_docs.py     # Markdown 文档
└── generate_all.py            # v2.0 统一入口（一键调用以上三个）
```

```bash
# 统一生成（推荐）
python script/script_py/generate_all.py

# 或通过 shell 脚本
bash script/generate_errors.sh
```

JSON 配置格式示例：

```json
{
    "namespace": "biz::trade_errors",
    "service_name": "交易服务",
    "domain": "application",
    "subsystem_id": 101,
    "modules": {
        "order": { "id": 1, "desc": "订单模块" }
    },
    "errors": [
        {
            "name": "ERR_ORDER_NOT_FOUND",
            "module": "order",
            "level": "error",
            "number": 1,
            "desc": "订单不存在或已删除"
        }
    ]
}
```

> v2.0 新增：`generate_error_dict.py` 在生成前自动检测 `(subsystem_id, module_id, number)` 三元组全局唯一性，冲突时打印详细信息并 exit(1)。

---

## 编译期配置选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | `ON` | 启用堆栈追踪 + per-code API |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | `ON` | 启用错误码验证 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | `ON` | 启用源位置追踪 |
| `ERROR_SYSTEM_ENABLE_LTO` | `OFF` | 启用链接时优化（IPO/LTO） |
| `ERROR_SYSTEM_ENABLE_PGO_GENERATE` | `OFF` | 启用 PGO 插桩阶段 |
| `ERROR_SYSTEM_ENABLE_PGO_USE` | `OFF` | 启用 PGO 优化阶段 |
| `ERROR_SYSTEM_ENABLE_ASAN` | `OFF` | 启用 AddressSanitizer |
| `ERROR_SYSTEM_ENABLE_UBSAN` | `OFF` | 启用 UndefinedBehaviorSanitizer |
| `ERROR_SYSTEM_WARNINGS_AS_ERRORS` | `ON` | Warning 视为错误 |
| `BUILD_SHARED_LIBS` | `OFF` | 构建为动态库 |

---

## 测试架构

### 测试框架

- **测试框架**: GoogleTest
- **测试文件数**: 16 个
- **测试覆盖模块**: Core, Plugin, Memory, Utils, Config, Domain

### 测试覆盖

| 模块 | 测试文件数 | 测试用例数 | 测试文件 |
|------|-----------|-----------|----------|
| Core 层 | 7 | 100+ | `tests/core/*_test.cc` |
| Plugin 层 | 2 | 20 | `tests/plugin/*_test.cc` |
| Memory 层 | 1 | 10 | `tests/memory/*_test.cc` |
| Utils 层 | 4 | 37+ | `tests/utils/*_test.cc` |
| Config 层 | 1 | 11 | `tests/config/*_test.cc` |
| Domain 层 | 1 | 3 | `tests/domain/*_test.cc` |
| **总计** | **16** | **247** | - |

### 运行测试

```bash
cd build
ctest --output-on-failure
```

或运行特定测试：

```bash
./tests/core/error_code_test
./tests/core/result_test
```

### 性能基线测试

```bash
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DERROR_SYSTEM_BUILD_PERF_TESTS=ON
cmake --build build-perf -j
./build-perf/tests/perf/error_context_benchmark
./build-perf/tests/perf/error_registry_benchmark
./build-perf/tests/perf/plugin_registry_benchmark
```

### 推荐生产构建

```bash
cmake -S . -B build-prod \
  -DCMAKE_BUILD_TYPE=Release \
  -DERROR_SYSTEM_ENABLE_LTO=ON
cmake --build build-prod -j
```