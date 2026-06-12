# Core 层 API 参考

> 命名空间: `error_system::core`

---

## error_code_t

64 位错误码，使用位移 + 掩码实现字段解析，100% 避免位域未定义行为。

### 位域划分

| 位域 | 位数 | 说明 |
|------|------|------|
| Sign | 1 bit | 符号位（0=错误，1=成功） |
| Reserved | 3 bits | 保留位 |
| Level | 4 bits | 错误等级（debug/info/warn/error/fatal） |
| System Domain | 8 bits | 系统域（none/system/middleware/database/application/third_party） |
| Subsystem | 16 bits | 子系统 ID |
| Module | 16 bits | 模块 ID |
| Number | 16 bits | 错误编号 |

### 核心 API

```cpp
class error_code_t {
public:
    using code_t = uint64_t;

    constexpr error_code_t() noexcept = default;
    constexpr explicit error_code_t(code_t code) noexcept;

    // 便捷构造函数（v2.0 新增，替代 error_builder_t::make_error_code）
    constexpr error_code_t(error_level_t level, domain::system_domain_t system,
                           uint16_t subsys, uint16_t module, uint16_t number) noexcept;

    constexpr code_t get_code() const noexcept;
    constexpr uint8_t get_sign() const noexcept;
    constexpr error_level_t get_level() const noexcept;
    constexpr domain::system_domain_t get_system() const noexcept;
    constexpr uint16_t get_subsys() const noexcept;
    constexpr uint16_t get_module() const noexcept;
    constexpr uint16_t get_number() const noexcept;
    constexpr module_group_id_t get_module_group_id() const noexcept;
    constexpr code_t get_identity_code() const noexcept;

    constexpr bool operator==(const error_code_t& other) const noexcept;
    constexpr bool operator!=(const error_code_t& other) const noexcept;
    constexpr bool operator<(const error_code_t& other) const noexcept;
};
```

### 构造方式

```cpp
// 方式 1：便捷构造函数（推荐，5参数）
error_code_t code(error_level_t::error, system_domain_t::database, 1, 2, 0x0010);

// 方式 2：从原始 64 位值构造
error_code_t code3(0x8000000001010001ULL);

// 方式 3：使用 DEFINE_ERROR_CODE 宏（自动注册）
DEFINE_ERROR_CODE(ERR_DB_FAIL,
    error_level_t::error, system_domain_t::database,
    1, 1, 0x0010, "数据库操作失败",
    "数据库服务", "连接管理");
```

### 字段解析

```cpp
auto level  = code.get_level();     // error_level_t::error
auto system = code.get_system();    // system_domain_t::database
auto subsys = code.get_subsys();    // 1
auto module = code.get_module();    // 2
auto number = code.get_number();    // 0x0010

// 模块组 ID（用于按模块查询注册表）
auto mgid = code.get_module_group_id();

// 身份码（清除 sign + reserved 位，用于注册表索引）
auto id = code.get_identity_code();
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/error_code_test.cc` | 17+ | 构造、5 参便捷构造、constexpr 5 参构造、字段解析、比较、哈希 |

---

## error_level_t

错误等级枚举。

```cpp
enum class error_level_t : uint8_t {
    debug = 0,
    info  = 1,
    warn  = 2,
    error = 3,
    fatal = 4,
};
```

### 转换函数

```cpp
constexpr const char* to_string(error_level_t level) noexcept;
constexpr error_level_t from_string(const char* string) noexcept;
```

---

## error_builder_t

编译期错误码构建工厂，所有方法均为 `constexpr`。

```cpp
class error_builder_t {
public:
    static constexpr error_code_t make_error_code(
        error_level_t level, domain::system_domain_t system,
        uint16_t subsystem, uint16_t module, uint16_t number) noexcept;
};
```

> **注意**: 自 v2.0 起，推荐使用 `error_code_t` 的五参数便捷构造函数作为更简洁的替代。

---

## error_context_t

错误上下文，包含错误码、消息、因果链、结构化负载、堆栈跟踪和源位置。

