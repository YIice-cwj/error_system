# Core 层 API 文档

> 命名空间：`error_system::core`

Core 层是整个错误系统的基础，定义了错误码数据结构、错误等级、错误上下文、构建器、全局配置、结果封装、异常封装及错误注册表。

---

## error_code_t

**头文件**：`error_system/core/error_code.h`

64 位错误码的核心数据类，使用 `union` + 位域实现零开销的字段拆解。

### 位域结构

| 位区间 | 长度 | 字段 | 描述 |
|--------|------|------|------|
| `63` | 1 bit | `sign` | 符号位（1 = 错误） |
| `60–62` | 3 bit | `reserved` | 预留位 |
| `56–59` | 4 bit | `level` | 错误等级 |
| `48–55` | 8 bit | `system` | 系统域 |
| `32–47` | 16 bit | `subsys` | 子系统编号 |
| `16–31` | 16 bit | `module` | 模块编号 |
| `0–15` | 16 bit | `number` | 具体错误编号 |

### 方法

| 方法 | 返回值 | 描述 |
|------|--------|------|
| `get_code()` | `uint64_t` | 获取原始 64 位码 |
| `get_sign()` | `uint8_t` | 获取符号位 |
| `get_reserved()` | `uint8_t` | 获取预留位 |
| `get_level()` | `error_level_t` | 获取错误等级 |
| `get_system()` | `system_domain_t` | 获取系统域 |
| `get_subsys()` | `uint16_t` | 获取子系统编号 |
| `get_module()` | `uint16_t` | 获取模块编号 |
| `get_number()` | `uint16_t` | 获取错误编号 |
| `get_module_group_id()` | `uint64_t` | 获取模块聚合隔离 ID（高 8 位和低 16 位置零，保留系统+子系统+模块信息） |
| `operator uint64_t()` | `uint64_t` | 隐式转换为原始码 |

### 示例

```cpp
error_code_t code(raw_value);
if (code.get_sign() == 1) {
    auto level = code.get_level();   // error_level_t::fatal
    auto sys   = code.get_system();  // system_domain_t::database
    auto num   = code.get_number();  // 404
    auto group = code.get_module_group_id(); // 模块聚合 ID
}
```

### 单元测试

测试文件：`tests/core/error_code_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `default_constructor_creates_success_code` | 默认构造函数创建成功码（code=0） |
| `explicit_constructor_stores_raw_code` | 显式构造函数存储原始码值 |
| `implicit_conversion_to_code_t` | 隐式转换为 code_t 类型 |
| `get_module_group_id_extracts_correct_bits` | 模块组 ID 正确提取系统/子系统/模块信息 |
| `fields_are_correctly_extracted` | 各字段正确提取 |
| `constexpr_evaluation_works` | constexpr 编译期求值 |
| `zero_code_represents_success` | 零码表示成功 |

---

## error_level_t

**头文件**：`error_system/core/error_level.h`

错误等级枚举，对应错误码位域中的 `level` 字段。

| 枚举值 | 整数值 | 描述 |
|--------|--------|------|
| `debug` | 0 | 调试信息 |
| `info` | 1 | 一般信息 |
| `warn` | 2 | 警告 |
| `error` | 3 | 错误 |
| `fatal` | 4 | 致命错误 |

### 辅助函数

```cpp
// 枚举 ↔ 字符串
constexpr const char* to_string(error_level_t level);
constexpr error_level_t from_string(const char* str);

// 枚举 ↔ 整数
constexpr uint8_t to_int(error_level_t level);
constexpr error_level_t from_int(uint8_t value);

// 相邻等级
constexpr error_level_t next_level(error_level_t level);
constexpr error_level_t prev_level(error_level_t level);

