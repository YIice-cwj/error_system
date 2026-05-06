# 架构设计文档

本文档描述错误码系统的整体架构、模块职责划分及关键设计决策。

---

## 总体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        Error System                              │
├──────────────┬──────────────┬────────────────┬──────────────────┤
│  Core 层     │  i18n 层     │  Plugin 层     │  Utils 层        │
│  核心数据结构 │  多语言翻译  │  可扩展插件    │  通用工具        │
├──────────────┴──────────────┴────────────────┴──────────────────┤
│  Domain / Subsystem / Module / Traits 层                         │
│  预定义的 18 大系统域、36+ 子系统枚举、模块枚举及 Traits 转换    │
├─────────────────────────────────────────────────────────────────┤
│  Memory 层                                                       │
│  线程局部对象池 (object_pool_t)，优化高频内存分配                │
└─────────────────────────────────────────────────────────────────┘
```

### 依赖关系

```
Utils  ←─────────────────────────────── 被所有层使用
  ↑
Core (error_code_t, error_level_t, error_config)
  ↑                  ↑
Domain/Traits      error_context_t  ←─── Plugin 层监听
                        ↑
                     i18n 层（翻译 code → 文本）
```

> 核心原则：**下层不依赖上层**，`error_code_t` 不感知 i18n 和 Plugin。

---

## 层次说明

### Core 层 (`include/error_system/core/`)

| 文件 | 类/结构 | 职责 |
|------|---------|------|
| `error_code.h` | `error_code_t` | 64 位错误码的存储与字段解析 |
| `error_level.h` | `error_level_t` | 错误等级枚举及转换函数 |
| `error_builder.h` | `error_builder_t` | 编译期错误码构建工厂 |
| `error_context.h` | `error_context_t` | 错误上下文（码+消息+因果链+结构化负载+堆栈跟踪） |
| `error_config.h` | `error_config` | 全局错误配置（堆栈阈值、默认语言、验证开关） |
| `result.h` | `result_t<T>` | 类 Rust Result，替代异常传递错误 |
| `error_registry.h` | `error_registry_t` | 错误码注册表，支持按模块组索引 |
| `error_exception.h` | `error_exception_t` | 基于 `std::exception` 的异常封装 |

### Domain / Subsystem / Module / Traits 层

预定义 18 大系统域、每域的子系统枚举和模块枚举，并提供 Traits 类型萃取用于编译期枚举↔字符串↔整数的双向转换。

### i18n 层 (`include/error_system/i18n/`)

| 文件 | 类 | 职责 |
|------|----|------|
| `i_translator.h` | `i_translator_t` | 翻译器抽象接口 |
| `json_translator.h` | `json_translator_t` | 基于 JSON 字典的翻译实现 |
| `translator_registry.h` | `translator_registry_t` | 全局翻译器单例注册表 |
| `language.h` | `language_t` | 支持语言枚举 |

### Plugin 层 (`include/error_system/plugin/`)

| 文件 | 类 | 职责 |
|------|----|------|
| `i_error_plugin.h` | `i_error_plugin_t` | 插件抽象接口 |
| `plugin_registry.h` | `plugin_registry_t` | 插件单例注册表，负责广播 |

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
| `error_formatter.h` | `operator<<` | `error_context_t` 输出流运算符重载 |

---

## 关键设计决策

### 1. 64 位 union + 位域实现零开销

```cpp
union error_code_union_t {
    uint64_t code;
    fields_t fields;  // 位域结构体
};
```

直接 `reinterpret` 整数与位域，字段读取等价于位运算，编译器可优化到单条指令。

### 2. `constexpr` 全链路

`error_builder_t::make_error_code()`、`to_string()`、`from_string()`、`hash()` 均为 `constexpr`，错误码可在编译期完全确定，进入只读数据段。

### 3. error_context_t 中的循环依赖打破

`plugin_registry.h` → `i_error_plugin.h` → `error_context.h`，若 `error_context.h` 直接 include `plugin_registry.h` 则循环。

**解法**：在 `error_context.h` 内仅声明自由函数 `__notify_plugins()`（无需完整定义 `plugin_registry_t`），其实现放在 `src/core/error_context.cc` 中，该 `.cc` 可安全 include `plugin_registry.h`。

```
error_context.h      ── 声明 __notify_plugins() ──→  头文件层无环
error_context.cc     ── include plugin_registry.h ──→ 仅在 .cc 连接
```

### 4. 全局单例注册表模式

i18n 的 `translator_registry_t` 和 Plugin 的 `plugin_registry_t` 均采用单例模式：
- **不持有所有权**：调用方负责对象生命周期
- **Meyer's singleton**：`static` 局部变量，C++11 保证线程安全的初始化
- **线程安全访问**：`plugin_registry_t` 使用 `std::shared_mutex` 保护插件列表；`translator_registry_t` 使用 `std::atomic` 保证指针操作的原子性

### 5. result_t\<T\> 无异常错误传递

```cpp
result_t<Data> fetch() {
    if (failed) return {error_code, "原因"};
    return data;
}

