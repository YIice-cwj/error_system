# 📋 Config 层 API

> `error_system::config`

原 `error_config_t` 已按单一职责原则（SRP）拆分为四个独立配置类。`error_config.h` 仅作向后兼容的统一包含入口。

> 📝 新代码请直接包含对应的细分头文件，无需包含 `error_config.h`。

---

## ⚙️ feature_flags_t

特性开关配置类 — 管理编译期特性开关与运行时布尔标志位。

> 所有特性开关编译期决议，分支由编译器死代码消除。类仅包含静态成员，禁止实例化。

### 🧱 编译期常量

```cpp
class feature_flags_t {
public:
    static constexpr bool STACKTRACE_ENABLED = /* ERROR_SYSTEM_ENABLE_STACKTRACE */;
    static constexpr bool VALIDATION_ENABLED = /* ERROR_SYSTEM_ENABLE_VALIDATION */;
    static constexpr bool LOCATION_ENABLED   = /* ERROR_SYSTEM_ENABLE_LOCATION */;

    // 通知模式枚举
    enum class notify_mode_t : uint8_t {
        sync = 0,           // 同步通知（默认）
        async_queue = 1,    // 异步队列通知（后台线程消费）
        sync_deferred = 2,  // 同步延迟通知（线程本地缓冲 + 显式 flush）
    };
};
```

### 🔄 运行时标志位

```cpp
class feature_flags_t {
public:
    // 堆栈追踪
    static void set_enable_stacktrace(bool enable) noexcept;
    [[nodiscard]] static bool is_stacktrace_enabled() noexcept;

    // 错误码验证
    static void set_enable_validation(bool enable) noexcept;
    [[nodiscard]] static bool is_validation_enabled() noexcept;

    // 源位置追踪
    static void set_enable_source_location(bool enable) noexcept;
    [[nodiscard]] static bool is_source_location_enabled() noexcept;

    // 短文件名模式
    static void set_enable_short_filename(bool enable) noexcept;
    [[nodiscard]] static bool is_short_filename_enabled() noexcept;

    // 通知模式
    static void set_notify_mode(notify_mode_t mode) noexcept;
    [[nodiscard]] static notify_mode_t get_notify_mode() noexcept;
};
```

> 💡 文本/i18n 输出开关已统一迁移到 `i18n_config_t::set_enable_i18n`，`feature_flags_t` 不再保留 `set_enable_text_output`。

> 💡 若编译期未启用对应特性，相关 `set_*` 调用无实际操作，`is_*_enabled()` 始终返回 `false`，编译器死代码消除未启用分支。

---

## 📚 stacktrace_config_t

堆栈追踪配置类 — 管理全局堆栈阈值与 per-code 覆盖配置。

```cpp
class stacktrace_config_t {
public:
    stacktrace_config_t() = delete;  // 禁止实例化

    // 全局阈值
    [[nodiscard]] static core::error_level_t get_stacktrace_level() noexcept;
    static void set_stacktrace_level(core::error_level_t level) noexcept;

    // per-code 覆盖
    static void set_per_code_stacktrace_level(uint64_t identity_code,
                                              core::error_level_t level) noexcept;
    [[nodiscard]] static std::optional<core::error_level_t>
        get_per_code_stacktrace_level(uint64_t identity_code) noexcept;
    static void remove_per_code_stacktrace_level(uint64_t identity_code) noexcept;
};
```

### 🎯 per-code 优先级

```
is_stacktrace_enabled() == false  →  不捕获
否则 → per-code 有值 ? per-code : 全局阈值
错误等级 >= 最终阈值 → 捕获堆栈
```

> ⚠️ per-code 覆盖不影响全局 `is_stacktrace_enabled()` 判断；编译期未启用堆栈追踪时，所有 `set_*` 调用无操作，`get_*` 返回 `warn` / `std::nullopt`。

---

## 🎨 formatter_config_t

自定义格式化器配置类 — 管理错误上下文的自定义格式化函数。

```cpp
using formatter_callback_t = std::function<std::string(const core::error_context_t&)>;

class formatter_config_t {
public:
    formatter_config_t() = delete;  // 禁止实例化

    // 设置自定义格式化函数（nullptr 清除，恢复默认）
    static void set_custom_formatter(formatter_callback_t formatter) noexcept;

    // 获取格式化函数副本（线程安全，调用方无需持锁）
    [[nodiscard]] static formatter_callback_t get_custom_formatter() noexcept;
};
```

> 💡 自定义格式化器在 `error_context_serializer_t::to_string()` 内部被调用，若返回非空则替换默认文本输出。

