# 🧩 Core 层 API

命名空间 `error_system::core`。

---

## 🔢 error_code_t

64 位错误码 — 位移 + 掩码实现字段解析，100% 避免严格别名与位域 UB。

### 位域

| 位域 | 位数 | 说明 |
|------|:---:|------|
| Sign | 1 | `0` = 错误 &nbsp; `1` = 成功 |
| Reserved | 3 | bit0 `retryable` · bit1 `transient` · bit2 预留 |
| Level | 4 | debug · info · warn · error · fatal |
| System Domain | 8 | 6 大系统域 |
| Subsystem | 16 | 子系统 ID |
| Module | 16 | 模块 ID |
| Number | 16 | 错误编号 |

默认构造为成功码（sign=1，其余字段为 0），可作为函数成功返回值的零成本默认。

### API

```cpp
// 命名空间级别名
using code_t = uint64_t;
using module_group_id_t = uint64_t;

class error_code_t {
public:
    constexpr error_code_t() noexcept;                        // sign=1 的成功码
    constexpr explicit error_code_t(code_t code) noexcept;
    constexpr error_code_t(error_level_t level, domain::system_domain_t system,
                           subsystem_id_t subsystem, module_id_t module,
                           error_number_t number) noexcept;
    // 构造/析构/拷贝/移动：Rule of 5，全部 noexcept = default

    static constexpr error_code_t make_success() noexcept;

    // retryable / transient 语义（Reserved.bit0 / bit1）
    constexpr bool is_retryable() const noexcept;
    constexpr bool is_transient() const noexcept;
    constexpr void set_retryable(bool) noexcept;
    constexpr void set_transient(bool) noexcept;

    // 字段解析
    constexpr error_level_t get_level() const noexcept;
    constexpr domain::system_domain_t get_system() const noexcept;
    constexpr uint16_t get_subsys() const noexcept;
    constexpr uint16_t get_module() const noexcept;
    constexpr uint16_t get_number() const noexcept;
    constexpr code_t get_code() const noexcept;              // 原始 64 位码
    constexpr code_t get_identity_code() const noexcept;      // 去除 sign/reserved 的业务标识
    constexpr module_group_id_t get_module_group_id() const noexcept;

    // 谓词
    constexpr bool is_error_code() const noexcept;
    constexpr bool is_success_code() const noexcept;

    explicit constexpr operator code_t() const noexcept;     // 显式转 64 位原始码
    constexpr bool operator==(const error_code_t&) const noexcept;  // 另有 != / <
};
```

💡 `subsystem_id_t` / `module_id_t` / `error_number_t` 是强类型包装（`struct { uint16_t value; }`，构造函数为 `explicit`），防止 subsystem/module ID 传反。

**构造方式**

```cpp
error_code_t code(error_level_t::error, system_domain_t::database,
                  subsystem_id_t{1}, module_id_t{2}, error_number_t{0x0010});  // 五参
error_code_t code3(0x8000000001010001ULL);                                       // 原始值
DEFINE_ERROR_CODE(ERR_DB_FAIL, error_level_t::error, system_domain_t::database,  // 宏
    1, 1, 0x0010, "数据库操作失败", "数据库服务", "连接管理");
auto ok = error_code_t::make_success();                                          // 成功码工厂
auto restored = error_builder_t::from_raw(raw_code);                             // 反序列化
```

---

## 🏷️ error_level_t

错误等级强类型枚举及配套 constexpr 转换函数。

```cpp
enum class error_level_t : uint8_t {
    debug = 0, info = 1, warn = 2, error = 3, fatal = 4
};

[[nodiscard]] constexpr uint8_t to_int(error_level_t level) noexcept;
[[nodiscard]] constexpr bool is_valid(uint8_t level) noexcept;
[[nodiscard]] constexpr error_level_t from_int(uint8_t level) noexcept;          // 非法值回退 fatal
[[nodiscard]] constexpr const char* to_string(error_level_t level) noexcept;     // 非法值返回 "unknown"
[[nodiscard]] constexpr error_level_t from_string(const char* str) noexcept;     // 未知返回 info
[[nodiscard]] constexpr error_level_t next_level(error_level_t level) noexcept;  // 越界回退 fatal
[[nodiscard]] constexpr error_level_t prev_level(error_level_t level) noexcept;
[[nodiscard]] constexpr bool should_log(error_level_t current, error_level_t min_level) noexcept;
```

全部函数 `constexpr noexcept`，可用于编译期常量与日志过滤模板参数。

---

## 🏗️ error_builder_t