构造时自动根据全局配置完成：错误码校验 → 堆栈捕获 → 源位置记录 → 插件通知。

### 核心 API

```cpp
struct error_context_t {
    error_code_t code{};
    std::string message{};
    std::unordered_map<std::string, std::string> payload{};
    std::shared_ptr<error_context_t> cause{nullptr};

    constexpr error_context_t() noexcept = default;

    // 模板构造函数（v2.0 简化：直接接受 error_code_t，无需 code_with_location_t）
    template <typename... Args>
    error_context_t(error_code_t code, std::string message_format = "", Args&&... args) noexcept;

    // 链式添加 payload（v2.0 支持多类型值）
    error_context_t& with(const std::string& key, const std::string& value) noexcept;
    error_context_t& with(const char* key, const char* value) noexcept;
    error_context_t& with(std::string_view key, std::string_view value) noexcept;
    error_context_t& with(std::string&& key, std::string&& value) noexcept;

    // 模板版本：自动转换 int、bool、double、float 等算术类型
    template <typename T>
    error_context_t& with(const std::string& key, T value) noexcept;

    // with_batch: 批量添加 payload（v2.1 新增）
    error_context_t& with_batch(
        std::initializer_list<std::pair<const std::string, std::string>> items) noexcept;

    // compare operators (v2.1)
    bool operator==(const error_context_t& other) const noexcept;
    bool operator!=(const error_context_t& other) const noexcept;

    // 因果链
    error_context_t wrap(const error_context_t& underlying) const noexcept;
    error_context_t wrap(error_context_t&& underlying) const noexcept;

    bool is_success() const noexcept;
    bool is_error() const noexcept;

    std::string to_string() const noexcept;
    std::string to_json() const noexcept;   // code 字段使用 identity_code，含因果链递归输出
    std::string to_binary() const noexcept; // v2.1 新增因果链标志位 + 递归序列化

    const std::unordered_map<std::string, std::string>& get_payload() const noexcept;
    const char* what() const noexcept;
};
```

### 构造与基本使用

```cpp
// v2.0 方式：直接传入 error_code_t
error_context_t ctx(ERR_DB_TIMEOUT, "连接超时: {}ms", 3000);

// 链式添加多类型 payload
ctx.with("host", "192.168.1.1")
   .with("port", 3306)           // int → "3306"
   .with("retry", true)          // bool → "true"
   .with("latency_ms", 150.5);   // double → "150.500000"
```

### 因果链

```cpp
// 底层错误
error_context_t db_error(ERR_DB_TIMEOUT, "数据库连接超时");

// 业务层包装
error_context_t biz_error(ERR_ORDER_FAIL, "订单查询失败");
auto chained = biz_error.wrap(db_error);
// chained.cause 指向 db_error
```

### 序列化

```cpp
// 人类可读（含子系统/模块名称）
std::string text = ctx.to_string();

// JSON 序列化
std::string json = ctx.to_json();
// {"code":0x8...","message":"...","payload":{"host":"..."},"cause":{...}}

// 二进制序列化（高性能 RPC / 持久化）
std::string binary = ctx.to_binary();
```

### 自动堆栈跟踪

当 `ERROR_SYSTEM_ENABLE_STACKTRACE` 开启时，构造函数根据等级自动捕获调用栈：

```cpp
// 全局阈值
error_config_t::set_stacktrace_level(error_level_t::warn);

// per-code 覆盖（v2.0 新增）
error_config_t::set_per_code_stacktrace_level(
    ERR_CRITICAL.get_identity_code(), error_level_t::debug);
// ERR_CRITICAL 总是捕获堆栈，其他错误码仍使用全局阈值
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/error_context_test.cc` | 7 | 构造、多类型 payload、序列化（含 to_binary 因果链）、with_batch、compare operators、含子系统/模块名称输出 |

---

## error_registry_t

错误码注册表单例，支持按码/按名查询、按子系统/模块组索引、批量注册和并发安全。

### 核心 API

