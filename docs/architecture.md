# 架构设计

---

## 🏛️ 分层架构

```
Config   ── 全局配置 + per-code 覆盖 + 通知模式 + i18n
            feature_flags_t · stacktrace_config_t · formatter_config_t · i18n_config_t
  ↑
i18n/Mapping/Migration
            ── i18n_t · status_mapper_t · error_migration_registry_t
  ↑
Core     ── error_code_t · error_context_t · result_t<T>
            error_context_serializer_t · error_context_initializer_t
            error_registry_t · error_builder_t · error_exception_t
  ↑
Domain   ── 6 大系统域
Plugin   ── i_error_plugin_t · plugin_registry_t · error_router_plugin_t
            async_notification_channel_t · error_dedup_sampler_t

Utils    ── 被所有层使用
            async_queue_t · string_utils_t · string_format_t
            json_dict_t · json_lexer_t · file_utils_t
            stack_trace_utils_t · source_location_t
```

> 💡 **核心原则**：下层不依赖上层，`error_code_t` 不感知 Plugin。

---

## 📋 各层职责

### ⚙️ Config 层

原 `error_config_t` 按 SRP 拆分：

| 📦 类 | 🎯 职责 |
|------|------|
| `feature_flags_t` | 编译期特性开关 + 运行时布尔标志位（堆栈/验证/位置/短文件名/文本输出/通知模式） |
| `stacktrace_config_t` | 堆栈追踪全局阈值 + per-code 覆盖配置 |
| `formatter_config_t` | 自定义格式化器配置（线程安全 set/get） |
| `i18n_config_t` | i18n 启用开关 + 输出/默认 locale 配置（atomic 无锁） |

### 🧩 Core 层

原 `error_context_t` 按 SRP 拆分：

| 📦 类 | 🎯 职责 |
|------|------|
| `error_code_t` | 64 位错误码存储与字段解析，五参便捷构造，含 retryable/transient 语义位 |
| `error_level_t` | 5 级错误等级枚举 |
| `error_builder_t` | 编译期构建工厂：枚举模板版本（防 ID 传反）+ `from_raw`（反序列化语义） |
| `error_context_t` | 错误上下文数据类：码 + 消息 + 负载 + 因果链 + 堆栈 + 源位置 |
| `error_context_serializer_t` | 序列化器：`to_string` / `to_json` / `to_binary` + `from_binary` / `from_json` |
| `error_context_initializer_t` | 初始化器：错误码校验、堆栈捕获、插件通知（sync / async_queue / sync_deferred 三模式分派） |
| `error_registry_t` | 单例注册表，按子系统/名称查询，纪元驱动的线程本地元数据缓存；委托 `subsystem_module_catalog_t` 与 `duplicate_policy_handler_t` |
| `result_t<T>` | 类 Rust Result，零异常错误传递；`operator=(&&)` 已实现允许变量复用 |
| `error_exception_t` | `std::exception` 封装，构造时缓存 `to_string` 结果 |

### 🌐 i18n 层

| 📦 类 | 🎯 职责 |
|------|------|
| `i18n_t` | 多语言消息目录单例，locale 回退查询 |
| `locale_t` | 语言区域强类型枚举（15 种预置语言），含 `to_string` / `from_string` / `is_valid` |
| `subsystem_module_catalog_t` | 子系统/模块名称多语言目录，`(subsystem_id, module_id) → (locale → 名称)` 两级映射 |

### 🔁 Mapping 层

| 📦 类 | 🎯 职责 |
|------|------|
| `status_mapper_t` | 错误码 → HTTP / gRPC 状态码映射（纯函数 constexpr，retryable/transient 优先映射 503/UNAVAILABLE） |
| `http_status_t` | HTTP 状态码类（含 `to_string` / `from_int` / `is_valid` / 分类判定） |
| `grpc_status_t` | gRPC 状态码类（含 `to_string` / `from_int` / `is_valid`） |

### 🔀 Migration 层

| 📦 类 | 🎯 职责 |
|------|------|
| `error_migration_registry_t` | 错误码废弃与迁移注册器单例，单跳 / 递归迁移 + 环检测（最大深度 16） |

### 🔌 Plugin 层

