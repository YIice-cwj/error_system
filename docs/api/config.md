# Config 层 API 参考

> 命名空间: `error_system::config`

---

## error_config_t

全局错误配置类，提供进程级的错误行为配置。所有方法均为 `static`，无需实例化。

### 核心 API

```cpp
class error_config_t {
public:
    // 堆栈追踪配置
    static void set_stacktrace_level(error_level_t level) noexcept;
    static error_level_t get_stacktrace_level() noexcept;
    static void set_enable_stacktrace(bool enable) noexcept;
    static bool is_stacktrace_enabled() noexcept;

    // 验证配置
    static void set_enable_validation(bool enable) noexcept;
    static bool is_validation_enabled() noexcept;

    // 源位置配置
    static void set_enable_source_location(bool enable) noexcept;
    static bool is_source_location_enabled() noexcept;
    static void set_enable_short_filename(bool enable) noexcept;
    static bool is_short_filename_enabled() noexcept;

    // 自定义格式化器
    static void set_custom_formatter(std::function<std::string(const error_context_t&)> formatter);
    static std::function<std::string(const error_context_t&)> get_custom_formatter() noexcept;

    // 自定义翻译器
    static void set_custom_translator(std::function<std::string(error_code_t)> translator);
    static std::function<std::string(error_code_t)> get_custom_translator() noexcept;

    // 重置所有配置为默认值
    static void reset_to_defaults() noexcept;
};
```

### 配置项说明

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `min_stacktrace_level_` | `error_level_t::error` | 自动捕获堆栈的最低等级 |
| `enable_stacktrace_` | `true` | 堆栈追踪总开关 |
| `enable_validation_` | `true` | 错误码验证开关 |
| `enable_source_location_` | `true` | 源位置追踪开关 |
| `enable_short_filename_` | `true` | 短文件名模式（仅显示文件名，不含路径） |
| `custom_formatter_` | `nullptr` | 自定义格式化回调，用于 `error_context_t::to_string()` |
| `custom_translator_` | `nullptr` | 自定义翻译函数，用于错误码描述翻译 |

### 使用示例

```cpp
using namespace error_system::config;

// 设置 WARN 及以上自动捕获堆栈
error_config_t::set_stacktrace_level(error_level_t::warn);

// 关闭堆栈追踪（性能敏感场景）
error_config_t::set_enable_stacktrace(false);

// 关闭错误码验证
error_config_t::set_enable_validation(false);

// 关闭源位置追踪
error_config_t::set_enable_source_location(false);

// 使用完整路径
error_config_t::set_enable_short_filename(false);

// 设置自定义格式化器
error_config_t::set_custom_formatter([](const error_context_t& ctx) {
    return "[CUSTOM] " + ctx.message;
});

// 设置自定义翻译器
error_config_t::set_custom_translator([](error_code_t code) {
    return "Translated: " + std::to_string(code.get_raw_code());
});

// 重置为默认值
error_config_t::reset_to_defaults();
```

### 与 CMake 选项的关系

| CMake 选项 | 编译期影响 | 运行时影响 |
|------------|-----------|-----------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | 开启/关闭堆栈追踪 API | `is_stacktrace_enabled()` 返回值 |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | 开启/关闭验证 API | `is_validation_enabled()` 返回值 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | 开启/关闭源位置 API | `is_source_location_enabled()` 返回值 |

> **注意**：CMake 选项在编译期决定相关 API 是否可用。若编译期关闭，运行时配置项将标记为 `[[deprecated]]` 并返回默认值。

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/config/error_config_test.cc` | 7 | 配置项设置/获取、默认值、重置 |

---

## 配置最佳实践

### 1. 开发环境配置

```cpp
// 开发环境：启用所有诊断功能
error_config_t::set_stacktrace_level(error_level_t::debug);
error_config_t::set_enable_stacktrace(true);
error_config_t::set_enable_validation(true);
error_config_t::set_enable_source_location(true);
error_config_t::set_enable_short_filename(true);
```

### 2. 生产环境配置

```cpp
// 生产环境：仅捕获 ERROR 及以上堆栈，关闭验证以提升性能
error_config_t::set_stacktrace_level(error_level_t::error);
error_config_t::set_enable_validation(false);
error_config_t::set_enable_source_location(true);
error_config_t::set_enable_short_filename(true);
```

### 3. 高性能场景配置

```cpp
// 高频错误场景：关闭堆栈和源位置，仅保留基本错误码
error_config_t::set_enable_stacktrace(false);
error_config_t::set_enable_source_location(false);
error_config_t::set_enable_validation(false);
```

### 4. 自定义格式化示例

```cpp
// JSON 格式输出
error_config_t::set_custom_formatter([](const error_context_t& ctx) {
    return ctx.to_json();
});

// 简洁格式输出
error_config_t::set_custom_formatter([](const error_context_t& ctx) {
    std::ostringstream oss;
    oss << "[" << to_string(ctx.code.get_level()) << "]"
        << "[" << ctx.code.get_domain() << "]"
        << " " << ctx.message;
    return oss.str();
});
```