纯静态工具类（构造/拷贝/移动全部 `= delete`）— 保留两个有独特价值的工厂方法，其他场景使用 `error_code_t` 便捷构造。

```cpp
class error_builder_t {
public:
    // 模板枚举版本：编译期类型安全，防止 subsystem/module ID 传反
    template <typename SubSystemEnum, typename ModuleEnum>
    [[nodiscard]] static constexpr error_code_t make_error_code(
        error_level_t level, domain::system_domain_t system,
        SubSystemEnum subsystem, ModuleEnum module, uint16_t number) noexcept;

    // 从 64 位原始码恢复（语义明确表达"反序列化"意图）
    [[nodiscard]] static constexpr error_code_t from_raw(code_t code) noexcept;
};
```

```cpp
enum class subsys_t : uint16_t { db_conn = 1 };
enum class module_t : uint16_t { timeout = 2 };
auto code = error_builder_t::make_error_code(
    error_level_t::error, system_domain_t::database,
    subsys_t::db_conn, module_t::timeout, 0x0001);  // 编译期类型安全
error_code_t restored = error_builder_t::from_raw(recv_from_network());  // 反序列化
```

---

## 📦 error_context_t

错误上下文数据类 — 错误码 + 消息 + 负载 + 因果链 + 堆栈 + 源位置。序列化职责委托给 `error_context_serializer_t`，运行时特性初始化委托给 `error_context_initializer_t`，遵循单一职责原则。

### API

```cpp
struct error_context_t {
    static constexpr size_t PAYLOAD_SSO_CAPACITY = 4;  // SSO 上限

    // 公共数据成员（默认初始化）
    std::string message{};
    utils::source_location_t source_location{};  // 由 LOCATION_ENABLED 控制
    const char* file_name{nullptr};               // 由 LOCATION_ENABLED 控制
    std::shared_ptr<error_context_t> cause{nullptr};
    std::vector<std::string> stack_frames{};      // 由 STACKTRACE_ENABLED 控制

    // 构造（located_code_t 隐式捕获源位置）
    template <typename... Args>
    error_context_t(located_code_t lc, std::string message_format, Args&&... args) noexcept;
    constexpr error_context_t() noexcept = default;
    // 拷贝构造深拷贝（bad_alloc 内部捕获）；移动构造显式清零源对象
    // 拷贝赋值 = delete；移动赋值 noexcept，允许复用变量，自赋值安全
    // 另有 from_exception() 工厂、get_code()/is_success()/is_error()/what() 等访问器

    // 负载（链式 API，SSO 优化：≤4 项零堆分配）
    // 支持 string/string_view/const char*/string&& 及模板版本（int/bool/double 等）
    error_context_t& with(const std::string& key, const std::string& value) noexcept;
    template <typename T>
    error_context_t& with(const std::string& key, T value) noexcept;
    error_context_t& with_batch(
        std::initializer_list<std::pair<const std::string, std::string>> items) noexcept;

    // 负载查询
    std::vector<std::pair<std::string, std::string>> get_payload() const noexcept;
    std::optional<std::string> get_payload_value(const std::string& key) const noexcept;
    size_t payload_size() const noexcept;
    bool is_payload_empty() const noexcept;
    template <typename Visitor>
    void for_each_payload(Visitor&& visitor) const noexcept;

    // 谓词（委托 error_code_t 对应位）
    bool is_fatal() const noexcept;      // level == fatal
    bool is_retryable() const noexcept;  // error_code_t Reserved.bit0
    bool is_transient() const noexcept;  // error_code_t Reserved.bit1

    // 序列化便捷方法（委托 error_context_serializer_t，免 include serializer 头文件）
    [[nodiscard]] std::string to_string() const noexcept;  // 可读文本
    [[nodiscard]] std::string to_json() const noexcept;    // JSON
    [[nodiscard]] std::string to_binary() const noexcept;  // 紧凑二进制

    // 因果链（返回包含 cause 的新对象）
    [[nodiscard]] error_context_t wrap(const error_context_t& underlying) const noexcept;
    [[nodiscard]] error_context_t wrap(error_context_t&& underlying) const noexcept;

    bool equals_by_code(const error_context_t&) const noexcept;  // O(1)，仅按错误码
    bool equals_strict(const error_context_t&) const noexcept;   // 含 cause 链 + stack_frames
    // 另有 operator==/!=（按 code/message/payload 比较）
};

// 错误聚合：批量校验场景累积多个错误后一次返回，主错误取首个，其余以 joined_error_N 为键附加
[[nodiscard]] error_context_t join_errors(std::vector<error_context_t>&& errors) noexcept;
```

