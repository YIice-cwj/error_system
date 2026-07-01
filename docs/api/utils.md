# 🧰 Utils 层 API

> `error_system::utils`

---

## 📬 async_queue_t\<T, Processor\>

异步工作队列 — 单生产者-单消费者模式。

> 📝 **说明**：首次 `enqueue()` 启动工作线程，析构自动 `join()`。`T` 必须可移动构造；`Processor` 必须可调用 `void(T&)` 且 `noexcept`。不可拷贝、不可移动。

### 🛠️ API

```cpp
template <typename T, typename Processor>
class async_queue_t {
public:
    using value_type_t      = T;
    using processor_t       = Processor;

    explicit async_queue_t(processor_t processor) noexcept;
    ~async_queue_t() noexcept;  // 自动停止并 join 工作线程

    bool  enqueue(value_type_t item) noexcept;   // 首次调用自动启动工作线程
    void  set_max_size(size_t size) noexcept;    // 0 = 无限制
    [[nodiscard]] size_t max_size() const noexcept;
    [[nodiscard]] size_t size()    const noexcept;
    [[nodiscard]] bool   empty()   const noexcept;
};
```

### ⚙️ 特性

| 特性 | 说明 |
|------|------|
| 自动生命周期 | 首次 `enqueue()` 启动，析构 `join()` |
| 死锁安全 | 先设 `running_=false`，再 `notify_all`，再 `join` |
| 异常隔离 | 处理器异常不退出工作线程 |
| 背压控制 | 队列满拒绝入队 |
| 零 `std::function` | `Processor` 模版参数，编译期确定调用 |

### 💡 使用示例

```cpp
async_queue_t<int, decltype([](int& v) noexcept { process(v); })> queue(
    [](int& v) noexcept { process(v); });
queue.enqueue(42);
queue.set_max_size(1000);
```

---

## ✂️ string_utils_t

字符串处理工具类（不可实例化，全部为静态方法）。

> 📝 **说明**：格式化功能已迁移至 `string_format_t`；JSON 转义功能已迁移至 `json_serializer_t`。

### 🛠️ API

```cpp
class string_utils_t {
public:
    [[nodiscard]] static constexpr uint64_t hash(std::string_view string) noexcept;
    [[nodiscard]] static constexpr uint64_t hash_limit(std::string_view string,
                                                       size_t max_length = 128) noexcept;
    [[nodiscard]] static constexpr bool starts_with(std::string_view string, std::string_view prefix) noexcept;
    [[nodiscard]] static constexpr bool ends_with(std::string_view string, std::string_view suffix) noexcept;

    template <typename T>
    [[nodiscard]] static std::optional<T> parse_number(std::string_view string) noexcept;

    static std::string replace_all(std::string string, std::string_view from, std::string_view to) noexcept;
    [[nodiscard]] static std::vector<std::string_view> split(std::string_view string,
                                                              std::string_view delimiter) noexcept;
    [[nodiscard]] static std::string join(const std::vector<std::string_view>& tokens,
                                           std::string_view delimiter) noexcept;
    [[nodiscard]] static std::string_view trim(std::string_view string) noexcept;
    [[nodiscard]] static std::string to_lower(std::string_view string) noexcept;
    [[nodiscard]] static std::string to_upper(std::string_view string) noexcept;
};
```

### 📋 速查

```cpp
string_utils_t::hash("hello");                            // constexpr FNV-1a
string_utils_t::split("a,b,c", ",");                      // {"a", "b", "c"}
string_utils_t::trim("  hi  ");                           // "hi"
string_utils_t::replace_all("hello world", "world", "universe");
string_utils_t::starts_with("hello", "he");               // true
string_utils_t::parse_number<int>("42");                  // std::optional{42}
```

---

## 🧩 json_dict_t

JSON 解析与点路径访问。默认可拷贝、可移动。

> ⚠️ **警告**：简化的 JSON 解析器，**仅支持扁平 / 嵌套的字符串键值对**，不支持数字、布尔、数组等非字符串值。生产环境如需完整 JSON 解析请使用 nlohmann/json 等第三方库。

### 🛠️ API

```cpp
class json_dict_t {
public:
    json_dict_t() noexcept = default;

    [[nodiscard]] std::optional<std::string> operator[](const std::string& key) const noexcept;
    [[nodiscard]] std::optional<std::string> get_value(const std::string& key) const noexcept;
    [[nodiscard]] std::string get_value_or(const std::string& key,
                                           const std::string& default_value) const noexcept;
    [[nodiscard]] bool   contains(const std::string& key) const noexcept;
    [[nodiscard]] bool   empty() const noexcept;
    [[nodiscard]] size_t size()  const noexcept;

    [[nodiscard]] static std::optional<json_dict_t> from_file(const std::filesystem::path& json_path) noexcept;
    [[nodiscard]] static std::optional<json_dict_t> parse(const std::string& json_content) noexcept;
};
```

### 💡 使用示例

```cpp
auto dict = json_dict_t::parse(R"({"user":{"name":"Alice"}})");
auto name = dict->get_value("user.name");           // "Alice"
auto code = dict->get_value_or("code", "0");        // std::optional{"0"}（默认）
dict->contains("user.name");                        // true

// 嵌套路径访问（支持多层）
auto deep = json_dict_t::parse(R"({"a":{"b":{"c":"value"}}})");
deep->get_value("a.b.c");                           // "value"
```

---

## 🧬 json_serializer_t

JSON 序列化辅助工具（不可实例化）。

### 🛠️ API

```cpp
class json_serializer_t {
public:
    [[nodiscard]] static std::string escape_json(std::string_view value) noexcept;
};
```

### 💡 使用示例

