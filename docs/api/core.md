# Core 层 API

> `error_system::core`

---

## error_code_t

64 位错误码 — 位移 + 掩码实现字段解析。

### 位域

| 位域 | 位数 | 说明 |
|------|:---:|------|
| Sign | 1 | `0` = 错误 &nbsp; `1` = 成功 |
| Reserved | 3 | 保留 |
| Level | 4 | debug · info · warn · error · fatal |
| System Domain | 8 | 6 大系统域 |
| Subsystem | 16 | 子系统 ID |
| Module | 16 | 模块 ID |
| Number | 16 | 错误编号 |

### API

```cpp
class error_code_t {
public:
    using code_t = uint64_t;

    constexpr error_code_t() noexcept = default;
    constexpr explicit error_code_t(code_t code) noexcept;

    // 五参便捷构造（推荐）
    constexpr error_code_t(error_level_t level, domain::system_domain_t system,
                           uint16_t subsys, uint16_t module, uint16_t number) noexcept;

    // 成功码工厂
    static constexpr error_code_t make_success() noexcept;

    constexpr code_t get_code() const noexcept;
    constexpr uint8_t get_sign() const noexcept;
    constexpr uint8_t get_reserved() const noexcept;
    constexpr void set_sign(uint8_t sign) noexcept;
    constexpr void set_reserved(uint8_t reserved) noexcept;
    constexpr bool is_error_code() const noexcept;
    constexpr bool is_success_code() const noexcept;
    constexpr error_level_t get_level() const noexcept;
    constexpr domain::system_domain_t get_system() const noexcept;
    constexpr uint16_t get_subsys() const noexcept;
    constexpr uint16_t get_module() const noexcept;
    constexpr uint16_t get_number() const noexcept;
    constexpr module_group_id_t get_module_group_id() const noexcept;

    // 恒等码（去除 sign/reserved 后的业务标识）
    constexpr code_t get_identity_code() const noexcept;

    constexpr bool operator==(const error_code_t&) const noexcept;
    constexpr bool operator!=(const error_code_t&) const noexcept;
    constexpr bool operator<(const error_code_t&) const noexcept;
};
```

**构造方式**

```cpp
// 1. 五参构造（推荐，constexpr）
error_code_t code(error_level_t::error, system_domain_t::database, 1, 2, 0x0010);

// 2. 原始值
error_code_t code3(0x8000000001010001ULL);

// 3. DEFINE_ERROR_CODE 宏（自动注册，见下方）
DEFINE_ERROR_CODE(ERR_DB_FAIL,
    error_level_t::error, system_domain_t::database,
    1, 1, 0x0010, "数据库操作失败", "数据库服务", "连接管理");

// 4. 成功码工厂
auto ok = error_code_t::make_success();
```

---

## error_level_t

```cpp
enum class error_level_t : uint8_t {
    debug = 0, info = 1, warn = 2, error = 3, fatal = 4
};

constexpr const char* to_string(error_level_t) noexcept;
constexpr error_level_t from_string(const char*) noexcept;
```

---

## error_builder_t

编译期构建工厂（v2.2 兼容层）。

> 推荐直接使用 `error_code_t` 五参构造。

```cpp
class error_builder_t {
public:
    static constexpr error_code_t make_error_code(
        error_level_t, domain::system_domain_t,
        uint16_t subsystem, uint16_t module, uint16_t number) noexcept;
};
```

---

## error_context_t

错误上下文数据类 — 错误码 + 消息 + 负载 + 因果链 + 堆栈 + 源位置。

> 序列化职责委托给 `error_context_serializer_t`，运行时特性初始化委托给 `error_context_initializer_t`，遵循单一职责原则。

### 公共数据成员

```cpp
struct error_context_t {
    std::string message{};
    utils::source_location_t source_location{};  // 由 LOCATION_ENABLED 控制
    const char* file_name{nullptr};               // 由 LOCATION_ENABLED 控制
    std::shared_ptr<error_context_t> cause{nullptr};
    std::vector<std::string> stack_frames{};      // 由 STACKTRACE_ENABLED 控制
    // ...
};
```

