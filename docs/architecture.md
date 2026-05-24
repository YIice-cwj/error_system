# 架构设计文档

本文档描述错误码系统的整体架构、模块职责划分及关键设计决策。

---

## 总体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        Error System                              │
├──────────────┬────────────────┬──────────────────────────────────┤
│  Config 层   │  Core 层       │  Plugin 层                       │
│  全局配置    │  核心数据结构  │  可扩展插件                      │
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
Core (error_code_t, error_level_t, error_context_t)
  ↑                  ↑
Domain             Plugin 层监听
```

> 核心原则：**下层不依赖上层**，`error_code_t` 不感知 Plugin。

---

## 层次说明

### Config 层 (`include/error_system/config/`)

| 文件 | 类/结构 | 职责 |
|------|---------|------|
| `error_config.h` | `error_config_t` | 全局错误配置（堆栈阈值、验证开关、源位置追踪开关、自定义格式化器、自定义翻译器） |

### Core 层 (`include/error_system/core/`)

| 文件 | 类/结构 | 职责 |
|------|---------|------|
| `error_code.h` | `error_code_t` | 64 位错误码的存储与字段解析（基于位移操作，避免位域 UB） |
| `error_level.h` | `error_level_t` | 错误等级枚举及转换函数 |
| `error_builder.h` | `error_builder_t` | 编译期错误码构建工厂 |
| `error_context.h` | `error_context_t` | 错误上下文（码+消息+因果链+结构化负载+堆栈跟踪+源位置） |
| `error_registry.h` | `error_registry_t` | 错误码注册表，支持按模块组索引 |
| `result.h` | `result_t<T>` | 类 Rust Result，替代异常传递错误 |
| `error_exception.h` | `error_exception_t` | 基于 `std::exception` 的异常封装 |

### Domain 层 (`include/error_system/domain/`)

预定义 6 大系统域：`none`, `system`, `middleware`, `database`, `application`, `third_party`。

### Plugin 层 (`include/error_system/plugin/`)

| 文件 | 类 | 职责 |
|------|----|------|
| `i_error_plugin.h` | `i_error_plugin_t` | 插件抽象接口 |
| `plugin_registry.h` | `plugin_registry_t` | 插件单例注册表，负责广播 |
| `error_router_plugin.h` | `error_router_plugin_t` | 错误路由插件，按码/域/模块组分发 |

### Translator 层 (`include/error_system/translator/`)

| 文件 | 类 | 职责 |
|------|----|------|
| `error_translator.h` | `error_translator_t` | 错误翻译器，子系统/模块名称映射 |

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
    code_t code_{0};

    static constexpr uint32_t LEVEL_SHIFT = 56;
    static constexpr uint64_t LEVEL_MASK = 0xFULL;

    constexpr error_level_t get_level() const noexcept {
        return static_cast<error_level_t>((code_ >> LEVEL_SHIFT) & LEVEL_MASK);
    }
};
```

直接通过位移和掩码操作读取字段，编译器可优化到单条指令，100% 避免严格别名规则与位域排布未定义行为。

### 2. `constexpr` 全链路

`error_builder_t::make_error_code()`、`to_string()`、`from_string()`、`hash()` 均为 `constexpr`，错误码可在编译期完全确定，进入只读数据段。

### 3. error_context_t 中的循环依赖打破

`plugin_registry.h` → `i_error_plugin.h` → `error_context.h`，若 `error_context.h` 直接 include `plugin_registry.h` 则循环。

**解法**：在 `error_context.h` 内仅声明自由函数 `notify_plugins()`（无需完整定义 `plugin_registry_t`），其实现放在 `src/core/error_context.cc` 中，该 `.cc` 可安全 include `plugin_registry.h`。

```
error_context.h      ── 声明 notify_plugins() ──→  头文件层无环
error_context.cc     ── include plugin_registry.h ──→ 仅在 .cc 连接
```

### 4. 全局单例注册表模式

Plugin 的 `plugin_registry_t` 采用单例模式：
- **不持有所有权**：调用方负责对象生命周期
- **Meyer's singleton**：`static` 局部变量，C++11 保证线程安全的初始化
- **线程安全访问**：`plugin_registry_t` 使用 `std::shared_mutex` 保护插件列表

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

### 8. 源位置追踪

通过 `ERROR_SYSTEM_ENABLE_LOCATION` CMake 选项开启后，`error_context_t` 构造函数会自动捕获错误发生的源文件位置：

```cpp
error_context_t ctx(code, "连接超时");  // 自动记录 file_name, function_name, line_number
```

- 使用 `utils::source_location_t::current()` 获取编译器内置的源位置信息
- 支持完整路径和短文件名两种模式（通过 `config::error_config_t::set_enable_short_filename()` 切换）
- 源位置信息会嵌入到 `to_string()`、`to_json()` 和 `to_binary()` 的输出中
- 若编译期关闭此功能，相关 API 将标记为 `[[deprecated]]` 并返回默认值

### 9. 自动堆栈跟踪

`error_context_t` 构造函数根据 `config::error_config_t::get_stacktrace_level()` 阈值和编译选项自动决定是否抓取调用栈：

```cpp
// 设置 WARN 及以上自动捕获堆栈
config::error_config_t::set_stacktrace_level(error_level_t::warn);

// 构造时自动判断：code.level >= warn ? 抓取堆栈 : 跳过
error_context_t ctx(code, "错误信息");  // 若 code.level >= warn，ctx.stack_frames 自动填充
```

