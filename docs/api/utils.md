# Utils 层 API 参考

> 命名空间: `error_system::utils`

---

## async_queue_t\<T, Processor\>

异步工作队列模版类，单生产者-单消费者模式。从 `plugin_registry_t` 抽离的通用并发工具。

### 模版参数

| 参数 | 说明 |
|------|------|
| `T` | 队列元素类型（必须可移动） |
| `Processor` | 处理器类型（`void(T&) noexcept` 可调用对象） |

### 核心 API

```cpp
template <typename T, typename Processor>
class async_queue_t {
public:
    explicit async_queue_t(Processor processor) noexcept;

    // 推入队列，首次调用自动启动工作线程。队列满时返回 false
    bool enqueue(T item) noexcept;

    // 背压控制：0 表示无限制
    void set_max_size(size_t size) noexcept;
    size_t max_size() const noexcept;

    // 查询队列状态
    size_t size() const noexcept;
    bool empty() const noexcept;
};
```

### 特性

| 特性 | 说明 |
|------|------|
| 自动生命周期 | 首次 `enqueue()` 启动线程，析构自动 `join()` |
| 死锁安全 | 先 `running_ = false` 再 `notify_all` 再 `join()` |
| 异常隔离 | 处理器异常 catch 后记录日志继续，工作线程不退出 |
| 背压控制 | `set_max_size()` 限制队列容量，满时拒绝入队 |
| 零 `std::function` | Processor 模版参数，编译期确定调用目标 |

### 使用示例

```cpp
#include "error_system/utils/async_queue.h"

using namespace error_system::utils;

// 创建异步队列，传入处理函数
async_queue_t<int, decltype([](int& v) noexcept { process(v); })> queue(
    [](int& v) noexcept { process(v); }
);

// 推入任务
queue.enqueue(42);

// 设置背压
queue.set_max_size(1000);
std::cout << "max size: " << queue.max_size() << std::endl;

// 析构时自动等待队列排空并停止工作线程
```

---

## string_utils_t

字符串工具类，提供哈希、格式化、分割、修剪、JSON 转义等功能。

### 核心 API

```cpp
class string_utils_t {
public:
    // 哈希（FNV-1a 64 位）
    static constexpr uint64_t hash(std::string_view string) noexcept;
    static constexpr uint64_t hash_limit(std::string_view string,
                                         size_t max_length = 128) noexcept;

    // 格式化（{} 占位符，基于变参模板 + std::to_chars）
    template <typename... Args>
    static inline std::string format(std::string_view format, Args&&... args) noexcept;

    // 分割 / 合并
    static std::vector<std::string_view> split(std::string_view string,
                                               std::string_view delimiter) noexcept;
    static std::string join(const std::vector<std::string_view>& tokens,
                            std::string_view delimiter) noexcept;

    // 修剪（返回 string_view，零拷贝）
    static std::string_view trim(std::string_view string) noexcept;

    // 替换（拷贝原始字符串，原地替换）
    static std::string replace_all(std::string string,
                                   std::string_view from,
                                   std::string_view to) noexcept;

    // 起止判断
    static constexpr bool starts_with(std::string_view string,
                                      std::string_view prefix) noexcept;
    static constexpr bool ends_with(std::string_view string,
                                    std::string_view suffix) noexcept;

    // 大小写转换
    static std::string to_lower(std::string_view string) noexcept;
    static std::string to_upper(std::string_view string) noexcept;

    // 数字解析
    template <typename T>
    static inline std::optional<T> parse_number(std::string_view string) noexcept;

    // JSON 转义
    static std::string escape_json(std::string_view string) noexcept;
};
```

### 使用示例

```cpp
using namespace error_system::utils;

// 哈希
auto h = string_utils_t::hash("hello");

// 格式化（{} 占位符风格）
auto msg = string_utils_t::format("Error code: {}, message: {}", 404, "Not Found");
// msg = "Error code: 404, message: Not Found"

// 分割（返回 string_view，零拷贝）
auto parts = string_utils_t::split("a,b,c", ",");
// parts = {"a", "b", "c"}

// 修剪（返回 string_view）
auto trimmed = string_utils_t::trim("  hello  ");
// trimmed = "hello"

// 替换
auto replaced = string_utils_t::replace_all("hello world", "world", "universe");
// replaced = "hello universe"

// 起止判断
bool pref = string_utils_t::starts_with("hello", "he");   // true
bool suf = string_utils_t::ends_with("hello", "lo");      // true

// 数字解析
auto num = string_utils_t::parse_number<int>("42");
if (num) { int value = *num; }

// 大小写转换
auto lower = string_utils_t::to_lower("HELLO");  // "hello"
auto upper = string_utils_t::to_upper("hello");  // "HELLO"

// JSON 转义
auto escaped = string_utils_t::escape_json("line1\nline2\"quote\"");
// escaped = "line1\\nline2\\\"quote\\\""

// 合并
std::vector<std::string_view> tokens = {"a", "b", "c"};
auto joined = string_utils_t::join(tokens, ", ");
// joined = "a, b, c"
```

---

## json_dict_t

JSON 字典解析与点路径访问工具。支持从文件或字符串加载 JSON。

### 核心 API