```cpp
class error_registry_t {
public:
    static error_registry_t& instance() noexcept;

    // 注册
    void register_error(const error_code_t code, std::string_view name,
                        std::string_view description) noexcept;
    size_t register_errors(const std::vector<error_code_t>& codes,
                           const std::vector<std::string_view>& names,
                           const std::vector<std::string_view>& descriptions) noexcept;
    void register_subsystem_module(uint16_t subsys_id, uint16_t module_id,
                                   std::string_view subsystem_name,
                                   std::string_view module_name) noexcept;

    // 查询
    bool is_registered(const error_code_t code) const noexcept;

    // v2.0: 返回裸指针，nullptr = 未注册
    const error_metadata_t* get_info(const error_code_t code) const noexcept;

    // v2.0 新增：按子系统 ID 查询该子系统下所有错误码
    std::vector<std::reference_wrapper<const error_metadata_t>>
    get_errors_by_subsystem(uint16_t subsys_id) const noexcept; // v2.1 subsystem_index_ O(1) 索引优化

    std::vector<std::reference_wrapper<const error_metadata_t>>
    get_errors_by_module(module_group_id_t module_group_id) const noexcept;

    // v2.0 新增：按名称查找错误码
    std::optional<error_code_t> find_by_name(const std::string_view name) const noexcept;

    // 注销
    void unregister_error(const error_code_t code) noexcept;
    void unregister_error(std::string_view name) noexcept;
    void unregister_module(module_group_id_t module_group_id) noexcept;
    void unregister_all() noexcept;

    // 重复策略
    void set_duplicate_policy(duplicate_policy_t policy) noexcept;
    duplicate_policy_t get_duplicate_policy() const noexcept;
    void set_duplicate_warn_callback(std::function<void(code_t, const error_metadata_t&)> cb) noexcept;
};
```

### 使用示例

```cpp
auto& registry = error_registry_t::instance();

// 查询元数据（v2.0：指针语义）
const error_metadata_t* info = registry.get_info(code);
if (info != nullptr) {
    std::cout << info->name << ": " << info->description << "\n";
}

// 按子系统查询
auto trade_errors = registry.get_errors_by_subsystem(1);
for (const auto& ref : trade_errors) {
    std::cout << ref.get().name << "\n";
}

// 按名称查找
auto found = registry.find_by_name("ERR_DB_CONNECTION_TIMEOUT");
if (found.has_value()) {
    error_code_t code = found.value();
}
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/error_registry_test.cc` | 26 | 注册、查询、get_errors_by_subsystem（含 subsystem_index_ 清理）、find_by_name、并发安全 |

---

## result_t\<T\>

类 Rust Result，零异常错误传递。内部使用 `std::variant<T, error_context_t>`，全部 `std::get` 已替换为 `std::get_if` + 静态哨兵值，确保永不抛异常。

### 核心 API

```cpp
template<typename T>
class result_t {
public:
    // 成功构造
    explicit result_t(const T& value) noexcept;

    // 错误构造（接受 error_context_t）
    explicit result_t(const error_context_t& ctx) noexcept;

    // 工厂函数
    static result_t make_error(error_code_t code, const std::string& message = "") noexcept;
    static result_t make_error(error_code_t code, std::string&& message) noexcept;
    static result_t make_error(const error_context_t& ctx) noexcept;

    // 状态检查
    bool is_success() const noexcept;
    bool is_error() const noexcept;
    explicit operator bool() const noexcept;

    // 值访问（v2.1 零异常：所有方法失败返回哨兵/指针，不抛异常）
    T& value();                             // 失败返回静态哨兵 T{}，要求 T 可默认构造
    const T& value() const;
    T* value_pointer();                     // 失败返回 nullptr
    const T* value_pointer() const;
    T value_or(T default_value) const;      // 失败返回默认值
    const T& expect(const char* msg) const; // Debug 断言失败 + Release 返回哨兵

    // 错误访问（v2.1 零异常：成功时返回空 error_context_t 哨兵）
    error_context_t& error();
    const error_context_t& error() const;

    // 链式操作（v2.1 全部基于 std::get_if，零异常）
    result_t and_then(std::function<result_t(T)> fn) &&;
    result_t or_else(std::function<result_t(const error_context_t&)> fn) &&;
    result_t and_then(std::function<result_t(T)> fn) const&;
    result_t or_else(std::function<result_t(const error_context_t&)> fn) const&;
    result_t map(std::function<auto(T)->U> fn) &&;                    // v2.1 成功值类型转换
    result_t map(std::function<auto(T)->U> fn) const&;
    result_t map_error(std::function<auto(const error_context_t)->ER> fn) &&;  // v2.1 错误类型转换
    result_t map_error(std::function<auto(const error_context_t)->ER> fn) const&;
};
```