// 等级过滤
constexpr bool should_log(error_level_t current, error_level_t min_level);
```

### 单元测试

测试文件：`tests/core/error_level_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `to_int_converts_correctly` | to_int 正确转换枚举到整数 |
| `from_int_converts_correctly` | from_int 正确转换整数到枚举 |
| `from_int_invalid_returns_fatal` | 无效整数值返回 fatal |
| `is_valid_checks_correctly` | is_valid 正确验证有效性 |
| `from_string_converts_correctly` | from_string 正确转换字符串到枚举 |
| `to_string_converts_correctly` | to_string 正确转换枚举到字符串 |
| `next_level_returns_correct_value` | next_level 返回下一级别 |
| `prev_level_returns_correct_value` | prev_level 返回上一级别 |
| `should_log_compares_correctly` | should_log 正确比较等级 |
| `constexpr_evaluation_works` | constexpr 编译期求值 |

---

## error_builder_t

**头文件**：`error_system/core/error_builder.h`

构建 `error_code_t` 的工厂类，所有方法均为 `constexpr`，支持编译期构建。

### 方法

```cpp
// 按字段构建（支持枚举类型的子系统和模块）
template<typename SubSystemEnum, typename ModuleEnum>
static constexpr error_code_t make_error_code(
    error_level_t level,
    domain::system_domain_t system,
    SubSystemEnum subsys,
    ModuleEnum module,
    uint16_t number
) noexcept;

// 按原始值构建
static constexpr error_code_t make_error_code(
    error_level_t level,
    domain::system_domain_t system,
    uint16_t subsys,
    uint16_t module,
    uint16_t number
) noexcept;

// 从原始 64 位码构建
static constexpr error_code_t make_error_code(uint64_t code) noexcept;
```

### 示例

```cpp
// 编译期常量，零运行时开销
constexpr auto db_conn_err = error_builder_t::make_error_code(
    error_level_t::fatal,
    domain::system_domain_t::database,
    100,   // subsystem id
    200,   // module id
    404    // error number
);
```

### 单元测试

测试文件：`tests/core/error_builder_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `make_error_code_with_raw_values` | 使用原始值构建错误码 |
| `make_error_code_with_enum_values` | 使用枚举值构建错误码 |
| `make_error_code_from_raw_code` | 从原始 64 位码构建 |
| `constexpr_error_code_construction` | constexpr 编译期构造 |
| `success_code_has_zero_sign` | 成功码符号位为 0 |
| `different_levels_produce_different_codes` | 不同等级产生不同码值 |
| `different_systems_produce_different_codes` | 不同系统产生不同码值 |

---

## error_config_t

**头文件**：`error_system/config/error_config.h`

> 命名空间：`error_system::config`

全局错误配置类，提供进程级的错误行为配置。所有方法均为 `static`，不可实例化。

> **注意**：堆栈追踪、错误码验证和源位置追踪功能可通过 CMake 选项 `ERROR_SYSTEM_ENABLE_STACKTRACE` / `ERROR_SYSTEM_ENABLE_VALIDATION` / `ERROR_SYSTEM_ENABLE_LOCATION` 在编译期开启或关闭。

### 配置项

| 配置项 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `min_stacktrace_level_` | `error_level_t` | `error` | 自动捕获堆栈的最低错误等级（仅在开启堆栈追踪时有效） |
| `enable_stacktrace_` | `bool` | `true` | 是否开启堆栈追踪（仅在开启编译选项时有效） |
| `enable_validation_` | `bool` | `true` | 是否开启错误码验证（仅在开启编译选项时有效） |
| `enable_source_location_` | `bool` | `true` | 是否开启源位置追踪（仅在开启编译选项时有效） |
| `enable_short_filename_` | `bool` | `true` | 是否使用短文件名（仅在开启源位置追踪时有效） |

### 方法

```cpp
// 堆栈追踪配置
static error_level_t get_stacktrace_level() noexcept;
static void set_stacktrace_level(error_level_t level) noexcept;
static void set_enable_stacktrace(bool enable) noexcept;
static bool is_stacktrace_enabled() noexcept;

// 错误码验证配置
static void set_enable_validation(bool enable) noexcept;
static bool is_validation_enabled() noexcept;

// 源位置追踪配置
static void set_enable_source_location(bool enable) noexcept;
static bool is_source_location_enabled() noexcept;
static void set_enable_short_filename(bool enable) noexcept;
static bool is_short_filename_enabled() noexcept;
```

### 示例

```cpp
using namespace error_system::config;