底层使用 `stack_trace_utils_t::generate()` 实现跨平台堆栈抓取，支持 Linux (`backtrace`)、macOS (`backtrace`) 和 Windows (`CaptureStackBackTrace`)。

> **注意**：堆栈追踪功能可通过 CMake 选项 `ERROR_SYSTEM_ENABLE_STACKTRACE` 在编译期开启或关闭。关闭后相关 API 将标记为 `[[deprecated]]` 并返回默认值。

### 10. 错误码验证

若 `config::error_config_t::is_validation_enabled()` 为 `true` 且错误码未在 `error_registry_t` 中注册，则 `error_context_t` 构造函数自动将错误码替换为 `fatal` 级别的未注册错误码，并在 `payload` 中附加 `illegal_raw_code` 字段。

> **注意**：验证功能可通过 CMake 选项 `ERROR_SYSTEM_ENABLE_VALIDATION` 在编译期开启或关闭。

### 11. 全局错误配置

`config::error_config_t` 提供进程级的错误行为配置，所有方法均为 `static`：

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `min_stacktrace_level_` | `error` | 自动捕获堆栈的最低等级 |
| `enable_stacktrace_` | `true` | 堆栈追踪总开关 |
| `enable_validation_` | `true` | 错误码验证开关 |
| `enable_source_location_` | `true` | 源位置追踪开关 |
| `enable_short_filename_` | `true` | 短文件名模式 |
| `custom_formatter_` | `nullptr` | 自定义格式化回调 |
| `custom_translator_` | `nullptr` | 自定义翻译函数 |

### 12. DEFINE_ERROR_CODE 宏

系统提供 `DEFINE_ERROR_CODE` 宏用于在定义错误码的同时自动注册到错误码注册表：

```cpp
DEFINE_ERROR_CODE(
    ERR_DB_CONNECTION_TIMEOUT,
    error_system::core::error_level_t::error,
    error_system::domain::system_domain_t::database,
    1, 1, 0x0001,
    "数据库连接超时");
```

宏会在静态初始化阶段自动调用 `error_registry_t::instance().register_error()` 完成注册。

### 13. 代码生成工具

系统提供 Python 脚本 `script/script_py/generate_error_codes.py` 从 JSON 配置自动生成错误码定义头文件：

```bash
python script/script_py/generate_error_codes.py \
    config/errors/trade_service_errors.json \
    include/generated/
```

JSON 配置格式：

```json
{
    "namespace": "biz::trade_errors",
    "service_name": "trade",
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

---

## 编译期配置选项

通过 CMake 选项控制功能开关：

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | `ON` | 启用堆栈追踪功能 |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | `ON` | 启用错误码验证 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | `ON` | 启用源位置追踪 |

关闭后相关 API 将标记为 `[[deprecated]]` 并返回默认值，实现零开销的编译期裁剪。

---

## 测试架构

### 测试框架

- **测试框架**: GoogleTest
- **测试文件数**: 16 个
- **测试覆盖模块**: Core, Plugin, Memory, Utils, Config, Translator, Domain

### 测试覆盖

| 模块 | 测试文件数 | 测试用例数 | 测试文件 |
|------|-----------|-----------|----------|
| Core 层 | 7 | 59+ | `tests/core/*_test.cc` |
| Plugin 层 | 2 | 17 | `tests/plugin/*_test.cc` |
| Memory 层 | 1 | 10 | `tests/memory/*_test.cc` |
| Utils 层 | 5 | 37+ | `tests/utils/*_test.cc` |
| Config 层 | 1 | 7 | `tests/config/*_test.cc` |
| Translator 层 | 1 | 9 | `tests/translator/*_test.cc` |
| Domain 层 | 1 | - | `tests/domain/*_test.cc` |
| **总计** | **17** | **149+** | - |

### 测试目录结构

```
tests/
├── CMakeLists.txt
├── core/                          # Core 层测试
│   ├── error_code_test.cc        # error_code_t 测试
│   ├── error_level_test.cc       # error_level_t 测试
│   ├── error_builder_test.cc     # error_builder_t 测试
│   ├── result_test.cc            # result_t<T> 测试
│   ├── error_exception_test.cc   # error_exception_t 测试
│   ├── error_registry_test.cc    # error_registry_t 测试
│   └── domain_test.cc            # system_domain_t 测试
├── plugin/                        # Plugin 层测试
│   ├── plugin_registry_test.cc   # plugin_registry_t 测试
│   └── error_router_plugin_test.cc # error_router_plugin_t 测试
├── memory/                        # Memory 层测试
│   └── object_pool_test.cc       # object_pool_t 测试
├── utils/                         # Utils 层测试
│   ├── string_utils_test.cc      # string_utils_t 测试
│   ├── json_utils_test.cc        # json_dict_t 测试
│   ├── file_utils_test.cc        # file_utils 测试
│   ├── stack_trace_utils_test.cc # stack_trace_utils_t 测试
│   └── source_location_test.cc   # source_location_t 测试
├── config/                        # Config 层测试
│   └── error_config_test.cc      # error_config_t 测试
└── translator/                    # Translator 层测试
    └── error_translator_test.cc  # error_translator_t 测试
```

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

### 测试设计原则

1. **全面覆盖**: 每个公共 API 都有对应的测试用例
2. **边界测试**: 测试空值、极限值、非法输入等边界条件
3. **constexpr 测试**: 验证编译期计算的正确性
4. **异常安全**: 测试异常处理和错误恢复
5. **线程安全**: 测试并发场景下的正确性
