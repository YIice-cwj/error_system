# Utils 层 API

> `error_system::utils`

---

## async_queue_t\<T, Processor\>

异步工作队列 — 单生产者-单消费者模式。

### API

```cpp
template <typename T, typename Processor>
class async_queue_t {
public:
    explicit async_queue_t(Processor processor) noexcept;

    bool enqueue(T item) noexcept;       // 首次调用自动启动工作线程
    void set_max_size(size_t) noexcept;  // 0 = 无限制
    size_t max_size() const noexcept;
    size_t size() const noexcept;
    bool empty() const noexcept;
};
```

| 特性 | 说明 |
|------|------|
| 自动生命周期 | 首次 `enqueue()` 启动，析构 `join()` |
| 死锁安全 | 先设 `running_=false`，再 `notify_all`，再 `join` |
| 异常隔离 | 处理器异常不退出工作线程 |
| 背压控制 | 队列满拒绝入队 |
| 零 `std::function` | Processor 模版参数，编译期确定调用 |

```cpp
async_queue_t<int, decltype([](int& v) noexcept { process(v); })> queue(
    [](int& v) noexcept { process(v); });
queue.enqueue(42);
queue.set_max_size(1000);
```

---

## string_utils_t

字符串处理工具。

### API

```cpp
class string_utils_t {
public:
    static constexpr uint64_t hash(std::string_view) noexcept;
    static constexpr uint64_t hash_limit(std::string_view, size_t max_len = 128) noexcept;

    template <typename... Args>
    static inline std::string format(std::string_view fmt, Args&&... args) noexcept;

    static std::vector<std::string_view> split(std::string_view, std::string_view delim) noexcept;
    static std::string join(const std::vector<std::string_view>&, std::string_view delim) noexcept;
    static std::string_view trim(std::string_view) noexcept;
    static std::string replace_all(std::string, std::string_view from, std::string_view to) noexcept;

    static constexpr bool starts_with(std::string_view, std::string_view prefix) noexcept;
    static constexpr bool ends_with(std::string_view, std::string_view suffix) noexcept;

    static std::string to_lower(std::string_view) noexcept;
    static std::string to_upper(std::string_view) noexcept;
    static std::string escape_json(std::string_view value) noexcept;

    template <typename T>
    static inline std::optional<T> parse_number(std::string_view) noexcept;
};
```

> `format()` 与 `escape_json()` 均为 `noexcept`，`std::bad_alloc` 内部 `try-catch` 捕获。

**速查**

```cpp
string_utils_t::hash("hello");                            // constexpr FNV-1a
string_utils_t::format("code: {}, msg: {}", 404, "NF");   // "code: 404, msg: NF"
string_utils_t::split("a,b,c", ",");                      // {"a", "b", "c"}
string_utils_t::trim("  hi  ");                           // "hi"
string_utils_t::replace_all("hello world", "world", "universe");
string_utils_t::starts_with("hello", "he");               // true
string_utils_t::parse_number<int>("42");                  // std::optional{42}
string_utils_t::escape_json(R"(a"b\c)");                  // "a\"b\\c"
```

---

## json_dict_t

JSON 解析与点路径访问。

> 简化的 JSON 解析器，仅支持扁平 / 嵌套的字符串键值对。不支持数字、布尔、数组等非字符串值。生产环境如需完整 JSON 解析请使用 nlohmann/json 等第三方库。

### API

```cpp
class json_dict_t {
public:
    std::optional<std::string> operator[](const std::string& key) const noexcept;
    std::optional<std::string> get_value(const std::string& key) const noexcept;
    std::optional<std::string> get_value_or(const std::string& key,
                                           const std::string& default_value) const noexcept;
    bool contains(const std::string& key) const noexcept;
    bool empty() const noexcept;
    size_t size() const noexcept;

    static std::optional<json_dict_t> from_file(const std::filesystem::path&) noexcept;
    static std::optional<json_dict_t> parse(const std::string&) noexcept;
};
```

```cpp
auto dict = json_dict_t::parse(R"({"user":{"name":"Alice"}})");
auto name = dict->get_value("user.name");           // "Alice"
auto code = dict->get_value_or("code", "0");        // "0" (默认)
dict->contains("user.name");                        // true

// 嵌套路径访问（支持多层）
auto deep = json_dict_t::parse(R"({"a":{"b":{"c":"value"}}})");
deep->get_value("a.b.c");                           // "value"
```

---

## json_lexer_t

JSON 词法分析器（内部使用，`json_dict_t` 的实现基础）。

> 支持 RFC 8259 规范的 UTF-16 代理对解析（`\uD83D\uDE00` → 4 字节 UTF-8 编码），孤立代理区码点会被静默丢弃。

---

## file_utils_t

跨平台文件操作。

### API

```cpp
class file_utils_t {
public:
    static std::optional<std::string> read_file(const std::filesystem::path&) noexcept;
    static bool write_file(const std::filesystem::path&, const std::string&) noexcept;
    static bool create_file(const std::filesystem::path&) noexcept;
    static bool delete_file(const std::filesystem::path&) noexcept;
    static bool force_delete_file(const std::filesystem::path&) noexcept;
    static bool file_exists(const std::filesystem::path&) noexcept;
    static bool dir_exists(const std::filesystem::path&) noexcept;
};
```

> `read_file()` 内置文件大小上限保护（`MAX_READ_FILE_SIZE`），防止 OOM 攻击。

```cpp
auto content = file_utils_t::read_file("config.json");
file_utils_t::write_file("output.txt", "Hello!");
file_utils_t::dir_exists("config/");    // true/false
```

---

## stack_trace_utils_t

跨平台堆栈跟踪。

### API

```cpp
class stack_trace_utils_t {
public:
    static std::vector<std::string> generate(int skip_frames = 1,
                                             int max_frames = 16) noexcept;
};
```

| 平台 | 实现 |
|------|------|
| Linux | `backtrace()` / `backtrace_symbols()` |
| macOS | `backtrace()` / `backtrace_symbols()` |
| Windows | `CaptureStackBackTrace()` + `SymFromAddr` |

```cpp
auto frames = stack_trace_utils_t::generate(2, 8);
for (const auto& f : frames) std::cout << f << "\n";
```

---

## source_location_t

源位置封装，`error_context_t` 构造时通过 `located_code_t` 隐式转换自动捕获调用者位置。

```cpp
class source_location_t {
public:
    static source_location_t current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint32_t line = __builtin_LINE()) noexcept;

    const char* file_name() const noexcept;
    const char* function_name() const noexcept;
    uint32_t line() const noexcept;
};
```

> 通过默认参数在调用点展开 `__builtin_FILE()`，捕获的是调用者位置而非库内部位置。

---

## string_format_t

`{}` 占位符格式化工具（`error_context_t` 消息格式化使用）。

```cpp
class string_format_t {
public:
    template <typename... Args>
    static std::string format(std::string fmt, Args&&... args) noexcept;
};
```

```cpp
auto msg = string_format_t::format("用户 {} 登录失败，重试 {} 次", "alice", 3);
// "用户 alice 登录失败，重试 3 次"
```

---

## error_formatter

`operator<<` 输出流支持。

```cpp
std::ostream& operator<<(std::ostream& os, const error_context_t& ctx);
```

**示例输出**

```
[Location: main.cc:42 @ main]
[Sign: Error Level: error, System: database, 数据库服务 / 连接管理]
Code: 1 (ERR_DB_TIMEOUT) - 数据库连接超时
  [Stacktrace]:
    0: main (main.cc:42)
    1: process_request (handler.cc:128)
↳ Caused by: [Location: ...] ... 连接超时
```