// 设置 WARN 及以上自动捕获堆栈
error_config_t::set_stacktrace_level(error_level_t::warn);

// 检查功能是否开启
if (error_config_t::is_stacktrace_enabled()) {
    // ...
}

// 创建错误上下文时，若等级 >= warn 则自动抓取堆栈
error_context_t ctx(code, "连接超时");  // 可能自动包含 stack_frames
```

### 单元测试

测试文件：`tests/config/error_config_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `set_and_get_custom_formatter` | 设置和获取自定义格式化器 |
| `custom_formatter_can_be_null` | 自定义格式化器可为 null |
| `stacktrace_level_defaults` | 堆栈追踪等级默认值 |
| `is_stacktrace_enabled_returns_false_when_disabled` | 禁用时返回 false |
| `validation_can_be_toggled` | 验证功能可切换 |
| `source_location_can_be_toggled` | 源位置追踪可切换 |
| `short_filename_can_be_toggled` | 短文件名模式可切换 |

---

## error_context_t

**头文件**：`error_system/core/error_context.h`

错误上下文，封装错误码、消息文本、格式化参数、结构化负载、堆栈跟踪及因果链（cause chain）。

### 成员

| 成员 | 类型 | 描述 |
|------|------|------|
| `code` | `error_code_t` | 错误码 |
| `message` | `std::string` | 错误描述文本（已格式化） |
| `payload` | `std::unordered_map<std::string, std::string>` | 结构化键值对负载 |
| `cause` | `shared_ptr<error_context_t>` | 上级原因（可选） |
| `stack_frames` | `std::vector<std::string>` | 调用栈帧列表（仅在开启 `ERROR_SYSTEM_ENABLE_STACKTRACE` 时存在） |
| `file_name` | `const char*` | 源文件名（仅在开启 `ERROR_SYSTEM_ENABLE_LOCATION` 时存在） |
| `function_name` | `const char*` | 函数名（仅在开启 `ERROR_SYSTEM_ENABLE_LOCATION` 时存在） |
| `line_number` | `uint32_t` | 行号（仅在开启 `ERROR_SYSTEM_ENABLE_LOCATION` 时存在） |

### 构造函数

```cpp
// 默认构造（code = 0，表示无错误）
constexpr error_context_t() noexcept = default;

// 带格式化参数和自动源位置捕获的构造
// 若 code.get_level() >= error_config_t::get_stacktrace_level()，自动抓取调用栈
// 若源位置追踪开启，自动记录 file_name, function_name, line_number
template<typename... Args>
error_context_t(code_with_location_t code_with, std::string format = "", Args&&... args) noexcept;
```

### 方法

```cpp
// 判断是否为有效错误（code != 0）
explicit operator bool() const noexcept;

// 包装因果链：将 underlying 作为 this 的 cause
// 优先使用线程局部对象池分配，池满时回退到堆分配
error_context_t wrap(const error_context_t& underlying) const noexcept;
error_context_t wrap(error_context_t&& underlying) const noexcept;

// 添加结构化键值对，支持链式调用
error_context_t& with(const std::string& key, const std::string& value) noexcept;
error_context_t& with(std::string&& key, std::string&& value) noexcept;

// 转为可读字符串
std::string to_string() const noexcept;

// 转为 JSON 字符串
std::string to_json() const noexcept;

// 转为二进制字符串（用于网络传输或持久化）
std::string to_binary() const noexcept;
```

### 插件触发

构造 `error_context_t` 时（`code != 0`），会自动调用 `plugin::plugin_registry_t::instance().notify_error()`，通知所有已注册插件。

### 错误码验证

若 `config::error_config_t::is_validation_enabled()` 为 `true` 且错误码未在 `error_registry_t` 中注册，则自动将错误码替换为 `fatal` 级别的未注册错误码，并在 `payload` 中附加 `illegal_raw_code` 字段。

### 堆栈跟踪

