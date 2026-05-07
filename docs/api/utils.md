# Utils 层 API 文档

> 命名空间：`error_system::utils`

Utils 层提供字符串处理、JSON 解析、文件操作和堆栈跟踪等通用工具，被系统各层广泛使用。

---

## string_utils_t

**头文件**：`error_system/utils/string_utils.h`

静态工具类，所有方法均为 `static`，不可实例化。

### hash / hash_limit

编译期 FNV-1a 哈希，用于 `constexpr switch` 的字符串分发。

```cpp
// 编译期哈希
static constexpr uint64_t hash(std::string_view str) noexcept;

// 限制长度后哈希（超出则截断）
static constexpr uint64_t hash_limit(std::string_view str, size_t max_length = 128) noexcept;

// 用法：switch 中进行字符串匹配
switch (string_utils_t::hash(input)) {
    case string_utils_t::hash("debug"): return error_level_t::debug;
    case string_utils_t::hash("error"): return error_level_t::error;
}
```

### format

轻量字符串格式化，使用 `{}` 作为占位符，支持任意可流输出类型。

```cpp
template<typename... Args>
static std::string format(std::string_view fmt, Args&&... args);

// 示例
std::string msg = string_utils_t::format(
    "[Level: {}, Code: {}]", "error", 404
);
// → "[Level: error, Code: 404]"
```

### starts_with / ends_with

```cpp
static constexpr bool starts_with(std::string_view str, std::string_view prefix) noexcept;
static constexpr bool ends_with(std::string_view str, std::string_view suffix) noexcept;
```

### parse_number

将字符串解析为数字，失败返回 `std::nullopt`。

```cpp
template<typename T>
static std::optional<T> parse_number(std::string_view str) noexcept;

auto n = string_utils_t::parse_number<int>("42");  // optional(42)
auto x = string_utils_t::parse_number<int>("abc"); // nullopt
```

### replace_all / split / join / trim

```cpp
// 替换所有出现
static std::string replace_all(std::string str, std::string_view from, std::string_view to) noexcept;

// 按分隔符分割（返回 string_view 视图，零拷贝）
static std::vector<std::string_view> split(std::string_view str, std::string_view delim) noexcept;

// 合并
static std::string join(const std::vector<std::string_view>& tokens, std::string_view delim) noexcept;

// 去除首尾空白
static std::string_view trim(std::string_view str) noexcept;
```

### to_lower / to_upper

```cpp
static std::string to_lower(std::string_view str) noexcept;
static std::string to_upper(std::string_view str) noexcept;
```

### escape_json

将字符串中的控制字符转义为合法的 JSON 格式，用于 `error_context_t::to_json()` 的序列化。

```cpp
static std::string escape_json(std::string_view str) noexcept;

// 示例
std::string json = string_utils_t::escape_json("quote\"newline\n");
// → "quote\\\"newline\\n"
```

---

## json_dict_t

**头文件**：`error_system/utils/json_utils.h`

轻量 JSON 字典，支持点分隔路径的嵌套键访问，主要用于 i18n 字典加载。

```cpp
// 从文件加载
static std::optional<json_dict_t> from_file(const std::filesystem::path& json_path) noexcept;

// 从字符串解析
static std::optional<json_dict_t> parse(const std::string& json_content) noexcept;

// 获取值，key 支持点分隔嵌套路径
std::optional<std::string> get_value(const std::string& key) const noexcept;

// 获取值，若不存在则返回 default_value
std::optional<std::string> get_value_or(const std::string& key,
                                         const std::string& default_value) const noexcept;

// 检查字典是否包含指定键
bool contains(const std::string& key) const noexcept;

// 判断字典是否为空
bool empty() const noexcept;

// 获取字典大小
size_t size() const noexcept;

// 示例
auto dict = json_dict_t::from_file("zh_cn.json");
std::string text = dict->get_value_or("domain.database", "database").value();
```

---

## file_utils

**头文件**：`error_system/utils/file_utils.h`

跨平台文件读写工具类。

```cpp
// 读取整个文件内容
static std::optional<std::string> read_file(const std::filesystem::path& path) noexcept;

// 写入文件（覆盖）
static bool write_file(const std::filesystem::path& path, const std::string& content) noexcept;

// 创建文件
static bool create_file(const std::filesystem::path& path) noexcept;

// 删除文件
static bool delete_file(const std::filesystem::path& path) noexcept;

// 强制删除文件
static bool force_delete_file(const std::filesystem::path& path) noexcept;

// 检查文件是否存在
static bool file_exists(const std::filesystem::path& path) noexcept;

// 检查文件路径是否存在
static bool file_path_exists(const std::filesystem::path& path) noexcept;
```

---

## stack_trace_utils_t