```cpp
class json_dict_t {
public:
    json_dict_t() noexcept = default;
    ~json_dict_t() noexcept = default;

    json_dict_t(const json_dict_t&) = default;
    json_dict_t& operator=(const json_dict_t&) = default;
    json_dict_t(json_dict_t&&) noexcept = default;
    json_dict_t& operator=(json_dict_t&&) noexcept = default;

    // 点路径访问："errors.0.code"
    std::optional<std::string> operator[](const std::string& key) const noexcept;
    std::optional<std::string> get_value(const std::string& key) const noexcept;
    std::optional<std::string> get_value_or(const std::string& key,
                                           const std::string& default_value) const noexcept;

    bool contains(const std::string& key) const noexcept;
    bool empty() const noexcept;
    size_t size() const noexcept;

    // 静态工厂方法
    static std::optional<json_dict_t> from_file(const std::filesystem::path& json_path) noexcept;
    static std::optional<json_dict_t> parse(const std::string& json_content) noexcept;
};
```

### 使用示例

```cpp
using namespace error_system::utils;

// 从文件加载
auto dict = json_dict_t::from_file("config/errors.json");
if (dict) {
    // 访问顶层键
    auto name = dict->get_value("service_name");

    // 点路径访问嵌套值
    auto err_name = dict->get_value("errors.0.name");
    auto err_level = dict->get_value("errors.0.level");
    auto err_code = dict->get_value_or("errors.0.code", "0");
}

// 从字符串解析
std::string json = R"({"code": "404", "message": "Not Found"})";
auto parsed = json_dict_t::parse(json);
if (parsed) {
    auto msg = parsed->get_value("message");  // "Not Found"
    auto exists = parsed->contains("code");    // true
}
```

---

## file_utils_t

文件操作工具类，基于 `std::filesystem` 实现。

### 核心 API

```cpp
class file_utils_t {
public:
    // 读写
    static std::optional<std::string> read_file(const std::filesystem::path& path) noexcept;
    static bool write_file(const std::filesystem::path& path,
                           const std::string& content) noexcept;

    // 创建 / 删除
    static bool create_file(const std::filesystem::path& path) noexcept;
    static bool delete_file(const std::filesystem::path& path) noexcept;
    static bool force_delete_file(const std::filesystem::path& path) noexcept;

    // 存在性检查
    static bool file_exists(const std::filesystem::path& path) noexcept;
    static bool dir_exists(const std::filesystem::path& path) noexcept;
};
```

### 使用示例

```cpp
using namespace error_system::utils;

// 读取文件
auto content = file_utils_t::read_file("config/errors.json");
if (content) {
    std::cout << *content << std::endl;
}

// 写入文件
file_utils_t::write_file("output.txt", "Hello, World!");

// 创建/删除文件
file_utils_t::create_file("new_file.txt");
file_utils_t::delete_file("obsolete.txt");
file_utils_t::force_delete_file("readonly.txt");

// 检查存在性
if (file_utils_t::file_exists("config/errors.json")) {
    // 文件存在
}
if (file_utils_t::dir_exists("config/")) {
    // 目录存在
}
```

---

## stack_trace_utils_t

跨平台堆栈跟踪工具，通过 `ERROR_SYSTEM_ENABLE_STACKTRACE` 编译开关控制。

### 核心 API

```cpp
class stack_trace_utils_t {
public:
    // 生成堆栈跟踪，skip_frames 跳过调用栈前 N 帧
    static std::vector<std::string> generate(int skip_frames = 1,
                                             int max_frames = 16) noexcept;
};
```

### 平台支持

| 平台 | 实现方式 | 符号解析 |
|------|----------|----------|
| Linux | `backtrace()` / `backtrace_symbols()` | 支持函数名 |
| macOS | `backtrace()` / `backtrace_symbols()` | 支持函数名 |
| Windows | `CaptureStackBackTrace()` | 支持函数名 |

### 使用示例

```cpp
using namespace error_system::utils;

// 生成堆栈跟踪
auto frames = stack_trace_utils_t::generate();
for (size_t i = 0; i < frames.size(); ++i) {
    std::cout << i << ": " << frames[i] << std::endl;
}

// 指定跳帧数和最大帧数
auto limited = stack_trace_utils_t::generate(2, 8);
```

### 编译期控制

```cmake
# 开启（默认）
set(ERROR_SYSTEM_ENABLE_STACKTRACE ON)

# 关闭（零开销）
set(ERROR_SYSTEM_ENABLE_STACKTRACE OFF)
```

---

## source_location_t

源文件位置信息封装，用于错误发生时的定位。

### 核心 API

```cpp
class source_location_t {
public:
    source_location_t() = default;

    // 自动捕获当前位置（编译期内建宏）
    static source_location_t current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint32_t line = __builtin_LINE());

    // 访问字段
    const char* file_name() const noexcept;
    const char* function_name() const noexcept;
    uint32_t line() const noexcept;
};
```

### 与 error_context_t 集成

`error_context_t` 构造函数自动捕获源位置：

```cpp
error_context_t context(code, "错误信息");
// 内部自动调用 source_location_t::current() 填充位置字段
// 可通过 context.to_string() 输出时显示
```

### 编译期控制

```cmake
# 开启（默认）
set(ERROR_SYSTEM_ENABLE_LOCATION ON)

# 关闭（零开销）
set(ERROR_SYSTEM_ENABLE_LOCATION OFF)
```

---

## error_formatter

`error_context_t` 输出流运算符重载。

```cpp
std::ostream& operator<<(std::ostream& os, const error_context_t& ctx);
```

### 输出文本示例

```
[ERROR][database][1][1][1] 数据库连接超时
at main (main.cc:42)
Stack:
  0: main (main.cc:42)
  1: process_request (handler.cc:128)
Cause: [WARN][network][2][1][1] 连接超时
```

### 自定义格式化

通过 `error_config_t::set_custom_formatter()` 设置自定义格式化器：

```cpp
error_system::config::error_config_t::set_custom_formatter(
    [](const error_context_t& ctx) {
        return ctx.to_json();  // JSON 格式输出
    }
);
```