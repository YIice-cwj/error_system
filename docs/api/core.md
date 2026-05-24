# Core 层 API 参考

> 命名空间: `error_system::core`

---

## error_code_t

64 位错误码，使用位移 + 掩码实现字段解析，100% 避免位域未定义行为。

### 位域划分

| 位域 | 位数 | 说明 |
|------|------|------|
| Sign | 1 bit | 符号位（0=正常，1=错误） |
| Reserved | 3 bits | 保留位 |
| Level | 4 bits | 错误等级（debug/info/warn/error/fatal） |
| System Domain | 8 bits | 系统域（none/system/middleware/database/application/third_party） |
| Subsystem | 8 bits | 子系统 ID |
| Module | 8 bits | 模块 ID |
| Number | 32 bits | 错误码编号 |

### 核心 API

```cpp
class error_code_t {
public:
    using code_t = uint64_t;

    constexpr error_code_t() noexcept = default;
    explicit constexpr error_code_t(code_t code) noexcept;

    constexpr bool is_error() const noexcept;
    constexpr error_level_t get_level() const noexcept;
    constexpr uint8_t get_domain() const noexcept;
    constexpr uint8_t get_subsystem() const noexcept;
    constexpr uint8_t get_module() const noexcept;
    constexpr uint32_t get_number() const noexcept;

    constexpr code_t get_raw_code() const noexcept;
    constexpr bool operator==(const error_code_t& other) const noexcept;
    constexpr bool operator!=(const error_code_t& other) const noexcept;
    constexpr bool operator<(const error_code_t& other) const noexcept;
};
```

### 构造与解析

```cpp
// 从原始 64 位值构造
error_code_t code(0x8000000001010001ULL);

// 使用 builder 构造（推荐）
auto code = error_builder_t::make_error_code(
    error_level_t::error,
    system_domain_t::database,
    1,   // subsystem
    1,   // module
    1    // number
);

// 字段解析
auto level = code.get_level();      // error_level_t::error
auto domain = code.get_domain();    // 4 (database)
auto subsys = code.get_subsystem(); // 1
auto module = code.get_module();    // 1
auto number = code.get_number();    // 1
```

### 比较与哈希

```cpp
error_code_t a = ...;
error_code_t b = ...;

if (a == b) { /* ... */ }
if (a < b)  { /* 用于 map/set 排序 */ }

// 哈希支持
std::unordered_map<error_code_t, std::string> dict;
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/error_code_test.cc` | 15+ | 构造、字段解析、比较、哈希、constexpr |

---

## error_level_t

错误等级枚举。

```cpp
enum class error_level_t : uint8_t {
    none = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
    fatal = 5
};
```

### 转换函数

```cpp
constexpr const char* to_string(error_level_t level) noexcept;
constexpr std::optional<error_level_t> from_string(std::string_view str) noexcept;
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/error_level_test.cc` | 8 | 枚举值、字符串转换、constexpr |

---

## error_builder_t

编译期错误码构建工厂，所有方法均为 `constexpr`。

```cpp
class error_builder_t {
public:
    static constexpr error_code_t make_error_code(
        error_level_t level,
        uint8_t domain,
        uint8_t subsystem,
        uint8_t module,
        uint32_t number) noexcept;

    static constexpr error_code_t make_error_code(
        error_level_t level,
        system_domain_t domain,
        uint8_t subsystem,
        uint8_t module,
        uint32_t number) noexcept;
};
```

### 使用示例

```cpp
// 使用预定义域
auto code = error_builder_t::make_error_code(
    error_level_t::error,
    system_domain_t::database,
    1, 1, 0x0001
);

// 使用原始域 ID
auto code2 = error_builder_t::make_error_code(
    error_level_t::warn,
    5,   // domain
    2,   // subsystem
    3,   // module
    100  // number
);
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/error_builder_test.cc` | 6 | 编译期构建、字段验证 |

---

## error_context_t

错误上下文，包含错误码、消息、因果链、结构化负载、堆栈跟踪和源位置。

