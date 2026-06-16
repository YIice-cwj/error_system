# 🧩 Core 层 API

> `error_system::core`

---

## 🔢 error_code_t

64 位错误码 — 位移 + 掩码实现字段解析。

### 位域

| 位域 | 位数 | 说明 |
|------|:---:|------|
| Sign | 1 | `0` = 错误 ❌ &nbsp; `1` = 成功 ✅ |
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

    // ⭐ 五参便捷构造（推荐）
    constexpr error_code_t(error_level_t level, domain::system_domain_t system,
                           uint16_t subsys, uint16_t module, uint16_t number) noexcept;

    constexpr code_t get_code() const noexcept;
    constexpr uint8_t get_sign() const noexcept;
    constexpr bool is_error_code() const noexcept;
    constexpr bool is_success_code() const noexcept;
    constexpr error_level_t get_level() const noexcept;
    constexpr domain::system_domain_t get_system() const noexcept;
    constexpr uint16_t get_subsys() const noexcept;
    constexpr uint16_t get_module() const noexcept;
    constexpr uint16_t get_number() const noexcept;
    constexpr module_group_id_t get_module_group_id() const noexcept;
    constexpr code_t get_identity_code() const noexcept;

    constexpr bool operator==(const error_code_t&) const noexcept;
    constexpr bool operator!=(const error_code_t&) const noexcept;
    constexpr bool operator<(const error_code_t&) const noexcept;
};
```

**📝 构造方式**

```cpp
// 1. 五参构造（推荐，constexpr）
error_code_t code(error_level_t::error, system_domain_t::database, 1, 2, 0x0010);

// 2. 原始值
error_code_t code3(0x8000000001010001ULL);

// 3. DEFINE_ERROR_CODE 宏（自动注册）
DEFINE_ERROR_CODE(ERR_DB_FAIL,
    error_level_t::error, system_domain_t::database,
    1, 1, 0x0010, "数据库操作失败", "数据库服务", "连接管理");
```

---

## 📊 error_level_t

```cpp
enum class error_level_t : uint8_t {
    debug = 0, info = 1, warn = 2, error = 3, fatal = 4
};

constexpr const char* to_string(error_level_t) noexcept;
constexpr error_level_t from_string(const char*) noexcept;
```

---

## 🏭 error_builder_t

编译期构建工厂。

> ⚠️ v2.2 标记为兼容层，推荐直接用 `error_code_t` 五参构造。

```cpp
class error_builder_t {
public:
    static constexpr error_code_t make_error_code(
        error_level_t, domain::system_domain_t,
        uint16_t subsystem, uint16_t module, uint16_t number) noexcept;
};
```

---

## 📦 error_context_t

错误上下文 — 错误码 + 消息 + 负载 + 因果链 + 堆栈 + 源位置。

### API

```cpp
struct error_context_t {
    // 📦 公共数据成员（始终存在，if constexpr 控制填充）
    std::string message{};
    std::shared_ptr<error_context_t> cause{nullptr};
    std::vector<std::string> stack_frames{};    // 由 STACKTRACE_ENABLED 控制
    const char* file_name{nullptr};              // 由 LOCATION_ENABLED 控制
    const char* function_name{nullptr};          // 由 LOCATION_ENABLED 控制
    uint32_t line_number{0};                     // 由 LOCATION_ENABLED 控制
    utils::source_location_t source_location_{}; // 由 LOCATION_ENABLED 控制

    // 🔒 只读访问错误码
    const error_code_t& get_code() const noexcept;

    // 🏗️ 构造（v2.3：message 必传）
    template <typename... Args>
    error_context_t(error_code_t code, std::string message_format, Args&&... args) noexcept;

    // 📎 负载
    error_context_t& with(const std::string& key, const std::string& value) noexcept;
    error_context_t& with(const char* key, const char* value) noexcept;
    error_context_t& with(std::string_view key, std::string_view value) noexcept;
    template <typename T>
    error_context_t& with(const std::string& key, T value) noexcept;  // int/bool/double 等
    error_context_t& with_batch(
        std::initializer_list<std::pair<const std::string, std::string>> items) noexcept;

    // 🔍 负载查询
    std::vector<std::pair<std::string, std::string>> get_payload() const noexcept;
    size_t payload_size() const noexcept;
    bool is_payload_empty() const noexcept;
    std::optional<std::string> get_payload_value(const std::string& key) const noexcept;
    template <typename Visitor>
    void for_each_payload(Visitor&& visitor) const noexcept;

    // 🔗 因果链
    error_context_t wrap(const error_context_t& underlying) const noexcept;
    error_context_t wrap(error_context_t&& underlying) const noexcept;

    // ✅ 状态
    bool is_success() const noexcept;
    bool is_error() const noexcept;

    // 📤 序列化
    std::string to_string() const noexcept;   // 人类可读
    std::string to_json() const noexcept;     // JSON（含因果链）
    std::string to_binary() const noexcept;   // 紧凑二进制（含因果链）
    const char* what() const noexcept;        // C 风格描述

    // 🏭 工厂
    static error_context_t from_exception(const error_code_t&,
                                          const std::exception& ex) noexcept;

    bool operator==(const error_context_t&) const noexcept;
    bool operator!=(const error_context_t&) const noexcept;
};
```

> 💡 所有成员始终存在（无需 `#ifdef`），填充由 `if constexpr` + `error_config_t::STACKTRACE_ENABLED` / `LOCATION_ENABLED` 编译期控制，编译器死代码消除未启用分支。

