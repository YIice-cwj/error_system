# ⚙️ Config 层 API

> `error_system::config`

---

## 🎛️ error_config_t

全局错误配置，所有方法 `static`，无需实例化。

### API

```cpp
class error_config_t {
public:
    // 🔧 编译期特性开关（public constexpr bool，每个宏仅判断一次）
    static constexpr bool STACKTRACE_ENABLED = /* ERROR_SYSTEM_ENABLE_STACKTRACE */;
    static constexpr bool VALIDATION_ENABLED = /* ERROR_SYSTEM_ENABLE_VALIDATION */;
    static constexpr bool LOCATION_ENABLED   = /* ERROR_SYSTEM_ENABLE_LOCATION */;

    // 🔍 堆栈追踪
    static void set_stacktrace_level(error_level_t) noexcept;
    static error_level_t get_stacktrace_level() noexcept;
    static void set_enable_stacktrace(bool) noexcept;
    static bool is_stacktrace_enabled() noexcept;

    // 🎯 per-code 覆盖（v2.0）
    static void set_per_code_stacktrace_level(uint64_t identity_code, error_level_t) noexcept;
    static std::optional<error_level_t> get_per_code_stacktrace_level(uint64_t) noexcept;
    static void remove_per_code_stacktrace_level(uint64_t) noexcept;

    // ✅ 验证
    static void set_enable_validation(bool) noexcept;
    static bool is_validation_enabled() noexcept;

    // 📍 源位置
    static void set_enable_source_location(bool) noexcept;
    static bool is_source_location_enabled() noexcept;
    static void set_enable_short_filename(bool) noexcept;
    static bool is_short_filename_enabled() noexcept;

    // 📤 输出格式
    static void set_enable_text_output(bool) noexcept;
    static bool is_text_output_enabled() noexcept;

    // 🔀 通知模式
    enum class notify_mode_t : uint8_t { sync = 0, async_queue = 1 };
    static void set_notify_mode(notify_mode_t) noexcept;
    static notify_mode_t get_notify_mode() noexcept;

    // 🎨 自定义格式化
    static void set_custom_formatter(
        std::function<std::string(const error_context_t&)> formatter) noexcept;
};
```

### 📋 默认配置

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `min_stacktrace_level_` | `error` | 自动捕获堆栈的最低等级 |
| `enable_stacktrace_` | `true` | 堆栈追踪总开关 |
| `enable_validation_` | `true` | 错误码验证 |
| `enable_source_location_` | `true` | 源位置追踪 |
| `enable_short_filename_` | `true` | 短文件名模式 |
| `enable_text_output_` | `true` | 文本（子系统名）输出 |
| `notify_mode_` | `sync` | 插件通知模式 |

### 🎯 per-code 优先级

```
is_stacktrace_enabled() == false  →  ❌ 不捕获
否则 → per-code 有值 ? per-code : 全局阈值
错误等级 >= 最终阈值 → ✅ 捕获堆栈
```

---

## ⚡ 配置场景速查

**🛠️ 开发环境**

```cpp
error_config_t::set_stacktrace_level(error_level_t::debug);
```

**🏭 生产环境**

```cpp
error_config_t::set_stacktrace_level(error_level_t::error);
error_config_t::set_notify_mode(error_config_t::notify_mode_t::async_queue);
plugin_registry_t::instance().set_max_queue_size(10000);
```

**🔥 高性能场景**

```cpp
error_config_t::set_enable_stacktrace(false);
error_config_t::set_enable_source_location(false);
error_config_t::set_enable_text_output(false);  // 数字 ID 输出
```

**🎯 差异化堆栈**

```cpp
error_config_t::set_stacktrace_level(error_level_t::error);           // 全局：仅 error+

error_config_t::set_per_code_stacktrace_level(                        // 关键错误：总是捕获
    ERR_CRITICAL.get_identity_code(), error_level_t::debug);

error_config_t::set_per_code_stacktrace_level(                        // 可恢复错误：关闭
    ERR_RATE_LIMIT.get_identity_code(), error_level_t::fatal);
```

---

## 🔧 CMake 编译选项

| 选项 | 默认 | 编译期控制 |
|------|:---:|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | ON | 堆栈追踪 + per-code API |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | ON | 错误码验证 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | ON | 源位置追踪 |

> 💡 编译期宏通过 `error_config_t::STACKTRACE_ENABLED` / `VALIDATION_ENABLED` / `LOCATION_ENABLED`（`public constexpr bool`）暴露，`error_context_t` 内部使用 `if constexpr` 替代 `#ifdef`，编译器死代码消除未启用分支。