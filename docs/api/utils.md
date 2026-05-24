# Utils 层 API 参考

> 命名空间: `error_system::utils`

---

## string_utils_t

字符串工具类，提供哈希、格式化、分割、修剪、JSON 转义等功能。

### 核心 API

```cpp
class string_utils_t {
public:
    // 哈希
    static size_t hash(std::string_view str) noexcept;

    // 格式化
    static std::string format(const char* fmt, ...);

    // 分割
    static std::vector<std::string> split(std::string_view str, std::string_view delimiter);

    // 修剪
    static std::string trim(std::string_view str);
    static std::string trim_left(std::string_view str);
    static std::string trim_right(std::string_view str);

    // JSON 转义
    static std::string escape_json(std::string_view str);

    // 大小写转换
    static std::string to_lower(std::string_view str);
    static std::string to_upper(std::string_view str);

    // 替换
    static std::string replace(std::string_view str, std::string_view from, std::string_view to);

    // 是否包含
    static bool contains(std::string_view str, std::string_view substr) noexcept;

    // 起止判断
    static bool starts_with(std::string_view str, std::string_view prefix) noexcept;
    static bool ends_with(std::string_view str, std::string_view suffix) noexcept;
};
```

### 使用示例

```cpp
using namespace error_system::utils;

// 哈希
auto h = string_utils_t::hash("hello");

// 格式化
auto msg = string_utils_t::format("Error code: %d, message: %s", 404, "Not Found");

// 分割
auto parts = string_utils_t::split("a,b,c", ",");
// parts = {"a", "b", "c"}

// 修剪
auto trimmed = string_utils_t::trim("  hello  ");
// trimmed = "hello"

// JSON 转义
auto escaped = string_utils_t::escape_json("line1\nline2\"quote\"");
// escaped = "line1\\nline2\\\"quote\\\""

// 大小写转换
auto lower = string_utils_t::to_lower("HELLO");  // "hello"
auto upper = string_utils_t::to_upper("hello");  // "HELLO"

// 替换
auto replaced = string_utils_t::replace("hello world", "world", "universe");
// replaced = "hello universe"
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/utils/string_utils_test.cc` | 12+ | 哈希、格式化、分割、修剪、JSON 转义、大小写转换 |

---

## json_dict_t

JSON 字典加载与点路径访问工具。

### 核心 API

```cpp
class json_dict_t {
public:
    bool load_from_file(const std::string& file_path);
    bool load_from_string(const std::string& json_str);

    bool has_key(std::string_view key) const;
    std::optional<std::string> get_string(std::string_view key) const;
    std::optional<int64_t> get_int(std::string_view key) const;
    std::optional<double> get_double(std::string_view key) const;
    std::optional<bool> get_bool(std::string_view key) const;

    // 点路径访问："errors.0.code"
    std::optional<std::string> get_string_by_path(std::string_view path) const;
    std::optional<int64_t> get_int_by_path(std::string_view path) const;

    void clear() noexcept;
    bool empty() const noexcept;
    size_t size() const noexcept;
};
```

### 使用示例

```cpp
using namespace error_system::utils;

json_dict_t dict;

// 从文件加载
if (dict.load_from_file("config/errors.json")) {
    // 访问顶层键
    auto name = dict.get_string("service_name");

    // 点路径访问嵌套值
    auto code = dict.get_string_by_path("errors.0.name");
    auto level = dict.get_string_by_path("errors.0.level");
    auto number = dict.get_int_by_path("errors.0.number");
}

// 从字符串加载
std::string json = R"({"code": 404, "message": "Not Found"})";
dict.load_from_string(json);
auto msg = dict.get_string("message");  // "Not Found"
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/utils/json_utils_test.cc` | 8 | 文件加载、字符串加载、点路径访问、类型转换 |

---

## file_utils

文件操作工具类。

### 核心 API

```cpp
class file_utils {
public:
    static std::string read_file(const std::string& file_path);
    static bool write_file(const std::string& file_path, std::string_view content);
    static bool append_file(const std::string& file_path, std::string_view content);

    static bool exists(const std::string& file_path);
    static bool remove(const std::string& file_path);
    static bool create_directory(const std::string& dir_path);

    static std::vector<std::string> list_files(const std::string& dir_path);
    static std::string get_extension(const std::string& file_path);
    static std::string get_filename(const std::string& file_path);
};
```

### 使用示例