### API

```cpp
struct error_context_t {
    // 构造（位于_code_t 隐式捕获源位置）
    template <typename... Args>
    error_context_t(located_code_t lc, std::string message_format, Args&&... args) noexcept;

    constexpr error_context_t() noexcept = default;
    error_context_t(const error_context_t&) noexcept;             // 深拷贝，bad_alloc 内部捕获
    error_context_t(error_context_t&&) noexcept;                  // 显式清零源对象
    error_context_t& operator=(const error_context_t&) = delete;
    error_context_t& operator=(error_context_t&&) = delete;
    ~error_context_t() noexcept = default;

    // 工厂
    static error_context_t from_exception(error_code_t code,
                                          const std::exception& ex) noexcept;

    // 只读访问错误码
    const error_code_t& get_code() const noexcept;

    // 负载（链式 API，SSO 优化：≤4 项零堆分配）
    error_context_t& with(const std::string& key, const std::string& value) noexcept;
    error_context_t& with(const char* key, const char* value) noexcept;
    error_context_t& with(std::string_view key, std::string_view value) noexcept;
    error_context_t& with(std::string&& key, std::string&& value) noexcept;
    template <typename T>
    error_context_t& with(const std::string& key, T value) noexcept;  // int/bool/double 等
    error_context_t& with(std::string_view key, T value) noexcept;
    error_context_t& with(const char* key, T value) noexcept;
    error_context_t& with_batch(
        std::initializer_list<std::pair<const std::string, std::string>> items) noexcept;

    // 负载查询
    std::vector<std::pair<std::string, std::string>> get_payload() const noexcept;
    std::optional<std::string> get_payload_value(const std::string& key) const noexcept;
    size_t payload_size() const noexcept;
    bool is_payload_empty() const noexcept;
    template <typename Visitor>
    void for_each_payload(Visitor&& visitor) const noexcept;

    // 因果链（返回包含 cause 的新对象）
    error_context_t wrap(const error_context_t& underlying) const noexcept;
    error_context_t wrap(error_context_t&& underlying) const noexcept;

    // 状态
    bool is_success() const noexcept;
    bool is_error() const noexcept;

    // 比较（O(n) 复杂度，使用 get_payload_value 查找）
    bool operator==(const error_context_t&) const noexcept;
    bool operator!=(const error_context_t&) const noexcept;
};
```

> 所有方法均为 `noexcept`。`std::bad_alloc` 等异常在内部 `try-catch` 捕获并记录到 `stderr`，对象保持可安全析构状态。

**使用示例**

```cpp
error_context_t ctx(ERR_DB_TIMEOUT, "连接超时: {}ms", 3000);

ctx.with("host", "192.168.1.1")
   .with("port", 3306)
   .with("latency_ms", 150.5);

auto chained = biz_error.wrap(db_error);     // 因果链
auto uid = ctx.get_payload_value("host");    // std::optional
size_t n = ctx.payload_size();               // 3
ctx.for_each_payload([](const auto& k, const auto& v) {
    std::cout << k << "=" << v << "\n";
});
```

---

## error_context_serializer_t

错误上下文序列化器（静态工具类，禁止实例化）。

```cpp
class error_context_serializer_t {
public:
    error_context_serializer_t() = delete;

    // 人类可读文本（含因果链 ↳ Caused by:）
    static std::string to_string(const error_context_t& ctx) noexcept;

    // JSON 字符串（含 cause 递归字段）
    static std::string to_json(const error_context_t& ctx) noexcept;

    // 紧凑二进制（小端序，含因果链，适合 RPC / 持久化）
    static std::string to_binary(const error_context_t& ctx) noexcept;
};
```

**输出示例（`to_string`）**