当构造时的错误等级满足 `code.get_level() >= config::error_config_t::get_stacktrace_level()` 且堆栈追踪功能已开启时，构造函数内部自动调用 `utils::stack_trace_utils_t::generate(1)` 抓取当前调用栈，存储到 `stack_frames` 中。

### 源位置追踪

当 `ERROR_SYSTEM_ENABLE_LOCATION` 编译选项开启且 `config::error_config_t::is_source_location_enabled()` 为 `true` 时，构造函数通过 `code_with_location_t` 自动捕获源文件位置：

```cpp
// code_with_location_t 自动调用 source_location_t::current()
error_context_t ctx(code, "错误信息");  // 隐式构造 code_with_location_t
```

- `file_name`: 源文件路径（可通过 `set_enable_short_filename()` 切换为短文件名）
- `function_name`: 函数签名
- `line_number`: 行号

源位置信息会自动嵌入到 `to_string()`、`to_json()` 和 `to_binary()` 的输出中。

### 示例

```cpp
using namespace error_system::core;
using namespace error_system::config;

// 设置堆栈捕获阈值
error_config_t::set_stacktrace_level(error_level_t::warn);

// 创建错误上下文（自动抓取堆栈，因为 error >= warn）
error_context_t ctx{db_err, "数据库连接超时: {}", "timeout"};

// 添加结构化负载
ctx.with("host", "192.168.1.1").with("port", "3306");

// 包装因果链
error_context_t outer{app_err, "请求处理失败"};
auto chained = outer.wrap(ctx);

// 转字符串
std::cout << chained.to_string() << std::endl;
// [Level: fatal, System: database, ...] Code: 404 - 数据库连接超时: timeout {host=192.168.1.1, port=3306}
//   [Stacktrace]:
//     #0  some_function
//     #1  caller_function
//   ↳ Caused by: ...
```

---

## result_t\<T\>

**头文件**：`error_system/core/result.h`

类 Rust `Result` 的返回值封装，持有成功值或错误上下文，不使用异常。

```cpp
// 泛型版本
result_t<int> r1 = 42;                          // 成功
result_t<int> r2 = error_context_t{some_code, "失败原因"};     // 错误

r1.is_success(); // true
r2.is_error();   // true
r2.error();      // error_context_t&
r1.value();      // int&

// void 特化（无成功值）
result_t<void> r3;                              // 成功（code == 0）
result_t<void> r4 = error_context_t{some_code, "操作失败"};   // 错误

// void 特化支持格式化构造
result_t<void> r5(some_code, "操作失败: {}", "原因");  // 错误，带格式化消息
```

### 链式操作

```cpp
// and_then: 成功时继续执行，失败时短路传递错误
// 支持左值引用和右值引用两种版本
template <typename Function>
auto and_then(Function&& function) &&;
template <typename Function>
auto and_then(Function&& function) &;

// or_else: 失败时执行恢复逻辑
// 支持左值引用和右值引用两种版本
template <typename Function>
result_t<value_type> or_else(Function&& function) &&;
template <typename Function>
result_t<value_type> or_else(Function&& function) &;
```

### 示例

```cpp
result_t<int> divide(int a, int b) {
    if (b == 0) {
        auto code = error_builder_t::make_error_code(
            error_level_t::error, domain::system_domain_t::application, 0, 0, 1);
        return error_context_t(code, "除数不能为零");
    }
    return a / b;
}

auto result = divide(10, 0);
if (result.is_error()) {
    std::cerr << result.error().to_string() << "\n";
}
```

### 单元测试