---

## 🌐 i18n_config_t

> 头文件：`error_system/config/i18n_config.h`

i18n 配置类 — 管理 i18n 启用开关与输出 locale 配置。从 `error_config_t` 拆分而来，单一职责。

> 💡 类仅含静态成员，禁止实例化。使用 `std::atomic` 无锁读写（`atomic<locale_t>` 部分平台不支持，改用 `uint8_t` 存储）。本类**仅持有配置状态**，不直接调用 `i18n_t` / `error_registry_t` — 序列化器从本类读取 locale 后显式传给 registry / i18n_t 查询本地化文本。

```cpp
class i18n_config_t {
public:
    i18n_config_t() = delete;  // 禁止实例化

    // i18n 总开关（false 时序列化输出回退为原始 ID 数字）
    static void set_enable_i18n(bool enable) noexcept;
    [[nodiscard]] static bool is_i18n_enabled() noexcept;

    // 默认 locale（回退查询使用）
    static void set_default_locale(i18n::locale_t locale) noexcept;
    [[nodiscard]] static i18n::locale_t get_default_locale() noexcept;

    // 输出 locale（显式设置后优先使用）
    static void set_output_locale(i18n::locale_t locale) noexcept;
    static void clear_output_locale() noexcept;                       // 回退到 default_locale
    [[nodiscard]] static std::optional<i18n::locale_t> get_output_locale() noexcept;

    // 解析最终输出 locale：output_locale（若已设置）→ default_locale
    [[nodiscard]] static i18n::locale_t resolve_output_locale() noexcept;
};
```

**locale 解析顺序**

```
resolve_output_locale()
1. output_locale 已设置 → 返回 output_locale
2. 否则 → 返回 default_locale
```

**使用示例**

```cpp
using namespace error_system::config;
using error_system::i18n::locale_t;

i18n_config_t::set_enable_i18n(true);                  // 启用 i18n（默认）
i18n_config_t::set_default_locale(locale_t::zh_CN);    // 默认中文

i18n_config_t::set_output_locale(locale_t::en_US);     // 运行时切换英文
auto loc = i18n_config_t::resolve_output_locale();     // → en_US

i18n_config_t::clear_output_locale();                  // 回退到默认
i18n_config_t::resolve_output_locale();                // → zh_CN

i18n_config_t::set_enable_i18n(false);                 // 关闭，序列化输出数字 ID
```

---

## 📊 默认配置

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

## 🚦 三种通知模式对比

| 模式 | 触发时机 | 适用场景 | 阻塞 |
|------|---------|---------|:----:|
| 🔵 `sync` | `error_context_t` 构造时立即调用所有插件 | 默认 / 调试 | 是 |
| 🟡 `async_queue` | 通知推入内部队列，由工作线程消费 | 生产 / 高吞吐 | 否 |
| 🟣 `sync_deferred` | 累积到线程本地缓冲，调用方显式 `flush_deferred_notifications()` | 批处理 / 请求边界 | 否 |

> 📝 三种通知模式的选择决策树详见 [决策树 · 1](../decision_tree.md#1-通知模式选择)。

---

## 🧭 配置场景速查

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

**请求处理批处理（sync_deferred）**

```cpp
// 在请求/事务边界累积通知，避免在错误处理中触发 I/O
feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync_deferred);
plugin::plugin_registry_t::instance().set_deferred_buffer_size(1024);

// ... 请求处理（构造 error_context_t 自动入线程本地缓冲）...

// 请求结束前显式 flush，触发插件 on_error()
plugin::plugin_registry_t::instance().flush_deferred_notifications();

if (plugin::plugin_registry_t::instance().deferred_buffer_overflowed()) {
    // 缓冲满，部分通知被丢弃
}
```

**高性能场景**

```cpp
feature_flags_t::set_enable_stacktrace(false);
feature_flags_t::set_enable_source_location(false);
i18n_config_t::set_enable_i18n(false);  // 数字 ID 输出
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

## 🔧 CMake 编译选项

| 选项 | 默认 | 编译期控制 |
|------|:---:|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | ON | 堆栈追踪 + per-code API |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | ON | 错误码验证 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | ON | 源位置追踪 |

> 📝 编译期宏通过 `feature_flags_t::STACKTRACE_ENABLED` / `VALIDATION_ENABLED` / `LOCATION_ENABLED`（`public constexpr bool`）暴露，`error_context_t` 内部使用 `if constexpr` 替代 `#ifdef`，编译器死代码消除未启用分支。