```cpp
class error_context_t {
public:
    error_code_t code;
    std::string message;
    std::unordered_map<std::string, std::string> payload;
    std::vector<std::string> stack_frames;
    utils::source_location_t source_location;
    std::shared_ptr<error_context_t> cause;

    error_context_t() = default;
    explicit error_context_t(error_code_t code, std::string message = "");

    error_context_t wrap(error_code_t new_code, std::string new_message) const;

    std::string to_string() const;
    std::string to_json() const;
    std::string to_binary() const;

    bool has_cause() const noexcept;
    std::string get_full_message() const;
    std::string get_stack_trace() const;
};
```

### 构造与基本使用

```cpp
auto code = error_builder_t::make_error_code(
    error_level_t::error,
    system_domain_t::database,
    1, 1, 1
);

// 基础构造
error_context_t ctx(code, "数据库连接超时");

// 添加结构化负载
ctx.payload["host"] = "192.168.1.1";
ctx.payload["port"] = "3306";
ctx.payload["timeout_ms"] = "5000";
```

### 因果链（错误包装）

```cpp
// 底层错误
auto db_code = error_builder_t::make_error_code(
    error_level_t::error, system_domain_t::database, 1, 1, 1
);
error_context_t db_error(db_code, "连接超时");

// 业务层包装
auto biz_code = error_builder_t::make_error_code(
    error_level_t::error, system_domain_t::application, 2, 1, 1
);
error_context_t biz_error = db_error.wrap(biz_code, "订单查询失败");

// 结果：biz_error.cause -> db_error
```

### 序列化

```cpp
// JSON 序列化
std::string json = ctx.to_json();
// 输出示例：
// {
//   "code": 9223372036854775809,
//   "message": "数据库连接超时",
//   "payload": {"host": "192.168.1.1", "port": "3306"},
//   "stack_frames": [...],
//   "cause": {...}
// }

// 二进制序列化
std::string binary = ctx.to_binary();
// 适合高性能 RPC 或持久化存储
```

### 字符串输出

```cpp
std::cout << ctx << std::endl;
// 输出格式：[ERROR][database][1][1][1] 数据库连接超时
//          at main (main.cc:42)
//          Stack: ...
```

### 自动堆栈跟踪

当 `config::error_config_t::is_stacktrace_enabled()` 为 `true` 且错误等级 >= `get_stacktrace_level()` 时，构造函数自动捕获调用栈：

```cpp
// 设置 WARN 及以上自动捕获
config::error_config_t::set_stacktrace_level(error_level_t::warn);

// 此错误会自动捕获堆栈
error_context_t warn_ctx(warn_code, "警告信息");

// DEBUG 级别不会捕获
error_context_t debug_ctx(debug_code, "调试信息");
```

### 自动源位置

当 `ERROR_SYSTEM_ENABLE_LOCATION` 开启时，构造函数自动记录源文件位置：

```cpp
error_context_t ctx(code, "错误信息");
// ctx.source_location.file_name = "main.cc"
// ctx.source_location.function_name = "main"
// ctx.source_location.line_number = 42
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/error_context_test.cc` | 20+ | 构造、包装、序列化、堆栈、源位置 |

---

## error_registry_t

错误码注册表，支持按模块组索引和错误码验证。

```cpp
class error_registry_t {
public:
    static error_registry_t& instance() noexcept;

    void register_error(error_code_t code, std::string_view name,
                        std::string_view desc, std::string_view module_group);

    bool is_registered(error_code_t code) const noexcept;
    std::optional<std::string> get_name(error_code_t code) const;
    std::optional<std::string> get_description(error_code_t code) const;
    std::optional<std::string> get_module_group(error_code_t code) const;

    std::vector<error_code_t> get_codes_by_module_group(std::string_view group) const;
    std::vector<error_code_t> get_all_codes() const;

    void clear() noexcept;
    size_t size() const noexcept;
};
```

### 使用示例