### 使用示例

```cpp
// 成功
result_t<int> result(42);

// 错误（v2.0 推荐工厂函数）
return result_t<int>::make_error(ERR_DB_FAIL, "数据库操作失败");

// 或直接传 error_context_t
error_context_t ctx(ERR_DB_FAIL, "失败");
return result_t<int>(ctx);

// 链式操作（v2.1 新增 map/map_error）
auto final = fetch()
    .map([](auto& d) { return d.name; })           // 成功值类型转换
    .map_error([](const auto& e) { return process(e); });  // 错误类型转换

// 状态检查（v2.1 operator bool）
if (result) {
    int value = result.value();
}

// 断言获取（v2.1 expect）
int value = result.expect("should never fail here");

// 安全获取（v2.1 value_pointer）
if (auto* ptr = result.value_pointer()) {
    // 安全使用 *ptr，无需检查 is_success()
}

// void 特化
result_t<void> ok;
result_t<void> fail(error_context_t(ERR_FAIL, "失败"));
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/core/result_test.cc` | 20+ | 构造、make_error、expect、value()/error() 哨兵、value_pointer、map/map_error、链式操作、void 特化 |

---

## error_exception_t

基于 `std::exception` 的异常封装。

```cpp
class error_exception_t : public std::exception {
public:
    explicit error_exception_t(error_context_t ctx) noexcept;
    const char* what() const noexcept override;
    const error_context_t& context() const noexcept;
    error_code_t code() const noexcept;
};
```

---

## DEFINE_ERROR_CODE 宏

在定义错误码的同时自动注册到错误码注册表，同步注册子系统/模块名称。

```cpp
// v2.0 签名（新增 SUBSYS_NAME 和 MODULE_NAME 参数）
DEFINE_ERROR_CODE(
    NAME,          // 错误码宏名
    LEVEL,         // error_level_t
    SYSTEM,        // system_domain_t
    SUBSYS,        // 子系统 ID
    MODULE,        // 模块 ID
    NUMBER,        // 错误编号
    DESC,          // 中文描述
    SUBSYS_NAME,   // 子系统名称（to_string() 可读输出）
    MODULE_NAME    // 模块名称（to_string() 可读输出）
);
```

### 使用示例

```cpp
DEFINE_ERROR_CODE(
    ERR_DB_CONNECTION_TIMEOUT,
    error_system::core::error_level_t::error,
    error_system::domain::system_domain_t::database,
    1, 1, 0x0001,
    "数据库连接超时",
    "数据库服务",
    "连接管理"
);

// to_string() 输出：[ERROR][数据库服务][连接管理][1] 数据库连接超时
```

宏展开后等价于：

```cpp
constexpr error_code_t ERR_DB_CONNECTION_TIMEOUT =
    error_builder_t::make_error_code(
        error_level_t::error, system_domain_t::database, 1, 1, 0x0001);

inline const error_registrar_t ERR_DB_CONNECTION_TIMEOUT_registrar_(
    ERR_DB_CONNECTION_TIMEOUT, "ERR_DB_CONNECTION_TIMEOUT",
    "数据库连接超时", "数据库服务", "连接管理");
```

---

## 编译期配置

| CMake 选项 | 默认值 | 影响 |
|------------|--------|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | `ON` | 堆栈追踪 + per-code 堆栈等级覆盖 |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | `ON` | 错误码注册表校验 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | `ON` | 源文件/函数/行号追踪 |