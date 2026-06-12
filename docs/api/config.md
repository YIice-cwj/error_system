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

    // per-code 堆栈等级覆盖（v2.0 新增）
    static void set_per_code_stacktrace_level(uint64_t identity_code, error_level_t level) noexcept;
    static std::optional<error_level_t> get_per_code_stacktrace_level(uint64_t identity_code) noexcept;
    static void remove_per_code_stacktrace_level(uint64_t identity_code) noexcept;

    // 验证配置
    static void set_enable_validation(bool enable) noexcept;
    static bool is_validation_enabled() noexcept;

    // 源位置配置
    static void set_enable_source_location(bool enable) noexcept;
    static bool is_source_location_enabled() noexcept;
    static void set_enable_short_filename(bool enable) noexcept;
    static bool is_short_filename_enabled() noexcept;

    // 输出格式配置
    static void set_enable_text_output(bool enable) noexcept;
    static bool is_text_output_enabled() noexcept;

    // 自定义格式化器
    static void set_custom_formatter(std::function<std::string(const error_context_t&)> formatter) noexcept;
    static const std::function<std::string(const error_context_t&)>& get_custom_formatter() noexcept;

    // 插件通知模式（v2.0 新增）
    enum class notify_mode_t : uint8_t { sync = 0, async_queue = 1 };
    static void set_notify_mode(notify_mode_t mode) noexcept;
    static notify_mode_t get_notify_mode() noexcept;

    // 异步队列背压控制（v2.1 新增）
    static void set_max_queue_size(size_t size) noexcept;
    static size_t get_max_queue_size() noexcept;

    // 重置所有配置为默认值
    static void reset_to_defaults() noexcept;
};
```

### 配置项说明

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `min_stacktrace_level_` | `error_level_t::error` | 自动捕获堆栈的最低等级 |
| `per_code_stacktrace_map_` | 空 | per-code 堆栈等级覆盖（优先级 > 全局配置） |
| `enable_stacktrace_` | `true` | 堆栈追踪总开关 |
| `enable_validation_` | `true` | 错误码验证开关 |
| `enable_source_location_` | `true` | 源位置追踪开关 |
| `enable_short_filename_` | `true` | 短文件名模式（仅显示文件名，不含路径） |
| `enable_text_output_` | `true` | 文本输出模式（false 时输出数字 ID） |
| `notify_mode_` | `sync` | 插件通知模式（sync：同步，async_queue：异步队列） |
| `max_queue_size_` | `0`（无限制） | 异步队列最大容量（v2.1），超过后新通知丢弃 |
| `custom_formatter_` | `nullptr` | 自定义格式化回调 |

### 使用示例

```cpp
using namespace error_system::config;

// 设置 WARN 及以上自动捕获堆栈
error_config_t::set_stacktrace_level(error_level_t::warn);

// per-code 覆盖：为特定错误码设置更低的触发等级
auto id = ERR_CRITICAL.get_identity_code();
error_config_t::set_per_code_stacktrace_level(id, error_level_t::debug);

// 查询 per-code 覆盖值
auto level = error_config_t::get_per_code_stacktrace_level(id);
if (level.has_value()) {
    // ERR_CRITICAL 总是捕获堆栈
}

// 删除 per-code 覆盖，恢复使用全局配置
error_config_t::remove_per_code_stacktrace_level(id);

// 关闭堆栈追踪（性能敏感场景）
error_config_t::set_enable_stacktrace(false);

// 关闭错误码验证
error_config_t::set_enable_validation(false);

// 控制输出格式：文本模式显示子系统/模块名称，数字模式显示原始 ID
error_config_t::set_enable_text_output(true);

// 切换为异步插件通知（v2.0 新增）
error_config_t::set_notify_mode(error_config_t::notify_mode_t::async_queue);
// error_context_t 构造时不再阻塞在插件 I/O 上

// v2.1：设置异步队列最大容量（背压保护）
error_config_t::set_max_queue_size(10000);
// 队列满时新通知将被丢弃，避免内存无限增长

// 设置自定义格式化器
error_config_t::set_custom_formatter([](const error_context_t& ctx) {
    return "[CUSTOM] " + ctx.message;
});

// 重置为默认值
error_config_t::reset_to_defaults();
```

### per-code 配置优先级

当同时设置了全局和 per-code 堆栈等级时，`finalize_runtime_features()` 的决策逻辑：

```
1. 若 is_stacktrace_enabled() == false → 不捕获堆栈
2. 否则，取 max(全局等级, per-code 等级)
   - 若 per-code 有值，使用 per-code 值
   - 若 per-code 无值，使用全局值
3. 若错误等级 >= 最终阈值 → 捕获堆栈
```

### 与 CMake 选项的关系

| CMake 选项 | 编译期影响 | 运行时影响 |
|------------|-----------|-----------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | 开启/关闭堆栈追踪 + per-code API | `is_stacktrace_enabled()` 返回值 |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | 开启/关闭验证 API | `is_validation_enabled()` 返回值 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | 开启/关闭源位置 API | `is_source_location_enabled()` 返回值 |

> **注意**：CMake 选项在编译期决定相关 API 是否可用。若编译期关闭，运行时配置项将标记为 `[[deprecated]]` 并返回默认值。

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/config/error_config_test.cc` | 11 | 配置项设置/获取、per-code 覆盖、notify_mode、默认值、重置、并发 |

---

## 配置最佳实践

### 1. 开发环境配置

```cpp
error_config_t::set_stacktrace_level(error_level_t::debug);
error_config_t::set_enable_stacktrace(true);
error_config_t::set_enable_validation(true);
error_config_t::set_enable_source_location(true);
error_config_t::set_enable_short_filename(true);
error_config_t::set_enable_text_output(true);
```

### 2. 生产环境配置

```cpp
error_config_t::set_stacktrace_level(error_level_t::error);
error_config_t::set_enable_validation(false);
error_config_t::set_enable_source_location(true);
error_config_t::set_enable_short_filename(true);
error_config_t::set_enable_text_output(true);

// 低延迟服务：使用异步插件通知
error_config_t::set_notify_mode(error_config_t::notify_mode_t::async_queue);
error_config_t::set_max_queue_size(10000);   // v2.1 背压保护
```

### 3. 高性能场景配置

```cpp
error_config_t::set_enable_stacktrace(false);
error_config_t::set_enable_source_location(false);
error_config_t::set_enable_validation(false);
error_config_t::set_enable_text_output(false);  // 数字 ID 输出（更快）
error_config_t::set_notify_mode(error_config_t::notify_mode_t::async_queue);
error_config_t::set_max_queue_size(0);  // v2.1 无限制（默认）
```

### 4. 差异化堆栈策略

```cpp
// 全局：仅 ERROR 级别以上捕获堆栈
error_config_t::set_stacktrace_level(error_level_t::error);

// 关键错误码：总是捕获堆栈（包括 DEBUG）
error_config_t::set_per_code_stacktrace_level(ERR_CRITICAL.get_identity_code(), error_level_t::debug);

// 大量发生的可恢复错误：完全关闭堆栈
error_config_t::set_per_code_stacktrace_level(ERR_RATE_LIMIT.get_identity_code(), error_level_t::fatal);
```