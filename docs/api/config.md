# Config 层 API

> `error_system::config`

原 `error_config_t` 已按单一职责原则（SRP）拆分为三个独立配置类。`error_config.h` 仅作向后兼容的统一包含入口。

---

## feature_flags_t

特性开关配置类 — 管理编译期特性开关与运行时布尔标志位。

> 所有特性开关编译期决议，分支由编译器死代码消除。类仅包含静态成员，禁止实例化。

### 编译期常量

```cpp
class feature_flags_t {
public:
    static constexpr bool STACKTRACE_ENABLED = /* ERROR_SYSTEM_ENABLE_STACKTRACE */;
    static constexpr bool VALIDATION_ENABLED = /* ERROR_SYSTEM_ENABLE_VALIDATION */;
    static constexpr bool LOCATION_ENABLED   = /* ERROR_SYSTEM_ENABLE_LOCATION */;

    // 通知模式枚举
    enum class notify_mode_t : uint8_t {
        sync = 0,         // 同步通知（默认）
        async_queue = 1,  // 异步队列通知
    };
};
```

### 运行时标志位

```cpp
class feature_flags_t {
public:
    // 堆栈追踪
    static void set_enable_stacktrace(bool enable) noexcept;
    static bool is_stacktrace_enabled() noexcept;

    // 错误码验证
    static void set_enable_validation(bool enable) noexcept;
    static bool is_validation_enabled() noexcept;

    // 源位置追踪
    static void set_enable_source_location(bool enable) noexcept;
    static bool is_source_location_enabled() noexcept;

    // 短文件名模式
    static void set_enable_short_filename(bool enable) noexcept;
    static bool is_short_filename_enabled() noexcept;

    // 文本输出（true=子系统/模块名称，false=原始 ID）
    static void set_enable_text_output(bool enable) noexcept;
    static bool is_text_output_enabled() noexcept;

    // 通知模式
    static void set_notify_mode(notify_mode_t mode) noexcept;
    static notify_mode_t get_notify_mode() noexcept;
};
```

> 若编译期未启用对应特性，相关 `set_*` 调用无实际操作，`is_*_enabled()` 始终返回 `false`，编译器死代码消除未启用分支。

---

## stacktrace_config_t

堆栈追踪配置类 — 管理全局堆栈阈值与 per-code 覆盖配置。

```cpp
class stacktrace_config_t {
public:
    stacktrace_config_t() = delete;  // 禁止实例化

    // 全局阈值
    static core::error_level_t get_stacktrace_level() noexcept;
    static void set_stacktrace_level(core::error_level_t level) noexcept;

    // per-code 覆盖
    static void set_per_code_stacktrace_level(uint64_t identity_code,
                                              core::error_level_t level) noexcept;
    static std::optional<core::error_level_t>
        get_per_code_stacktrace_level(uint64_t identity_code) noexcept;
    static void remove_per_code_stacktrace_level(uint64_t identity_code) noexcept;
};
```

### per-code 优先级

```
is_stacktrace_enabled() == false  →  不捕获
否则 → per-code 有值 ? per-code : 全局阈值
错误等级 >= 最终阈值 → 捕获堆栈
```

---

## formatter_config_t

自定义格式化器配置类 — 管理错误上下文的自定义格式化函数。

```cpp
using formatter_callback_t = std::function<std::string(const core::error_context_t&)>;

class formatter_config_t {
public:
    formatter_config_t() = delete;  // 禁止实例化

    // 设置自定义格式化函数（nullptr 清除，恢复默认）
    static void set_custom_formatter(formatter_callback_t formatter) noexcept;

    // 获取格式化函数副本（线程安全，调用方无需持锁）
    static formatter_callback_t get_custom_formatter() noexcept;
};
```

> 自定义格式化器在 `error_context_serializer_t::to_string()` 内部被调用，若返回非空则替换默认文本输出。

---

## 默认配置

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `min_stacktrace_level_` | `error` | 自动捕获堆栈的最低等级 |
| `enable_stacktrace_` | `true` | 堆栈追踪总开关 |
| `enable_validation_` | `true` | 错误码验证 |
| `enable_source_location_` | `true` | 源位置追踪 |
| `enable_short_filename_` | `true` | 短文件名模式 |
| `enable_text_output_` | `true` | 文本（子系统名）输出 |
| `notify_mode_` | `sync` | 插件通知模式 |

---

## 配置场景速查

**开发环境**

```cpp
using namespace error_system::config;

stacktrace_config_t::set_stacktrace_level(core::error_level_t::debug);
```

**生产环境**

```cpp
stacktrace_config_t::set_stacktrace_level(core::error_level_t::error);
feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::async_queue);
plugin::plugin_registry_t::instance().set_max_queue_size(10000);
```

**高性能场景**

```cpp
feature_flags_t::set_enable_stacktrace(false);
feature_flags_t::set_enable_source_location(false);
feature_flags_t::set_enable_text_output(false);  // 数字 ID 输出
```

**差异化堆栈**

```cpp
stacktrace_config_t::set_stacktrace_level(core::error_level_t::error);  // 全局：仅 error+

// 关键错误：总是捕获
stacktrace_config_t::set_per_code_stacktrace_level(
    ERR_CRITICAL.get_identity_code(), core::error_level_t::debug);

// 可恢复错误：关闭
stacktrace_config_t::set_per_code_stacktrace_level(
    ERR_RATE_LIMIT.get_identity_code(), core::error_level_t::fatal);
```

**自定义格式化器（logfmt 风格）**

```cpp
formatter_config_t::set_custom_formatter(
    [](const core::error_context_t& e) -> std::string {
        std::string out = "level=";
        out += core::to_string(e.get_code().get_level());
        out += " msg=\"" + e.message + "\"";
        e.for_each_payload([&](const std::string& k, const std::string& v) {
            out += " " + k + "=" + v;
        });
        return out;
    });

// 清除自定义格式化器，恢复默认
formatter_config_t::set_custom_formatter(nullptr);
```

---

## CMake 编译选项

| 选项 | 默认 | 编译期控制 |
|------|:---:|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | ON | 堆栈追踪 + per-code API |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | ON | 错误码验证 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | ON | 源位置追踪 |

> 编译期宏通过 `feature_flags_t::STACKTRACE_ENABLED` / `VALIDATION_ENABLED` / `LOCATION_ENABLED`（`public constexpr bool`）暴露，`error_context_t` 内部使用 `if constexpr` 替代 `#ifdef`，编译器死代码消除未启用分支。