**📝 使用示例**

```cpp
error_context_t ctx(ERR_DB_TIMEOUT, "连接超时: {}ms", 3000);

ctx.with("host", "192.168.1.1")
   .with("port", 3306)
   .with("latency_ms", 150.5);

auto chained = biz_error.wrap(db_error);     // 因果链
auto uid = ctx.get_payload_value("host");    // std::optional
```

**🔍 自动堆栈跟踪**

```cpp
error_config_t::set_stacktrace_level(error_level_t::warn);       // 全局阈值
error_config_t::set_per_code_stacktrace_level(                   // per-code 覆盖
    ERR_CRITICAL.get_identity_code(), error_level_t::debug);
```

---

## 📋 error_registry_t

错误码注册表单例。

### API

```cpp
class error_registry_t {
public:
    static error_registry_t& instance() noexcept;

    // 📝 注册
    void register_error(error_code_t code, std::string_view name,
                        std::string_view description) noexcept;
    void register_subsystem_module(uint16_t subsys, uint16_t module,
                                   std::string_view subsys_name,
                                   std::string_view module_name) noexcept;

    // 🔍 查询（指针语义：nullptr = 未注册）
    bool is_registered(error_code_t code) const noexcept;
    const error_metadata_t* get_info(error_code_t code) const noexcept;
    std::vector<std::reference_wrapper<const error_metadata_t>>
        get_errors_by_subsystem(uint16_t subsys_id) const noexcept;
    std::vector<std::reference_wrapper<const error_metadata_t>>
        get_errors_by_module(module_group_id_t) const noexcept;
    std::optional<error_code_t> find_by_name(std::string_view name) const noexcept;

    // ❌ 注销
    void unregister_error(error_code_t code) noexcept;
    void unregister_error(std::string_view name) noexcept;
    void unregister_module(module_group_id_t) noexcept;
    void unregister_all() noexcept;

    // ⚙️ 重复策略
    void set_duplicate_policy(duplicate_policy_t) noexcept;
    void set_duplicate_warn_callback(std::function<void(code_t, const error_metadata_t&)>) noexcept;
};
```

---

## 🎯 result_t\<T\>

类 Rust Result，零异常错误传递。

> 💡 内部用 `std::get_if` + 哨兵值，永不抛异常。

### API

```cpp
template<typename T>
class result_t {
public:
    explicit result_t(const T& value) noexcept;
    explicit result_t(const error_context_t& ctx) noexcept;

    // 🏭 工厂
    static result_t make_error(error_code_t code, const std::string& msg = "") noexcept;
    static result_t make_error(error_code_t code, std::string&& msg) noexcept;
    static result_t make_error(const error_context_t& ctx) noexcept;
    static result_t make_success(T value) noexcept;

    // ✅ 状态
    bool is_success() const noexcept;
    bool is_error() const noexcept;
    explicit operator bool() const noexcept;

    // 🔓 值访问（零异常）
    T& value();                          // 失败返回 T{} 哨兵
    const T& value() const;
    T* value_pointer();                  // 失败返回 nullptr
    const T* value_pointer() const;
    T value_or(T default_value) const;   // 失败返回默认值
    const T& expect(const char* msg) const;  // Debug 断言，Release 哨兵
    T unwrap() const noexcept;           // 失败返回 T{}

    // 🔓 错误访问（成功返回空哨兵）
    error_context_t& error();
    const error_context_t& error() const;

    // 🔗 链式操作
    result_t and_then(std::function<result_t(T)> fn) &&;
    result_t or_else(std::function<result_t(const error_context_t&)> fn) &&;
    result_t and_then(std::function<result_t(T)> fn) const&;
    result_t or_else(std::function<result_t(const error_context_t&)> fn) const&;
    result_t map(std::function<auto(T)->U> fn) &&;
    result_t map(std::function<auto(T)->U> fn) const&;
    result_t map_error(std::function<auto(const error_context_t)->ER> fn) &&;
    result_t map_error(std::function<auto(const error_context_t)->ER> fn) const&;

    // 🎭 模式匹配
    template <typename SuccessFn, typename ErrorFn>
    auto match(SuccessFn&&, ErrorFn&&) const noexcept;
};
```

**📝 使用示例**

```cpp
auto r = fetch()
    .map([](auto& d) { return d.name; })
    .map_error([](const auto& e) { return recover(e); });

if (r) { auto v = r.value(); }
if (auto* p = r.value_pointer()) { /* 安全使用 */ }
int v = r.expect("should not fail");

// void 特化
result_t<void> ok;
result_t<void> fail(error_context_t(ERR_FAIL, "失败"));
```

---

## 💥 error_exception_t

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

## 📌 DEFINE_ERROR_CODE

定义 + 自动注册错误码。

```cpp
DEFINE_ERROR_CODE(
    NAME, LEVEL, SYSTEM, SUBSYS, MODULE, NUMBER,
    DESC,          // 描述
    SUBSYS_NAME,   // 子系统名称
    MODULE_NAME    // 模块名称
);
```

---

## ⚙️ 编译期配置

| CMake 选项 | 默认 | 影响 |
|------------|:---:|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | ON | 堆栈追踪 + per-code API |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | ON | 错误码注册校验 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | ON | 源位置追踪 |