| 📦 类 | 🎯 职责 |
|------|------|
| `i_error_plugin_t` | 插件抽象接口（`name` / `min_level` / `on_error`） |
| `plugin_registry_t` | 单例注册表，RCU 快照无锁读取，sync / async_queue / sync_deferred 三模式通知 |
| `async_notification_channel_t` | 异步通知通道（封装 `async_queue_t`，注入回调打破循环依赖） |
| `error_router_plugin_t` | 按码/模块组/域路由分发（单例） |
| `error_dedup_sampler_t` | 错误去重 + 采样插件（基于 identity_code 去重 + 确定性计数器采样） |

### 🛠️ Utils 层

| 📦 类/工具 | 🎯 职责 |
|------|------|
| `async_queue_t<T, Processor>` | 异步工作队列模板（单生产者-单消费者，死锁安全析构） |
| `string_utils_t` | FNV-1a 哈希、前缀/后缀检测、数字解析、字符串变换 |
| `string_format_t` | `{}` 占位符格式化（支持算术/指针/bool/`to_string` 类型） |
| `json_dict_t` | JSON 解析与点路径访问 |
| `json_lexer_t` | JSON 词法分析器（支持 UTF-16 代理对） |
| `file_utils_t` | 跨平台文件操作（含 `MAX_READ_FILE_SIZE` OOM 防护） |
| `stack_trace_utils_t` | 跨平台堆栈跟踪（POSIX `backtrace` + Windows `StackWalk64`） |
| `source_location_t` | 源位置封装（文件/函数/行号，`current()` 默认参数捕获调用者位置） |
| `operator<<` | `error_context_t` 输出流 |

---

## 🎯 关键设计决策

### 🔢 1. 64 位位移 + 掩码

```cpp
static constexpr uint32_t LEVEL_SHIFT = 56;
static constexpr uint64_t LEVEL_MASK = 0xFULL;

constexpr error_level_t get_level() const noexcept {
    return from_int(static_cast<uint8_t>((code_ >> LEVEL_SHIFT) & LEVEL_MASK));
}
```

> ⚡ 单条指令完成字段提取，100% 避免位域未定义行为。
> 📝 Reserved 3 bits 实为语义位：bit0=`retryable`、bit1=`transient`、bit2 预留，默认 0 兼容历史，`is_retryable()` / `is_transient()` / `set_retryable()` / `set_transient()` 直接读写。

### ⚡ 2. constexpr 全链路

`error_code_t` 构造、`to_string()`、`from_string()`、`hash()` 均为 `constexpr`，编译期确定的值直接进入只读数据段。

### 🔄 3. 循环依赖打破

- **循环 1**（plugin_registry ↔ error_context）：`error_context_initializer_t::initialize()` 调用 `plugin_registry_t::instance()`，实现放 `src/core/error_context_initializer.cc`，`.cc` 可安全 include `plugin_registry.h`。
- **循环 2**（async_notification_channel ↔ plugin_registry）：构造时注入 `notify_callback_t` 回调，channel 不感知具体 registry 实现（依赖倒置）。

### 🧱 4. 简化构造 API（v2.0）

移除 `code_with_location_t`，引入 `located_code_t` 通过隐式转换从 `error_code_t` 构造时自动捕获调用者位置。`source_location_t::current()` 作为默认参数在调用点展开 `__builtin_FILE()`，捕获调用者位置而非库内部位置。

### 📦 5. 多类型 payload

`with()` 模板版本通过 `if constexpr` 分派：bool → `"true"/"false"`，算术类型 → `std::to_string`，其他 → `static_cast<std::string>`。字符串类型优先匹配非模板重载。

### 🗃️ 6. 全局单例注册表

Meyer's singleton + `std::shared_mutex`；`register_plugin(unique_ptr)` 接管所有权，`register_plugin_ref()` 非持有引用；`notify_error()` 捕获每个插件异常，回调在锁外执行。

### 📢 7. 通知模式

| 🔧 模式 | ⚙️ 机制 | ⚠️ 丢失风险 | 🎯 场景 |
|------|------|:---:|------|
| `sync` | `error_context_t` 构造时同步调用插件 | 无 | 日志、指标 |
| `async_queue` | 通知推入 `async_queue_t`，工作线程异步消费 | 队列满拒绝入队 | 网络上报、持久化 |
| `sync_deferred` | 通知累积到线程本地缓冲，显式 `flush_deferred_notifications()` 触发 | 缓冲满丢弃 | 请求处理批处理、事务边界累积 |