auto r = fetch();
if (r.is_error()) log(r.error().to_string());
```

避免异常开销，同时强制调用方显式处理错误。

### 6. 对象池优化因果链

`error_context_t::wrap()` 使用线程局部对象池分配 cause 节点，仅在池满时才回退到 `std::make_shared`，显著降低高频错误链场景下的堆分配压力。

```cpp
object_pool_t& pool = object_pool_t::instance_thread_local();
error_context_t* ptr = pool.acquire();
if (ptr) {
    *ptr = underlying;
    new_code_context.cause = std::shared_ptr<error_context_t>(ptr, [&pool](auto* p){ pool.release(p); });
} else {
    new_code_context.cause = std::make_shared<error_context_t>(underlying);
}
```

### 7. JSON 与二进制序列化

`error_context_t` 提供两种序列化方式，满足不同场景需求：

- **`to_json()`**: 生成人类可读的 JSON 字符串，包含 `code`、`message`、`payload`、`stack_frames`、`cause` 等完整字段，便于日志存储和网络传输。
- **`to_binary()`**: 生成紧凑的二进制格式，包含原始 `uint64_t` 错误码、消息长度与内容、payload 键值对等，适合高性能 RPC 或持久化存储。

底层使用 `string_utils_t::escape_json()` 处理 JSON 中的特殊字符转义。

### 8. 自动堆栈跟踪
`error_context_t` 构造函数根据 `error_config::get_stacktrace_level()` 阈值和编译选项自动决定是否抓取调用栈：

```cpp
// 设置 WARN 及以上自动捕获堆栈
error_config::set_stacktrace_level(error_level_t::warn);

// 构造时自动判断：code.level >= warn ? 抓取堆栈 : 跳过
error_context_t ctx(code, "错误信息");  // 若 code.level >= warn，ctx.stack_frames 自动填充
```

底层使用 `stack_trace_utils_t::generate()` 实现跨平台堆栈抓取，支持 Linux (`backtrace`)、macOS (`backtrace`) 和 Windows (`CaptureStackBackTrace`)。

> **注意**：堆栈追踪功能可通过 CMake 选项 `ERROR_SYSTEM_ENABLE_STACKTRACE` 在编译期开启或关闭。关闭后相关 API 将标记为 `[[deprecated]]` 并返回默认值。

### 9. 错误码验证

若 `error_config::is_validation_enabled()` 为 `true` 且错误码未在 `error_registry_t` 中注册，则 `error_context_t` 构造函数自动将错误码替换为 `fatal` 级别的未注册错误码，并在 `payload` 中附加 `illegal_raw_code` 字段。

> **注意**：验证功能可通过 CMake 选项 `ERROR_SYSTEM_ENABLE_VALIDATION` 在编译期开启或关闭。

### 10. 全局错误配置

`error_config_t` 提供进程级的错误行为配置：
- **堆栈捕获阈值**：控制自动堆栈跟踪的触发等级
- **默认语言**：影响 `translator_registry_t` 自动创建的默认翻译器语言
- **验证开关**：控制错误码注册验证的开启/关闭

```cpp
// 配置堆栈捕获阈值
error_config_t::set_stacktrace_level(error_level_t::error);

// 配置默认语言
error_config_t::set_default_language("en_us");

// 检查功能是否开启
if (error_config_t::is_stacktrace_enabled()) { /* ... */ }
if (error_config_t::is_validation_enabled()) { /* ... */ }
```

---

## 扩展指南

### 新增系统域

1. 在 `domain/system_domain.h` 的 `system_domain_t` 枚举中添加值
2. 添加对应的子系统枚举文件（`subsystem/xxx_subsystem.h`）
3. 添加对应的模块枚举文件（`module/xxx_module.h`）
4. 添加 traits 文件（`traits/subsystem/xxx_subsystem_traits.h`、`traits/module/xxx_module_traits.h`）
5. 在 `json_translator.cc` 的 switch 中新增 case
6. 更新 JSON 字典文件

### 新增翻译语言

1. 在 `language_t` 枚举中新增语言值
2. 在 `languages/` 目录添加对应 JSON 字典文件（如 `ja_jp.json`）
3. 字典格式参考 `docs/api/i18n.md`

### 新增插件

1. 继承 `plugin::i_error_plugin_t`，实现 `name()` 和 `on_error()`
2. 在程序启动时调用 `plugin_registry_t::instance().register_plugin(&plugin)`
3. 参考 `docs/api/plugin.md` 中的示例