```cpp
json_serializer_t::escape_json(R"(a"b\c)");   // "a\"b\\c"
```

---

## 🔍 json_lexer_t

JSON 词法分析器，`json_dict_t` 的实现基础，位于 `error_system::utils::detail` 命名空间。默认可拷贝、可移动。

> 📝 **说明**：支持 RFC 8259 规范的 UTF-16 代理对解析（`\uD83D\uDE00` → 4 字节 UTF-8 编码），孤立代理区码点会被静默丢弃。

### 🛠️ API

```cpp
namespace error_system::utils::detail {

class json_lexer_t {
public:
    enum class token_type_t {
        string, colon, comma, left_brace, right_brace, eof, invalid
    };

    struct token_t {
        token_type_t type{token_type_t::eof};
        std::string  value;
    };

    explicit json_lexer_t(std::string_view json_text) noexcept;

    [[nodiscard]] token_t next() noexcept;
};

}  // namespace error_system::utils::detail
```

---

## 📁 file_utils_t

跨平台文件操作工具（不可实例化）。

> ⚠️ **警告**：`read_file()` 内置文件大小上限保护（`MAX_READ_FILE_SIZE = 64 MB`），防止 OOM 攻击，超过阈值返回 `std::nullopt`。

### 🛠️ API

```cpp
class file_utils_t {
public:
    static constexpr size_t MAX_READ_FILE_SIZE = 64 * 1024 * 1024;  // 64 MB

    [[nodiscard]] static std::optional<std::string> read_file(const std::filesystem::path& path) noexcept;
    [[nodiscard]] static bool write_file(const std::filesystem::path& path, const std::string& content) noexcept;
    [[nodiscard]] static bool create_file(const std::filesystem::path& path) noexcept;
    [[nodiscard]] static bool delete_file(const std::filesystem::path& path) noexcept;
    [[nodiscard]] static bool force_delete_file(const std::filesystem::path& path) noexcept;
    [[nodiscard]] static bool file_exists(const std::filesystem::path& path) noexcept;
    [[nodiscard]] static bool dir_exists(const std::filesystem::path& path) noexcept;
};
```

### 💡 使用示例

```cpp
auto content = file_utils_t::read_file("config.json");
file_utils_t::write_file("output.txt", "Hello!");
file_utils_t::dir_exists("config/");    // true/false
```

---

## 📚 stack_trace_utils_t

跨平台堆栈跟踪工具（不可实例化）。

### 🛠️ API

```cpp
class stack_trace_utils_t {
public:
    [[nodiscard]] static std::vector<std::string> generate(int skip_frames = 1,
                                                            int max_frames  = 16) noexcept;
};
```

### 🖥️ 平台实现

| 平台 | 实现 |
|------|------|
| Linux | `backtrace()` / `backtrace_symbols()` + cxxabi |
| macOS | `backtrace()` / `backtrace_symbols()` + cxxabi |
| Windows | `StackWalk64()` + `SymFromAddr` |

### 💡 使用示例

```cpp
auto frames = stack_trace_utils_t::generate(2, 8);
for (const auto& f : frames) std::cout << f << "\n";
```

---

## 📍 source_location_t

源位置封装，`error_context_t` 构造时通过 `located_code_t` 隐式转换自动捕获调用者位置。默认可拷贝、可移动。

### 🛠️ 相关函数

```cpp
// 提取路径中的文件名部分（去掉目录）
[[nodiscard]] constexpr const char* extract_short_filename(const char* path) noexcept;
```

### 🛠️ API

```cpp
class source_location_t {
public:
    constexpr source_location_t() noexcept = default;
    constexpr source_location_t(const char* file, const char* func, uint32_t line) noexcept;

    [[nodiscard]] static constexpr source_location_t current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint32_t   line = __builtin_LINE()) noexcept;

    [[nodiscard]] constexpr const char* file_name()     const noexcept;
    [[nodiscard]] constexpr const char* function_name() const noexcept;
    [[nodiscard]] constexpr uint32_t    line()          const noexcept;
};
```

> 💡 **提示**：通过默认参数在调用点展开 `__builtin_FILE()`，捕获的是调用者位置而非库内部位置。

---

## 🎨 string_format_t

`{}` 占位符格式化工具，`error_context_t` 消息格式化使用。

> 💡 **提示**：支持算术类型、指针、`bool`、`char`，以及含 `to_string()` 成员或全局函数的自定义类型。`{{` / `}}` 转义为字面 `{` / `}`。

### 🛠️ API

```cpp
class string_format_t {
public:
    template <typename... Args>
    [[nodiscard]] static inline std::string format(std::string_view format_str,
                                                    Args&&... args) noexcept;
};
```

### 💡 使用示例

```cpp
auto msg = string_format_t::format("用户 {} 登录失败，重试 {} 次", "alice", 3);
// "用户 alice 登录失败，重试 3 次"

string_format_t::format("code: {}, msg: {}", 404, "NF");
// "code: 404, msg: NF"
```

---

## 🖨️ error_formatter

`error_context_t` 的 `operator<<` 输出流支持。

> 📝 **说明**：位于 `error_system::core` 命名空间（非 `utils`）。

### 🛠️ API

```cpp
namespace error_system::core {

[[nodiscard]] inline std::ostream& operator<<(std::ostream& stream,
                                               const error_context_t& context) noexcept;

}  // namespace error_system::core
```

### 📤 示例输出

```
[Location: main.cc:42 @ main]
[Sign: Error Level: error, System: database, 数据库服务 / 连接管理]
Code: 1 (ERR_DB_TIMEOUT) - 数据库连接超时
  [Stacktrace]:
    0: main (main.cc:42)
    1: process_request (handler.cc:128)
↳ Caused by: [Location: ...] ... 连接超时
```
