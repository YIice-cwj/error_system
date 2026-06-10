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
    std::shared_ptr<error_context_t> cause;

    error_context_t() = default;

    // 模板构造函数：接受 code_with_location_t + 格式化字符串
    template <typename... Args>
    error_context_t(code_with_location_t code_with, std::string message_format = "", Args&&... args) noexcept;

    // 链式调用添加 payload
    error_context_t& with(const std::string& key, const std::string& value) noexcept;
    error_context_t& with(std::string&& key, std::string&& value) noexcept;
    error_context_t& with(const char* key, const char* value) noexcept;

    // 错误包装（因果链）
    error_context_t wrap(const error_context_t& underlying) const noexcept;
    error_context_t wrap(error_context_t&& underlying) const noexcept;

    bool is_success() const noexcept;
    bool is_error() const noexcept;

    std::string to_string() const noexcept;
    std::string to_json() const noexcept;
    std::string to_binary() const noexcept;

    const std::unordered_map<std::string, std::string>& get_payload() const noexcept;
    const char* what() const noexcept;
};
```

### 构造与基本使用

```cpp
auto code = error_builder_t::make_error_code(
    error_level_t::error,
    system_domain_t::database,
    1, 1, 1
);

// 模板构造：code_with_location_t + 格式化字符串
using error_system::core::code_with_location_t;
error_context_t ctx(code_with_location_t(code), "数据库连接超时");

// 或使用格式化参数
error_context_t ctx2(code_with_location_t(code), "连接 {} 超时，耗时 {}ms", "DB", 5000);

// 链式添加结构化负载
ctx.with("host", "192.168.1.1")
   .with("port", "3306")
   .with("timeout_ms", "5000");
```

### 因果链（错误包装）

```cpp
// 底层错误
auto db_code = error_builder_t::make_error_code(
    error_level_t::error, system_domain_t::database, 1, 1, 1
);
error_context_t db_error(code_with_location_t(db_code), "连接超时");

// 业务层包装
auto biz_code = error_builder_t::make_error_code(
    error_level_t::error, system_domain_t::application, 2, 1, 1
);
error_context_t biz_error = db_error.wrap(
    error_context_t(code_with_location_t(biz_code), "订单查询失败")
);

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
| `tests/core/error_context_test.cc` | 4 | 构造、payload、序列化 |

---

## error_registry_t

错误码注册表，支持按码快速查找、按名注销、按模块组索引和批量注册。

```cpp
class error_registry_t {
public:
    static error_registry_t& instance() noexcept;

    void register_error(const error_code_t code, std::string_view name,
                        std::string_view description) noexcept;

    size_t register_errors(const std::vector<error_code_t>& codes,
                           const std::vector<std::string_view>& names,
                           const std::vector<std::string_view>& descriptions) noexcept;

    void unregister_error(const error_code_t code) noexcept;
    void unregister_error(std::string_view name) noexcept;
    void unregister_module(module_group_id_t module_group_id) noexcept;
    void unregister_all() noexcept;

    bool is_registered(const error_code_t code) const noexcept;

    std::optional<std::reference_wrapper<const error_metadata_t>>
    get_info(const error_code_t code) const noexcept;

    std::vector<std::reference_wrapper<const error_metadata_t>>
    get_errors_by_module(module_group_id_t module_group_id) const noexcept;

    void register_subsystem_module(uint16_t subsys_id, uint16_t module_id,
                                   std::string_view subsystem_name,
                                   std::string_view module_name) noexcept;

    void set_duplicate_policy(duplicate_policy_t policy) noexcept;
    duplicate_policy_t get_duplicate_policy() const noexcept;
    void set_duplicate_warn_callback(std::function<void(code_t, const error_metadata_t&)> callback) noexcept;
};
```

### 使用示例

```cpp
auto& registry = error_registry_t::instance();

// 注册错误码
registry.register_error(
    ERR_DB_CONNECTION_TIMEOUT,
    "ERR_DB_CONNECTION_TIMEOUT",
    "数据库连接超时"
);

// 批量注册
registry.register_errors({code1, code2}, {"E1", "E2"}, {"Desc1", "Desc2"});

// 查询
if (registry.is_registered(code)) {
    auto info = registry.get_info(code);
    info->get().name;        // "ERR_DB_CONNECTION_TIMEOUT"
    info->get().description; // "数据库连接超时"
}

// 按名注销（O(1)）
registry.unregister_error("ERR_DB_CONNECTION_TIMEOUT");

// 按模块组查询
auto errors = registry.get_errors_by_module(code.get_module_group_id());

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
| `tests/core/error_registry_test.cc` | 22 | 注册、查询、注销、批量注册、重复策略、并发、name_index |

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
        return result_t<int>(error_context_t(code, "除数不能为零"));
    }
    return result_t<int>(a / b);
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

// 内联静态初始化时自动注册
inline const error_registrar_t ERR_DB_CONNECTION_TIMEOUT_registrar_(
    ERR_DB_CONNECTION_TIMEOUT, "ERR_DB_CONNECTION_TIMEOUT", "数据库连接超时");
```

---

## 编译期配置

| CMake 选项 | 默认值 | 影响 |
|------------|--------|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | `ON` | 堆栈追踪功能 |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | `ON` | 错误码验证 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | `ON` | 源位置追踪 |

关闭后相关 API 标记为 `[[deprecated]]` 并返回默认值。