**头文件**：`error_system/utils/stack_trace_utils.h`

跨平台堆栈跟踪工具类，支持 Linux、macOS 和 Windows。自动处理符号解析（demangle C++ 函数名）。

> **注意**：堆栈追踪功能可通过 CMake 选项 `ERROR_SYSTEM_ENABLE_STACKTRACE` 在编译期开启或关闭。关闭后 `generate()` 返回空向量。

### 方法

```cpp
// 抓取当前线程的函数调用栈
// skip_frames: 跳过的顶部栈帧数（默认跳过 generate 自身）
// max_frames: 最大抓取深度（默认 64）
static std::vector<std::string> generate(int skip_frames = 1, int max_frames = 64) noexcept;
```

### 平台支持

| 平台 | 实现方式 | 符号解析 |
|------|----------|----------|
| Linux | `backtrace` + `backtrace_symbols` | `abi::__cxa_demangle` |
| macOS | `backtrace` + `backtrace_symbols` | `abi::__cxa_demangle` |
| Windows | `CaptureStackBackTrace` + `SymFromAddr` | `DbgHelp` / `abi::__cxa_demangle` (MinGW) |

### 示例

```cpp
#include "error_system/utils/stack_trace_utils.h"

// 抓取当前调用栈（跳过 generate 自身）
auto frames = utils::stack_trace_utils_t::generate(1, 32);

for (size_t i = 0; i < frames.size(); ++i) {
    std::cout << "#" << i << " " << frames[i] << "\n";
}
// 输出示例：
// #0 some_function() at /path/to/file.cc:42
// #1 caller_function() at /path/to/file.cc:56
// #2 main at /path/to/main.cc:10
```

### 在 error_context_t 中的使用

`error_context_t` 的构造函数内部自动调用 `stack_trace_utils_t::generate(1)` 来抓取堆栈。捕获行为由 `error_config::set_stacktrace_level()` 和编译选项共同控制：

```cpp
// 设置 WARN 及以上自动捕获堆栈
error_config::set_stacktrace_level(error_level_t::warn);

// 此错误等级为 error，会自动抓取堆栈
auto code = error_builder_t::make_error_code(
    error_level_t::error, domain::system_domain_t::database, 0, 0, 500);
error_context_t ctx(code, "数据库错误");  // ctx.stack_frames 自动填充
```

---

## source_location_t

**头文件**：`error_system/utils/source_location.h`

轻量级源文件位置信息封装，用于 `error_context_t` 的源位置追踪功能。基于编译器内置宏（`__builtin_FILE()` / `__builtin_FUNCTION()` / `__builtin_LINE()`）实现，支持 GCC、Clang 和 MSVC 1926+。

> **注意**：源位置追踪功能可通过 CMake 选项 `ERROR_SYSTEM_ENABLE_LOCATION` 在编译期开启或关闭。

### 方法

```cpp
// 获取当前源位置（编译期）
static constexpr source_location_t current(
    const char* file = __builtin_FILE(),
    const char* func = __builtin_FUNCTION(),
    uint32_t line = __builtin_LINE()
) noexcept;

// 访问字段
constexpr const char* file_name() const noexcept;
constexpr const char* function_name() const noexcept;
constexpr uint32_t line() const noexcept;
```

### 辅助函数

```cpp
// 从完整路径中提取短文件名（constexpr）
constexpr const char* extract_short_filename(const char* path) noexcept;
```

### 示例

```cpp
#include "error_system/utils/source_location.h"

auto loc = utils::source_location_t::current();
std::cout << "File: " << loc.file_name() << "\n";
std::cout << "Function: " << loc.function_name() << "\n";
std::cout << "Line: " << loc.line() << "\n";
```

### 在 error_context_t 中的使用

当 `ERROR_SYSTEM_ENABLE_LOCATION` 开启时，`error_context_t` 的构造函数自动通过 `code_with_location_t` 捕获源位置：

```cpp
error_context_t ctx(code, "错误信息");  // 自动记录 file_name, function_name, line_number
```

捕获行为由 `error_config_t::is_source_location_enabled()` 控制：

```cpp
// 关闭源位置追踪（运行时）
error_config_t::set_enable_source_location(false);

// 使用短文件名模式
error_config_t::set_enable_short_filename(true);
```

---

## operator<< (error_context_t)

**头文件**：`error_system/utils/error_formatter.h`

为 `error_context_t` 提供的 `std::ostream` 输出流运算符重载，方便直接打印错误上下文。

```cpp
inline std::ostream& operator<<(std::ostream& os, const error_system::core::error_context_t& context);
```

### 示例

```cpp
#include "error_system/utils/error_formatter.h"

error_context_t ctx(code, "操作失败");
std::cout << ctx << "\n";  // 等价于 ctx.to_string()
```