```cpp
using namespace error_system::utils;

// 读取文件
auto content = file_utils::read_file("config/errors.json");

// 写入文件
file_utils::write_file("output.txt", "Hello, World!");

// 检查存在性
if (file_utils::exists("config/errors.json")) {
    // ...
}

// 创建目录
file_utils::create_directory("logs/2024");

// 列出文件
auto files = file_utils::list_files("config/errors");
// files = {"trade_service_errors.json", "user_service_errors.json", ...}
```

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/utils/file_utils_test.cc` | 8 | 读写、存在性检查、目录操作、文件列表 |

---

## stack_trace_utils_t

跨平台堆栈跟踪工具，支持 Linux、macOS 和 Windows。

### 核心 API

```cpp
class stack_trace_utils_t {
public:
    static std::vector<std::string> generate(size_t skip_frames = 1, size_t max_frames = 64);
    static std::string to_string(const std::vector<std::string>& frames);

    static bool is_supported() noexcept;
    static std::string get_platform_info() noexcept;
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
for (const auto& frame : frames) {
    std::cout << frame << std::endl;
}

// 输出示例：
// 0: main (main.cc:42)
// 1: process_request (handler.cc:128)
// 2: handle_connection (server.cc:256)

// 转换为字符串
std::string stack_str = stack_trace_utils_t::to_string(frames);

// 检查平台支持
if (stack_trace_utils_t::is_supported()) {
    std::cout << "Platform: " << stack_trace_utils_t::get_platform_info() << std::endl;
}
```

### 编译期控制

堆栈追踪功能可通过 CMake 选项在编译期开启或关闭：

```cmake
# 开启（默认）
set(ERROR_SYSTEM_ENABLE_STACKTRACE ON)

# 关闭（零开销）
set(ERROR_SYSTEM_ENABLE_STACKTRACE OFF)
```

关闭后 `generate()` 将标记为 `[[deprecated]]` 并返回空向量。

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/utils/stack_trace_utils_test.cc` | 6 | 生成、格式化、平台检测 |

---

## source_location_t

源文件位置信息封装，用于错误发生时的定位。

### 核心 API

```cpp
class source_location_t {
public:
    std::string file_name;
    std::string function_name;
    uint32_t line_number{0};
    uint32_t column_number{0};

    source_location_t() = default;

    static source_location_t current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint32_t line = __builtin_LINE(),
        uint32_t column = __builtin_COLUMN());

    std::string to_string() const;
    bool is_valid() const noexcept;
};
```

### 使用示例

```cpp
using namespace error_system::utils;

// 自动捕获当前位置
auto loc = source_location_t::current();
// loc.file_name = "/path/to/file.cc"
// loc.function_name = "my_function"
// loc.line_number = 42

// 手动构造
source_location_t manual_loc;
manual_loc.file_name = "custom.cc";
manual_loc.function_name = "custom_func";
manual_loc.line_number = 100;

// 转换为字符串
std::string str = loc.to_string();
// 输出: "file.cc:42 (my_function)"

// 检查有效性
if (loc.is_valid()) {
    std::cout << loc.to_string() << std::endl;
}
```

### 与 error_context_t 集成

`error_context_t` 构造函数自动捕获源位置：

```cpp
error_context_t ctx(code, "错误信息");
// ctx.source_location 自动填充为构造位置
```

### 编译期控制

源位置追踪可通过 CMake 选项在编译期开启或关闭：

```cmake
# 开启（默认）
set(ERROR_SYSTEM_ENABLE_LOCATION ON)

# 关闭（零开销）
set(ERROR_SYSTEM_ENABLE_LOCATION OFF)
```

关闭后 `current()` 将标记为 `[[deprecated]]` 并返回默认构造的对象。

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/utils/source_location_test.cc` | 5 | 自动捕获、手动构造、字符串转换、有效性检查 |

---

## error_formatter.h

`error_context_t` 输出流运算符重载。

```cpp
std::ostream& operator<<(std::ostream& os, const error_context_t& ctx);
```

### 输出格式

```cpp
error_context_t ctx(code, "数据库连接超时");
std::cout << ctx << std::endl;

// 输出示例：
// [ERROR][database][1][1][1] 数据库连接超时
// at main (main.cc:42)
// Stack:
//   0: main (main.cc:42)
//   1: process_request (handler.cc:128)
// Cause: [WARN][network][2][1][1] 连接超时
```

### 自定义格式化

通过 `config::error_config_t::set_custom_formatter()` 设置自定义格式化器：

```cpp
error_system::config::error_config_t::set_custom_formatter(
    [](const error_context_t& ctx) {
        return ctx.to_json();  // JSON 格式输出
    }
);
```