所有方法均为 `noexcept`。`std::bad_alloc` 等异常在内部 `try-catch` 捕获并记录到 `stderr`，对象保持可安全析构状态。

```cpp
error_context_t ctx(ERR_DB_TIMEOUT, "连接超时: {}ms", 3000);
ctx.with("host", "192.168.1.1").with("port", 3306);
auto chained = biz_error.wrap(db_error);     // 因果链
auto uid = ctx.get_payload_value("host");    // std::optional
ctx.for_each_payload([](const auto& k, const auto& v) { std::cout << k << "=" << v; });
if (a.equals_by_code(b))  { /* 错误归类 */ }
if (ctx.is_retryable())   { /* 重试逻辑 */ }
if (ctx.is_fatal())       { /* 告警/熔断 */ }
// 批量校验聚合
std::vector<error_context_t> errs;
if (!validate_a()) errs.push_back(err_a);
if (!validate_b()) errs.push_back(err_b);
return join_errors(std::move(errs));
```

---

## 🔄 error_context_serializer_t

错误上下文序列化器 — 纯静态工具类（构造/析构/拷贝/移动全部 `= delete`），提供文本 / JSON / 二进制双向转换。

```cpp
class error_context_serializer_t {
public:
    static constexpr uint32_t BINARY_MAGIC = 0x52455345u;  // "ESER" 小端
    static constexpr uint8_t  BINARY_VERSION = 1;

    // 编码
    [[nodiscard]] static std::string to_string(const error_context_t& ctx) noexcept;  // 人类可读文本（含 ↳ Caused by:）
    [[nodiscard]] static std::string to_json(const error_context_t& ctx) noexcept;    // JSON（含 cause 递归字段）
    [[nodiscard]] static std::string to_binary(const error_context_t& ctx) noexcept;  // 紧凑二进制（小端序）

    // 解码（任何格式错误或分配失败均返回 std::nullopt，不抛异常）
    [[nodiscard]] static std::optional<error_context_t> from_binary(std::string_view data) noexcept;
    [[nodiscard]] static std::optional<error_context_t> from_json(std::string_view json) noexcept;

    // 注入自定义子系统/模块名称解析器（nullptr 重置为默认 catalog）
    static void set_subsystem_module_resolver(const i18n::i_subsystem_module_resolver_t* resolver) noexcept;
};
```

反序列化的文件名与函数名由 `error_context_t` 内部字符串存储持有，保证 `file_name` 与 `source_location` 中 `const char*` 的生命周期安全。

**输出示例（`to_string`）**

```
[Location: demo.cc:42 @ my_function]
[Sign: Error Level: error, System: application, 交易服务 / 订单模块]
Code: 1 (ERR_ORDER_NOT_FOUND) - 订单创建失败: 订单不存在或已删除
  [Stacktrace]: #0 my_app 0x0000... my_function(int) + 200  |  #1 my_app 0x0000... main + 100
↳ Caused by: [Location: ...] ... 支付服务调用失败 ...
```

**输出示例（`to_json`）**

```json
{
    "location": {"file": "demo.cc", "function": "my_function", "line": 42},
    "code": 217299115812388865, "message": "订单查询失败",
    "payload": {"user_id": "8848", "order_id": "ORD-001"},
    "cause": { ... }
}
```

---

## 📋 error_registry_t

错误码注册表单例 — 重复处理委托 `duplicate_policy_handler_t`，子系统/模块名称委托 `subsystem_module_catalog_t`。

### API

```cpp
class error_registry_t {
public:
    static error_registry_t& instance() noexcept;

    // 单个/批量注册（批量返回成功数量；数组长度不一致返回 0 且不执行注册）
    void register_error(error_code_t code, std::string_view name,
                        std::string_view description) noexcept;
    [[nodiscard]] size_t register_errors(const std::vector<error_code_t>& codes,
                           const std::vector<std::string_view>& names,
                           const std::vector<std::string_view>& descriptions) noexcept;

    // 查询（返回值副本 / optional，避免悬垂指针）
    [[nodiscard]] bool is_registered(error_code_t code) const noexcept;
    [[nodiscard]] std::optional<error_metadata_t> get_info(error_code_t code) const noexcept;
    [[nodiscard]] std::vector<error_metadata_t> get_errors_by_subsystem(uint16_t subsys_id) const noexcept;
    [[nodiscard]] std::vector<error_metadata_t> get_errors_by_module(module_group_id_t) const noexcept;
    [[nodiscard]] std::optional<error_code_t> find_by_name(std::string_view name) const noexcept;

    // 热路径缓存查询（thread_local 环形缓存，纪元失效）
    [[nodiscard]] std::optional<error_metadata_t> get_info_cached(error_code_t code) const noexcept;
    [[nodiscard]] uint64_t get_epoch() const noexcept;
    void invalidate_metadata_cache() const noexcept;

    // 注销系列：unregister_error(code/name) / unregister_module() / unregister_all()

    // 重复策略
    void set_duplicate_policy(duplicate_policy_t) noexcept;
    duplicate_policy_t get_duplicate_policy() const noexcept;
    void set_duplicate_warn_callback(duplicate_warn_callback_t callback) noexcept;  // nullptr 清除
    const duplicate_warn_callback_t& get_duplicate_warn_callback() const noexcept;
};
```