测试文件：`tests/core/result_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `success_result_with_value` | 成功结果存储值 |
| `error_result_with_context` | 错误结果存储上下文 |
| `error_result_with_code_and_message` | 错误结果包含码和消息 |
| `move_constructor_transfers_value` | 移动构造函数转移值 |
| `and_then_chains_success` | and_then 链式成功处理 |
| `and_then_preserves_error` | and_then 保留错误并短路 |
| `or_else_handles_error` | or_else 处理错误并恢复 |
| `or_else_preserves_success` | or_else 保留成功 |
| `void_success_result` | void 特化成功结果 |
| `void_error_result` | void 特化错误结果 |
| `void_and_then_chains_success` | void 特化 and_then |
| `void_or_else_handles_error` | void 特化 or_else |
| `lvalue_and_then_works` | 左值引用 and_then |
| `lvalue_or_else_works` | 左值引用 or_else |

---

## error_exception_t

**头文件**：`error_system/core/error_exception.h`

基于 `std::exception` 的异常封装类，用于在需要异常机制的场景中传递错误上下文。

### 方法

| 方法 | 返回值 | 描述 |
|------|--------|------|
| `error_exception_t(error_context_t context)` | — | 从错误上下文构造异常（显式构造，移动语义） |
| `what()` | `const char*` | 返回完整的错误详情字符串（缓存，noexcept） |
| `context()` | `const error_context_t&` | 获取原始错误上下文（noexcept） |
| `code()` | `error_code_t` | 获取原始错误码（noexcept） |

### 示例

```cpp
error_context_t ctx(code, "操作失败");
throw error_exception_t(std::move(ctx));

try {
    // ...
} catch (const error_exception_t& e) {
    std::cerr << e.what() << "\n";
    auto code = e.code();
}
```

### 单元测试

测试文件：`tests/core/error_exception_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `exception_stores_context` | 异常存储上下文 |
| `exception_inherits_from_std_exception` | 继承自 std::exception |
| `context_returns_original_context` | 返回原始上下文 |
| `move_context_into_exception` | 移动上下文到异常 |
| `noexcept_constructor` | 构造函数为 noexcept |

---

## error_registry_t

**头文件**：`error_system/core/error_registry.h`

错误码注册表，用于注册和查询错误码的元信息，支持按模块组索引。

### 元数据结构

```cpp
struct error_metadata_t {
    std::string name;         // 宏名称 (例: "ERR_TCP_TIMEOUT")
    std::string description;  // 中文描述 (例: "TCP 连接超时")
    uint16_t module_id;       // 所属模块 ID
    uint16_t error_number;    // 局部错误编号
    error_level_t level;      // 错误等级
};
```

### 方法

```cpp
// 获取单例
static error_registry_t& instance() noexcept;

// 注册/注销错误码
void register_error(error_code_t code, std::string_view name, std::string_view description) noexcept;
void register_errors(const std::vector<error_code_t>& codes,
                     const std::vector<std::string_view>& names,
                     const std::vector<std::string_view>& descriptions) noexcept;
void unregister_error(error_code_t code) noexcept;
void unregister_error(std::string_view name) noexcept;
void unregister_module(module_group_id_t module_group_id) noexcept;
void unregister_all() noexcept;

// 查询错误码信息
bool is_registered(error_code_t code) const noexcept;
std::optional<error_metadata_t> get_info(error_code_t code) const noexcept;
std::vector<error_metadata_t> get_errors_by_module(module_group_id_t module_group_id) const noexcept;

// 模板版本：通过枚举获取模块下的所有错误码
template <typename SystemEnum, typename SubSystemEnum, typename ModuleEnum>
std::vector<error_metadata_t> get_errors_by_module(SystemEnum system,
                                                    SubSystemEnum subsystem,
                                                    ModuleEnum module) const noexcept;
```

### 使用示例

```cpp
// 注册错误码元信息
error_registry_t::instance().register_error(
    db_conn_err, "DB_CONNECTION_TIMEOUT", "数据库连接超时");

// 批量注册
error_registry_t::instance().register_errors(
    {code1, code2}, {"NAME1", "NAME2"}, {"DESC1", "DESC2"});

// 查询错误码信息
auto info = error_registry_t::instance().get_info(db_conn_err);
if (info) {
    std::cout << "Name: " << info->name << "\n";
    std::cout << "Description: " << info->description << "\n";
}

// 检查错误码是否已注册
if (error_registry_t::instance().is_registered(db_conn_err)) {
    // 错误码已注册
}

// 按模块查询
auto errors = error_registry_t::instance().get_errors_by_module(
    domain::system_domain_t::database, 1, 1);

// 注销错误码
error_registry_t::instance().unregister_error(db_conn_err);

// 注销整个模块的所有错误码
error_registry_t::instance().unregister_module(module_group_id);

// 注销所有错误码
error_registry_t::instance().unregister_all();
```