```cpp
auto& registry = error_registry_t::instance();

// 注册错误码
registry.register_error(
    ERR_DB_CONNECTION_TIMEOUT,
    "ERR_DB_CONNECTION_TIMEOUT",
    "数据库连接超时",
    "database"
);

// 查询
if (registry.is_registered(code)) {
    auto name = registry.get_name(code);        // "ERR_DB_CONNECTION_TIMEOUT"
    auto desc = registry.get_description(code); // "数据库连接超时"
    auto group = registry.get_module_group(code); // "database"
}

// 按模块组查询
auto db_codes = registry.get_codes_by_module_group("database");

// 使用 DEFINE_ERROR_CODE 宏自动注册
DEFINE_ERROR_CODE(
    ERR_DB_CONNECTION_TIMEOUT,
    error_level_t::error,
    system_domain_t::database,
    1, 1, 0x0001,
    "数据库连接超时");
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/error_registry_test.cc` | 10+ | 注册、查询、模块组索引、单例 |

---

## result_t\<T\>

类 Rust Result，用于无异常的错误传递。

```cpp
template<typename T>
class result_t {
public:
    result_t() = default;
    result_t(T value);
    result_t(error_code_t code, std::string message);

    bool is_ok() const noexcept;
    bool is_error() const noexcept;

    T& value();
    const T& value() const;
    error_context_t& error();
    const error_context_t& error() const;

    T& operator*();
    T* operator->();
    explicit operator bool() const noexcept;
};
```

### 使用示例

```cpp
result_t<int> divide(int a, int b) {
    if (b == 0) {
        auto code = error_builder_t::make_error_code(
            error_level_t::error,
            system_domain_t::application,
            1, 1, 1
        );
        return {code, "除数不能为零"};
    }
    return a / b;
}

auto result = divide(10, 0);
if (result.is_error()) {
    std::cerr << result.error().to_string() << std::endl;
} else {
    std::cout << "结果: " << result.value() << std::endl;
}

// 或使用 bool 转换
if (result) {
    std::cout << *result << std::endl;
}
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/result_test.cc` | 12+ | 构造、值访问、错误访问、bool 转换 |

---

## error_exception_t

基于 `std::exception` 的异常封装。

```cpp
class error_exception_t : public std::exception {
public:
    explicit error_exception_t(error_context_t ctx);

    const char* what() const noexcept override;
    const error_context_t& context() const noexcept;
    error_code_t code() const noexcept;
};
```

### 使用示例

```cpp
try {
    auto code = error_builder_t::make_error_code(
        error_level_t::fatal,
        system_domain_t::system,
        1, 1, 1
    );
    throw error_exception_t(error_context_t(code, "系统崩溃"));
} catch (const error_exception_t& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << e.context().to_string() << std::endl;
}
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/error_exception_test.cc` | 6 | 构造、what()、context() 访问 |

---

## DEFINE_ERROR_CODE 宏

在定义错误码的同时自动注册到错误码注册表。

```cpp
DEFINE_ERROR_CODE(
    ERR_DB_CONNECTION_TIMEOUT,                    // 变量名
    error_system::core::error_level_t::error,     // 等级
    error_system::domain::system_domain_t::database, // 域
    1,                                             // 子系统
    1,                                             // 模块
    0x0001,                                        // 编号
    "数据库连接超时"                               // 描述
);
```

宏展开后等价于：

```cpp
inline constexpr error_code_t ERR_DB_CONNECTION_TIMEOUT =
    error_builder_t::make_error_code(
        error_level_t::error,
        system_domain_t::database,
        1, 1, 0x0001
    );

// 静态初始化时自动注册
static struct ERR_DB_CONNECTION_TIMEOUT_registrar {
    ERR_DB_CONNECTION_TIMEOUT_registrar() {
        error_registry_t::instance().register_error(
            ERR_DB_CONNECTION_TIMEOUT,
            "ERR_DB_CONNECTION_TIMEOUT",
            "数据库连接超时",
            "database"
        );
    }
} ERR_DB_CONNECTION_TIMEOUT_registrar_instance;
```

---

## 编译期配置

| CMake 选项 | 默认值 | 影响 |
|------------|--------|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | `ON` | 堆栈追踪功能 |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | `ON` | 错误码验证 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | `ON` | 源位置追踪 |

关闭后相关 API 标记为 `[[deprecated]]` 并返回默认值。