```
[Location: demo.cc:42 @ my_function]
[Sign: Error Level: error, System: application, 交易服务 / 订单模块]
Code: 1 (ERR_ORDER_NOT_FOUND) - 订单创建失败: 订单不存在或已删除
  [Stacktrace]:
    #0  my_app  0x0000...  my_function(int) + 200
    #1  my_app  0x0000...  main + 100
↳ Caused by: [Location: ...] ... 支付服务调用失败 ...
```

**输出示例（`to_json`）**

```json
{
    "location": {"file": "demo.cc", "function": "my_function", "line": 42},
    "code": 217299115812388865,
    "message": "订单查询失败",
    "payload": {"user_id": "8848", "order_id": "ORD-001"},
    "cause": { ... }
}
```

---

## error_registry_t

错误码注册表单例。

### API

```cpp
class error_registry_t {
public:
    static error_registry_t& instance() noexcept;

    // 注册
    void register_error(error_code_t code, std::string_view name,
                        std::string_view description) noexcept;
    void register_subsystem_module(uint16_t subsys, uint16_t module,
                                   std::string_view subsys_name,
                                   std::string_view module_name) noexcept;

    // 查询（返回值副本 / optional，避免悬垂指针）
    bool is_registered(error_code_t code) const noexcept;
    std::optional<error_metadata_t> get_info(error_code_t code) const noexcept;
    std::vector<error_metadata_t> get_errors_by_subsystem(uint16_t subsys_id) const noexcept;
    std::vector<error_metadata_t> get_errors_by_module(module_group_id_t) const noexcept;
    std::optional<error_code_t> find_by_name(std::string_view name) const noexcept;

    // 注销
    void unregister_error(error_code_t code) noexcept;
    void unregister_error(std::string_view name) noexcept;
    void unregister_module(module_group_id_t) noexcept;
    void unregister_all() noexcept;

    // 重复策略
    void set_duplicate_policy(duplicate_policy_t) noexcept;
    void set_duplicate_warn_callback(
        std::function<void(code_t, const error_metadata_t&)>) noexcept;
};
```

> 查询接口返回 `std::optional` 或值副本（`std::vector`），避免 unregister 后调用方持有悬垂指针。

---

## result_t\<T\>

类 Rust Result，零异常错误传递。

> 内部使用 `std::variant<T, error_context_t>` + `std::get_if` + 哨兵值，永不抛异常。

### API