### 单元测试

测试文件：`tests/core/error_registry_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `register_and_retrieve_error` | 注册和查询错误码 |
| `register_multiple_errors` | 批量注册错误码 |
| `unregister_error_by_code` | 通过码注销错误 |
| `unregister_error_by_name` | 通过名称注销错误 |
| `unregister_module_group` | 注销模块组所有错误 |
| `get_info_returns_nullopt_for_unregistered` | 未注册返回 nullopt |
| `get_errors_by_module` | 按模块查询错误 |
| `singleton_returns_same_instance` | 单例返回相同实例 |
| `register_duplicate_keeps_first_metadata` | 重复注册保留首次元数据 |

### 宏注册（推荐）

使用 `DEFINE_ERROR_CODE` 宏可以在定义错误码的同时自动注册到错误码注册表：

```cpp
// 在头文件或源文件中定义错误码
DEFINE_ERROR_CODE(
    ERR_DB_CONNECTION_TIMEOUT,                    // 错误码名称
    error_system::core::error_level_t::error,     // 错误等级
    error_system::domain::system_domain_t::database, // 系统域
    1,                                            // 子系统 ID
    1,                                            // 模块 ID
    0x0001,                                       // 错误编号
    "数据库连接超时");                              // 描述

// 宏会自动在静态初始化阶段注册错误码
```

**宏参数说明**：

| 参数 | 类型 | 说明 |
|------|------|------|
| `NAME` | 标识符 | 错误码宏名称，将创建同名 `constexpr` 常量 |
| `LEVEL` | `error_level_t` | 错误等级（`debug`/`info`/`warn`/`error`/`fatal`） |
| `SYSTEM` | `system_domain_t` | 系统域枚举值 |
| `SUBSYS` | `uint16_t` | 子系统 ID（1-65535） |
| `MODULE` | `uint16_t` | 模块 ID（1-65535） |
| `NUMBER` | `uint16_t` | 错误编号（1-65535） |
| `DESC` | 字符串 | 错误描述，将自动注册到注册表 |

**使用建议**：
- 在项目的错误码定义头文件中使用此宏集中定义所有错误码
- 宏创建的常量可以在编译期使用（`constexpr`）
- 宏自动处理注册，避免手动注册遗漏

### 代码生成工具

系统提供 Python 脚本从 JSON 配置自动生成错误码定义：

**JSON 配置示例** (`config/errors/trade_service_errors.json`):

```json
{
    "namespace": "biz::trade_errors",
    "service_name": "trade",
    "domain": "application",
    "subsystem_id": 101,
    "modules": {
        "order": { "id": 1, "desc": "订单模块" },
        "payment": { "id": 2, "desc": "支付模块" }
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

**生成命令**:

```bash
python script/script_py/generate_error_codes.py \
    config/errors/trade_service_errors.json \
    include/generated/
```

生成的头文件可以直接在业务代码中使用：

```cpp
#include "generated/trade_service_errors.h"

using namespace biz::trade_errors;

// 使用自动生成的错误码
error_context_t ctx(ERR_ORDER_NOT_FOUND);
```

---

## 测试总结

Core 层共包含 **11 个测试文件**，**80+ 个测试用例**，覆盖：

| 模块 | 测试文件 | 测试数量 |
|------|----------|----------|
| error_code | `tests/core/error_code_test.cc` | 7 |
| error_level | `tests/core/error_level_test.cc` | 10 |
| error_builder | `tests/core/error_builder_test.cc` | 7 |
| error_context | 通过 error_registry/result/exception 测试间接覆盖 | - |
| result | `tests/core/result_test.cc` | 14 |
| error_exception | `tests/core/error_exception_test.cc` | 5 |
| error_registry | `tests/core/error_registry_test.cc` | 9 |
| error_config | `tests/config/error_config_test.cc` | 7 |

**运行测试**:

```bash
cd build
ctest --output-on-failure
```