💡 子系统/模块名称注册与查询已迁移至 [`subsystem_module_catalog_t`](i18n.md#subsystem_module_catalog_t)，`error_registry_t` 不再保留相关方法。

### `get_info()` vs `get_info_cached()`

| 路径 | 锁开销 | 一致性 | 适用 |
|------|:---:|:---:|------|
| `get_info()` | `shared_lock` | 强一致 | 管理工具、偶尔查询 |
| `get_info_cached()` | 命中时零锁 | 纪元失效后最终一致 | `error_context_t` 构造、`fill_validation_fields_` 等热路径 |

`get_info_cached()` 使用 thread_local 环形缓存（容量 16），命中时零锁开销；任何 `register_*` / `unregister_*` 调用会 `bump_epoch_()`（release 序），缓存检测到纪元变化（acquire 序）后整体失效重建，同时记录"已注册"与"未注册"结果避免重复加锁查询。查询路径选择决策树详见 [决策树 · 2](../decision_tree.md#2-错误码元数据查询路径选择)。

---

## 🎯 result_t\<T\>

类 Rust Result，零异常错误传递。内部使用 `std::variant<T, error_context_t>` + `std::get_if` + 哨兵值，永不抛异常。

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
    // 拷贝/移动构造的 noexcept 性跟随 T；拷贝赋值 = delete；移动赋值 = default

    // 工厂（推荐）
    [[nodiscard]] static result_t make_success(T value) noexcept;
    [[nodiscard]] static result_t make_error(error_code_t code, const std::string& message = "",
                               utils::source_location_t = current()) noexcept;
    [[nodiscard]] static result_t make_error(error_code_t code, std::string&& message,
                               utils::source_location_t = current()) noexcept;
    [[nodiscard]] static result_t make_error(const error_context_t& ctx) noexcept;

    // 状态
    [[nodiscard]] bool is_success() const noexcept;
    [[nodiscard]] bool is_error() const noexcept;
    explicit operator bool() const noexcept;

    // 值访问（零异常，失败返回哨兵）
    // value() 含 static_assert 要求 T 可默认构造，否则使用 value_pointer() / operator->
    [[nodiscard]] const T& value() const noexcept;            // 失败返回 thread_local T{} 哨兵
    [[nodiscard]] const T* value_pointer() const noexcept;    // 失败返回 nullptr
    [[nodiscard]] const T& value_or(const T& default_value) const noexcept;
    [[nodiscard]] const T& operator*() const noexcept;        // 等价 value()
    [[nodiscard]] const T* operator->() const noexcept;       // 等价 value_pointer()
    // 上述 value/value_pointer/operator*/operator-> 均另有无 const 重载

    // 错误访问（含 mutable 重载 error() noexcept）
    [[nodiscard]] const error_context_t& error() const noexcept;

    // 链式操作（全部 noexcept，用户函数异常被 try-catch 保护并转为 fatal 错误）
    // and_then 有 & / const& / && 三个重载；map / map_error 有 const& / && 两个；or_else 有 & / && 两个
    template <typename Function> [[nodiscard]] auto and_then(Function&& fn) const& noexcept;
    template <typename Function> [[nodiscard]] auto and_then(Function&& fn) && noexcept;
    template <typename Function> [[nodiscard]] result_t or_else(Function&& fn) & noexcept;
    template <typename Function> [[nodiscard]] result_t or_else(Function&& fn) && noexcept;
    template <typename Function> [[nodiscard]] auto map(Function&& fn) const& noexcept;
    template <typename Function> [[nodiscard]] auto map(Function&& fn) && noexcept;
    template <typename Function> [[nodiscard]] result_t map_error(Function&& fn) const& noexcept;
    template <typename Function> [[nodiscard]] result_t map_error(Function&& fn) && noexcept;

    // 传播时附加上下文（完美转发到 with()）
    // 错误时追加 payload，成功时无操作；& 返回自身引用，&& 返回移动后对象
    template <typename K, typename V> result_t& context(K&& key, V&& value) & noexcept;
    template <typename K, typename V> [[nodiscard]] result_t context(K&& key, V&& value) && noexcept;

    // 模式匹配（条件 noexcept：仅当两个回调均 noexcept 时才 noexcept）
    template <typename SuccessFn, typename ErrorFn>
    [[nodiscard]] auto match(SuccessFn&&, ErrorFn&&) const;
};
```

```cpp
auto r = fetch()
    .map([](auto& d) { return d.name; })
    .map_error([](const auto& e) { return recover(e); });
if (auto* p = r.value_pointer()) { /* 安全使用，不要求 T 可默认构造 */ }
auto msg = r.match(  // 强制同时处理两条路径
    [](const std::string& s) { return "ok: " + s; },
    [](const error_context_t& e) { return "fail: " + std::string(e.message); });
// 传播时附加 payload
return inner_call().context("host", host_).context("port", port_);
```

`value()` 含 `static_assert` 要求 T 可默认构造，否则编译失败 — 此时改用 `value_pointer()` 或 `operator->`。

### `result_t<void>` 特化

仅列与主模板的差异（构造/析构/拷贝/移动 Rule of 5、`is_success`/`is_error`/`operator bool`/`make_error` 系列/`error() const`/`map_error`/`or_else` 均同主模板）：

```cpp
template <>
class result_t<void> {
public:
    using value_type_t = void;

    result_t() noexcept;                      // 成功（持有 monostate，零 error_context_t 构造）
    explicit result_t(const error_context_t&) noexcept;

    [[nodiscard]] static result_t make_success() noexcept;  // 无参（主模板接受 T value）

    // 无 value/value_or/value_pointer/operator*/operator->、无 mutable error() 重载、无 map()、无 match()

    // and_then 仅 & / && 两个重载（无 const&，因 void 无值可读）
    template <typename Function> [[nodiscard]] auto and_then(Function&&) & noexcept;
    template <typename Function> [[nodiscard]] auto and_then(Function&&) && noexcept;

    // 传播时附加上下文（同主模板）
    template <typename K, typename V> result_t<void>& context(K&&, V&&) & noexcept;
    template <typename K, typename V> [[nodiscard]] result_t<void> context(K&&, V&&) && noexcept;
};
```

**惰性上下文设计**：`result_t<void>` 内部用 `std::variant<std::monostate, error_context_t>` 存储。成功路径只持有 `monostate`（trivial 构造），不构造 `error_context_t` 的任何成员，避免成功时无谓地构造 13 个空 `std::string`（payload_small_ + metadata + message 等）。失败路径才构造完整 `error_context_t`。对象 sizeof 与 `error_context_t` 基本持平（424 vs 416）。

```cpp
result_t<void> ok;
result_t<void> fail = result_t<void>::make_error(ERR_FAIL, "失败");
```

---

## 💥 error_exception_t

将 `error_context_t` 封装为可抛出异常，继承 `std::exception`。构造时通过 `error_context_serializer_t::to_string` 缓存错误详情字符串，`what()` 在异常传播期间返回稳定指针。

```cpp
class error_exception_t : public std::exception {
public:
    explicit error_exception_t(error_context_t context) noexcept;
    // 构造/析构/拷贝/移动：Rule of 5，拷贝/移动构造 = default，赋值运算符 = delete

    const char* what() const noexcept override;              // 返回缓存的完整错误详情
    [[nodiscard]] const error_context_t& context() const noexcept;
    [[nodiscard]] error_code_t code() const noexcept;
};
```

---

## 🏷️ DEFINE_ERROR_CODE

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

利用 C++ 静态初始化在 `main()` 之前完成注册，无需手动调用。`constexpr error_code_t` 常量为编译期决议，无 SIOF 风险；但请勿在其它 TU 的静态初始化代码中查询注册表（跨 TU 动态初始化顺序未指定）。

---

## 编译期配置

编译期特性开关（`ERROR_SYSTEM_ENABLE_STACKTRACE` / `ERROR_SYSTEM_ENABLE_VALIDATION` / `ERROR_SYSTEM_ENABLE_LOCATION` 等 CMake 选项）通过 `feature_flags_t` 的 `public constexpr bool` 暴露，`error_context_t` 内部使用 `if constexpr` 替代 `#ifdef`，由编译器死代码消除未启用分支。完整 CMake 选项列表与设计说明详见 [架构设计](../architecture.md) 的「编译配置」与「编译期特性开关 + 死代码消除」节。