> 💡 通知模式选择决策树详见 [决策树 · 1](decision_tree.md#1-通知模式选择)。

### 🛡️ 8. result_t\<T\> 零异常

- `std::variant<T, error_context_t>` + `std::get_if` + `thread_local` 哨兵值替代 `std::get`，防止跨线程污染
- `value()` / `value_or()` / `error()` 失败永不抛异常
- `map()` / `map_error()` / `and_then()` / `or_else()` 全部 noexcept；`match()` 不标记 noexcept（用户回调异常会传播，避免吞异常）
- 拷贝赋值 `= delete`；**移动赋值 `= default`（已实现）**，noexcept 跟随 T：`is_nothrow_move_assignable_v<T>`

### 🔍 9. 子系统索引（v2.1）

`subsystem_index_` 将 `get_errors_by_subsystem` 从 O(n) 优化为 O(1) 索引 + O(k) 遍历。

### 📸 10. RCU 快照（v2.3）

`plugin_registry_t` 用 `shared_ptr<const vector>` + `atomic_load/store` 实现无锁热路径。读者持有 `shared_ptr` 共享所有权，避免 unregister 后 use-after-free。

### 📥 11. SSO 小负载（v2.3）

`error_context_t` 载荷使用 `array<pair<string,string>, 4>` 栈存储 + `unique_ptr<unordered_map>` 惰性溢出，≤4 项零堆分配。`payload_count_` 仅计 SSO 部分，溢出后归零。

### 🚀 12. 异步队列模板

`async_queue_t<T, Processor>` 支持死锁安全析构、背压控制（`set_max_size` 加锁保护）、异常隔离、零 `std::function` 开销。

### 📊 13. 自动堆栈 + Per-Code 覆盖

优先级：`is_stacktrace_enabled → per-code 覆盖 → 全局阈值`。

### 🔒 14. 安全性设计

| ⚠️ 风险 | 🛡️ 防护措施 |
|------|------|
| `std::bad_alloc` | 所有 `noexcept` 函数内部 `try-catch` 捕获并记录到 stderr |
| 跨线程 result 哨兵污染 | `thread_local` 哨兵值，每线程独立 |
| RCU 快照 use-after-free | `shared_ptr` 共享所有权，读者持有插件引用 |
| 异步队列满 OOM | 背压控制 `set_max_queue_size` |
| 持锁调用用户回调死锁 | 复制回调指针 → 释放锁 → 调用 `on_error()` |
| 文件读取 OOM 攻击 | `MAX_READ_FILE_SIZE` 上限保护（64 MB） |
| JSON 解析器孤立代理区码点 | `append_utf8()` 静默丢弃 0xD800-0xDFFF 范围 |
| 移动后源对象状态不一致 | 显式清零源对象 `payload_count_`，自赋值安全 |
| payload 写入 bad_alloc 静默吞掉 | `payload_error_` 标志位，`has_payload_error()` 可查询 |
| 反序列化字符串生命周期 | `loc_file_storage_` / `loc_func_storage_` 持有字符串 |

### 🧩 15. SRP 拆分

原 `error_config_t` 与 `error_context_t` 承担过多职责，按单一职责原则拆分：

| 📦 原类 | 🔀 拆分后 | 🎯 单一职责 |
|------|------|------|
| `error_config_t` | `feature_flags_t` | 编译期特性开关 + 运行时布尔标志位 |
| `error_config_t` | `stacktrace_config_t` | 堆栈追踪全局阈值 + per-code 覆盖 |
| `error_config_t` | `formatter_config_t` | 自定义格式化器配置 |
| `error_config_t` | `i18n_config_t` | i18n 启用开关 + 输出/默认 locale 配置 |
| `error_context_t` | `error_context_t` | 数据类：码 + 消息 + 负载 + 因果链 + 堆栈 + 源位置 |
| `error_context_t` | `error_context_serializer_t` | 序列化：文本 / JSON / 二进制 + 反序列化 |
| `error_context_t` | `error_context_initializer_t` | 运行时特性初始化：校验 / 堆栈 / 位置 / 通知 |
| `error_registry_t` | `duplicate_policy_handler_t` | 重复注册策略（skip / overwrite / warn） |
| `error_registry_t` | `subsystem_module_catalog_t` | 子系统/模块名称多语言目录 |

> 💡 `error_config.h` 仅作向后兼容的统一包含入口，新代码直接包含细分头文件。

### ⚙️ 16. 编译期特性开关 + 死代码消除

`feature_flags_t` 通过 `if constexpr` + 编译期常量（`STACKTRACE_ENABLED` / `VALIDATION_ENABLED` / `LOCATION_ENABLED`）消除未启用分支的运行时开销，由编译器 DCE 优化。

```cpp
[[nodiscard]] static bool is_stacktrace_enabled() noexcept {
    if constexpr (STACKTRACE_ENABLED) {
        return get_flag_stacktrace_().load();
    } else {
        return false;  // 编译期返回 false，整条分支被 DCE
    }
}
```

### 🏷️ 17. 错误码自动注册

`DEFINE_ERROR_CODE` 宏结合 `constexpr error_code_t` 常量与 `inline const error_registrar_t` 静态初始化器，在 `main()` 前完成注册，避免跨 TU 静态初始化顺序问题（SIOF）。

```cpp
constexpr error_code_t NAME(LEVEL, SYSTEM, SUBSYS, MODULE, NUMBER);
inline const error_registrar_t NAME##_registrar_(NAME, #NAME, DESC, SUBSYS_NAME, MODULE_NAME);
```

> ⚠️ 单例用 `std::call_once` + 函数局部静态，跨 TU 安全；但请勿在其它 TU 的静态初始化代码中查询注册表。

### ⏱️ 18. 纪元驱动线程本地缓存

`error_registry_t` 通过 `std::atomic<uint64_t> epoch_counter_` 驱动 `thread_local` 缓存失效：register/unregister 调用 `fetch_add(1, release)`，读取用 `acquire` 配对。缓存为 `thread_local` 环形缓冲（容量 16），命中时零锁开销，同时缓存"未注册"结果。

```cpp
void bump_epoch_() noexcept {
    epoch_counter_.fetch_add(1, std::memory_order_release);
}
[[nodiscard]] uint64_t get_epoch() const noexcept {
    return epoch_counter_.load(std::memory_order_acquire);
}
```

### 🌍 19. i18n 多语言设计

`i18n_t` 单例维护 `locale_t → (code identity → message)` 两级哈希，查询路径：active → default → 空字符串。`subsystem_module_catalog_t` 维护 `(subsystem_id, module_id) → (locale → 名称)` 两级映射，与 `i18n_t` 共用 `locale_t` 但独立存储，避免双源不同步。

> 📝 `subsystem_module_catalog_t` 不持有 locale 状态，查询时由调用方（如 `error_context_serializer_t`）显式传入 locale。

### 🔀 20. 错误码迁移与废弃

`error_migration_registry_t` 分离「废弃」与「迁移」两个正交维度：废弃标记不一定有替代码，迁移也不一定意味着源码已废弃（可能是别名）。`migrate_recursive` 沿迁移链递归跳转，最大深度 16 防栈溢出，检测到环后返回当前码。

### 📡 21. constexpr 状态码映射

`status_mapper_t` 为纯函数工具类，`to_http_status` / `to_grpc_status` 均 `constexpr`。映射策略：retryable/transient 优先 → 503/UNAVAILABLE；info/warn → 200/OK；error 按系统域细分（application→400, third_party→502, database/middleware→503, system/none→500）；fatal → 500/INTERNAL。

```cpp
[[nodiscard]] static constexpr http_status_t to_http_status(error_code_t code) noexcept {
    if (code.is_success_code()) { return http_status_t{http_status_t::ok}; }
    return to_http_status_impl_(code.get_level(), code.get_system(),
                                code.is_retryable(), code.is_transient());
}
```

---

## ⚙️ 编译配置

| 🔧 CMake 选项 | 📝 说明 | 🏁 默认 |
|------|------|:---:|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | 编译堆栈追踪功能（链接 OS 底层库） | ON |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | 编译严格错误码验证器 | ON |
| `ERROR_SYSTEM_ENABLE_LOCATION` | 编译源位置宏 | ON |
| `ERROR_SYSTEM_ENABLE_LTO` | 启用 LTO/IPO | OFF |
| `ERROR_SYSTEM_ENABLE_PGO_GENERATE` | PGO 生成阶段 | OFF |
| `ERROR_SYSTEM_ENABLE_PGO_USE` | PGO 使用阶段 | OFF |
| `ERROR_SYSTEM_ENABLE_ASAN` | 地址安全检查器 | OFF |
| `ERROR_SYSTEM_ENABLE_UBSAN` | 未定义行为检测器 | OFF |
| `ERROR_SYSTEM_WARNINGS_AS_ERRORS` | 警告视为错误 | ON |
| `ERROR_SYSTEM_BUILD_TESTS` | 构建单元测试 | ON |
| `ERROR_SYSTEM_BUILD_EXAMPLES` | 构建示例 | ON |
| `ERROR_SYSTEM_BUILD_PERF_TESTS` | 构建性能基准 | ON |

> ⚠️ `ERROR_SYSTEM_ENABLE_PGO_GENERATE` 与 `ERROR_SYSTEM_ENABLE_PGO_USE` 不可同时启用，否则 CMake 报错。
> 📝 警告包含 `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion -Wnull-dereference -Wformat=2 -Wnon-virtual-dtor`（MSVC `/W4`）。

---

## 🧪 测试架构

GoogleTest v1.14.0（`FetchContent`）+ `gtest_discover_tests` 注册到 CTest；`tests/` 镜像 `include/` 结构，每个模块独立 `*_test.cc`，共 **21 个测试文件 / 469 个用例**，链接 `error_system::error_system` + `GTest::gtest_main`，仅应用警告选项不应用 LTO/PGO/Sanitizer。性能基准位于 `tests/perf/*_benchmark.cc`（3 个 benchmark）；Python3 脚本从 `config/errors/*.json` 生成错误码头文件、O(1) 字典与文档。

---

## 📊 测试覆盖要点

| 🧪 模块 | 🎯 覆盖要点 |
|------|------|
| `error_code_t` | 字段位移/掩码、retryable/transient 位、`get_identity_code`、比较运算 |
| `error_context_t` | SSO/溢出切换、因果链 wrap、payload 错误标志、移动赋值与自赋值 |
| `error_context_serializer_t` | 文本/JSON/二进制往返、反序列化字符串生命周期、cause 链递归 |
| `error_context_initializer_t` | 三种通知模式分派、成功码跳过、堆栈阈值与 per-code 覆盖 |
| `error_registry_t` | 注册/注销/查询、纪元失效、线程本地缓存命中/未注册缓存、模块组注销 |
| `result_t<T>` | value/error/map/and_then/or_else/match、用户函数异常转 fatal、移动赋值 |
| `i18n_t` / `subsystem_module_catalog_t` | locale 回退、批量注册、清除、查询 |
| `i18n_config_t` | i18n 开关、output/default locale 设置与回退、原子读写 |
| `status_mapper_t` | retryable/transient 优先、各系统域映射、constexpr 编译期求值 |
| `error_migration_registry_t` | 单跳/递归迁移、环检测、废弃标记与迁移映射独立性 |
| `plugin_registry_t` | RCU 快照、sync/async/deferred 三模式、延迟缓冲溢出、use-after-free 防护 |
| `error_router_plugin_t` | 按码/模块组/域路由、注册/注销处理器 |
| `error_dedup_sampler_t` | 时间窗口去重、确定性采样、统计计数、并发安全 |
| `async_queue_t` | 死锁安全析构、背压、处理器异常隔离、首次 enqueue 启动 worker |
| `file_utils_t` | `MAX_READ_FILE_SIZE` 上限、读写创建删除 |
| `json_lexer_t` / `json_dict_t` | 代理对处理、点路径访问、孤立代理区码点丢弃 |
| `string_format_t` / `string_utils_t` | `{}` 占位符、FNV-1a 哈希、数字解析 |
| `stack_trace_utils_t` | 跨平台栈抓取（POSIX/Windows） |