```cpp
template<typename T>
class result_t {
public:
    using value_type_t = T;

    // 构造（推荐使用工厂方法 make_success / make_error）
    explicit result_t(const T& value) noexcept;
    explicit result_t(T&& value) noexcept;
    explicit result_t(const error_context_t& ctx) noexcept;

    // 拷贝/移动构造的 noexcept 性跟随 T
    result_t(const result_t&) noexcept(std::is_nothrow_copy_constructible_v<T>
                                       && std::is_nothrow_copy_constructible_v<error_context_t>);
    result_t(result_t&&) noexcept(std::is_nothrow_move_constructible_v<T>);
    result_t& operator=(const result_t&) = delete;
    result_t& operator=(result_t&&) = delete;

    // 工厂（推荐）
    static result_t make_success(T value) noexcept;
    static result_t make_error(error_code_t code,
                               const std::string& message = "",
                               utils::source_location_t = current()) noexcept;
    static result_t make_error(error_code_t code,
                               std::string&& message,
                               utils::source_location_t = current()) noexcept;
    static result_t make_error(const error_context_t& ctx) noexcept;

    // 状态
    bool is_success() const noexcept;
    bool is_error() const noexcept;
    explicit operator bool() const noexcept;

    // 值访问（零异常，失败返回哨兵）
    const T& value() const noexcept;            // 失败返回 thread_local T{} 哨兵
    T& value() noexcept;
    const T* value_pointer() const noexcept;    // 失败返回 nullptr
    T* value_pointer() noexcept;
    const T& value_or(const T& default_value) const noexcept;
    const T& expect() const noexcept;           // Debug 断言，Release 哨兵
    T unwrap() const noexcept;                  // 失败返回 T{}
    T unwrap_or(T default_value) const noexcept;

    // 错误访问
    const error_context_t& error() const noexcept;
    error_context_t& error() noexcept;

    // 链式操作（全部 noexcept，用户函数异常被 try-catch 保护并转为 fatal 错误）
    template <typename Function>
    auto and_then(Function&& fn) & noexcept;
    template <typename Function>
    auto and_then(Function&& fn) const& noexcept;
    template <typename Function>
    auto and_then(Function&& fn) && noexcept;
    template <typename Function>
    result_t or_else(Function&& fn) & noexcept;
    template <typename Function>
    result_t or_else(Function&& fn) && noexcept;
    template <typename Function>
    auto map(Function&& fn) const& noexcept;
    template <typename Function>
    auto map(Function&& fn) && noexcept;
    template <typename Function>
    result_t map_error(Function&& fn) const& noexcept;
    template <typename Function>
    result_t map_error(Function&& fn) && noexcept;

    // 模式匹配（不标记 noexcept：用户回调异常会传播给调用方）
    template <typename SuccessFn, typename ErrorFn>
    auto match(SuccessFn&&, ErrorFn&&) const;
};

// void 特化
template <>
class result_t<void> {
public:
    result_t() noexcept;  // 成功
    explicit result_t(const error_context_t&) noexcept;

    static result_t make_success() noexcept;
    static result_t make_error(error_code_t, const std::string& message = "",
                               utils::source_location_t = current()) noexcept;
    static result_t make_error(const error_context_t&) noexcept;

    bool is_success() const noexcept;
    bool is_error() const noexcept;
    explicit operator bool() const noexcept;
    void expect() const noexcept;  // Debug 断言

    const error_context_t& error() const noexcept;

    // 链式操作（同上，特化版本）
    template <typename Function> auto and_then(Function&&) & noexcept;
    template <typename Function> auto and_then(Function&&) && noexcept;
    template <typename Function> result_t map_error(Function&&) const& noexcept;
    template <typename Function> result_t map_error(Function&&) && noexcept;
    template <typename Function> result_t or_else(Function&&) & noexcept;
    template <typename Function> result_t or_else(Function&&) && noexcept;
};
```

**使用示例**

```cpp
auto r = fetch()
    .map([](auto& d) { return d.name; })
    .map_error([](const auto& e) { return recover(e); });

if (r) { auto v = r.value(); }
if (auto* p = r.value_pointer()) { /* 安全使用 */ }
int v = r.unwrap_or(0);

// match：强制同时处理两条路径
auto msg = r.match(
    [](const std::string& s) { return "ok: " + s; },
    [](const error_context_t& e) { return "fail: " + std::string(e.message); });

// void 特化
result_t<void> ok;
result_t<void> fail = result_t<void>::make_error(ERR_FAIL, "失败");
```

---

## error_exception_t

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

## DEFINE_ERROR_CODE

定义 `constexpr` 错误码常量并自动注册到 `error_registry_t`。

```cpp
DEFINE_ERROR_CODE(
    NAME,         // 错误码宏名（如 ERR_DB_FAIL）
    LEVEL,        // error_level_t::xxx
    SYSTEM,       // system_domain_t::xxx
    SUBSYS,       // 子系统 ID (uint16_t)
    MODULE,       // 模块 ID (uint16_t)
    NUMBER,       // 错误编号 (uint16_t)
    DESC,         // 错误描述（const char*）
    SUBSYS_NAME,  // 子系统名称（const char*）
    MODULE_NAME   // 模块名称（const char*）
);
```

> 利用 C++ 静态初始化在 `main()` 之前完成注册，无需手动调用。

---

## 编译期配置

| CMake 选项 | 默认 | 影响 |
|------------|:---:|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | ON | 堆栈追踪 + per-code API |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | ON | 错误码注册校验 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | ON | 源位置追踪 |

> 编译期宏通过 `feature_flags_t::STACKTRACE_ENABLED` 等 `public constexpr bool` 暴露，`error_context_t` 内部使用 `if constexpr` 替代 `#ifdef`，编译器死代码消除未启用分支。
