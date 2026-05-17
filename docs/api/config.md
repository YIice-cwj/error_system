# Config 层 API 文档

> 命名空间：`error_system::config`

Config 层提供全局错误配置管理，包括堆栈追踪级别、验证开关、源位置追踪、自定义格式化器和翻译器等。

---

## error_config_t

**头文件**：`error_system/config/error_config.h`

全局错误配置类，所有方法均为静态方法，提供进程级的错误行为配置。

### 配置项

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `min_stacktrace_level_` | `error_level_t` | `error` | 自动捕获堆栈的最低等级 |
| `enable_stacktrace_` | `bool` | `true` | 堆栈追踪总开关 |
| `enable_validation_` | `bool` | `true` | 错误码验证开关 |
| `enable_source_location_` | `bool` | `true` | 源位置追踪开关 |
| `enable_short_filename_` | `bool` | `true` | 短文件名模式 |
| `custom_formatter_` | `formatter_callback_t` | `nullptr` | 自定义格式化回调 |
| `custom_translator_` | `translator_func_t` | `nullptr` | 自定义翻译函数 |

### 类型定义

```cpp
// 自定义格式化回调类型
using formatter_callback_t = std::function<std::string(const core::error_context_t&)>;

// 自定义翻译函数类型
using translator_func_t = std::function<std::string(uint16_t subsys_id, uint16_t module_id)>;
```

### 方法

#### 堆栈追踪配置

| 方法 | 返回值 | 描述 |
|------|--------|------|
| `set_stacktrace_level(error_level_t level)` | `void` | 设置自动捕获堆栈的最低等级 |
| `get_stacktrace_level()` | `error_level_t` | 获取自动捕获堆栈的最低等级 |
| `set_enable_stacktrace(bool enable)` | `void` | 设置堆栈追踪总开关 |
| `is_stacktrace_enabled()` | `bool` | 检查堆栈追踪是否启用 |

#### 验证配置

| 方法 | 返回值 | 描述 |
|------|--------|------|
| `set_enable_validation(bool enable)` | `void` | 设置错误码验证开关 |
| `is_validation_enabled()` | `bool` | 检查错误码验证是否启用 |

#### 源位置配置

| 方法 | 返回值 | 描述 |
|------|--------|------|
| `set_enable_source_location(bool enable)` | `void` | 设置源位置追踪开关 |
| `is_source_location_enabled()` | `bool` | 检查源位置追踪是否启用 |
| `set_enable_short_filename(bool enable)` | `void` | 设置短文件名模式 |
| `is_short_filename_enabled()` | `bool` | 检查短文件名模式是否启用 |

#### 自定义格式化

| 方法 | 返回值 | 描述 |
|------|--------|------|
| `set_custom_formatter(formatter_callback_t formatter)` | `void` | 设置自定义格式化回调 |
| `get_custom_formatter()` | `formatter_callback_t` | 获取自定义格式化回调 |

#### 自定义翻译

| 方法 | 返回值 | 描述 |
|------|--------|------|
| `set_translator(translator_func_t translator)` | `void` | 设置自定义翻译函数 |
| `get_translator()` | `translator_func_t` | 获取自定义翻译函数 |

### 示例

```cpp
#include "error_system/config/error_config.h"
using namespace error_system::config;

// 配置堆栈追踪
error_config_t::set_stacktrace_level(error_level_t::warn);
error_config_t::set_enable_stacktrace(true);

// 配置验证
error_config_t::set_enable_validation(true);

// 配置源位置
error_config_t::set_enable_source_location(true);
error_config_t::set_enable_short_filename(true);

// 设置自定义格式化器
error_config_t::set_custom_formatter([](const core::error_context_t& ctx) {
    return "[CUSTOM] " + ctx.message;
});

// 设置自定义翻译函数
error_config_t::set_translator([](uint16_t subsys_id, uint16_t module_id) -> std::string {
    // 自定义子系统和模块名称映射
    return fmt::format("子系统[{}]/模块[{}]", subsys_id, module_id);
});
```

### 自定义翻译函数说明

自定义翻译函数用于在 `error_context_t::to_string()` 中生成子系统和模块的描述字符串。

**默认行为**：
```
SubSys: 101, Module: 1
```

**自定义后**：
```
交易服务/订单模块
```

**使用场景**：
- 国际化（i18n）支持
- 业务名称映射
- 动态配置加载

---

## 编译期配置选项

通过 CMake 选项控制功能开关：

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | `ON` | 启用堆栈追踪功能 |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | `ON` | 启用错误码验证 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | `ON` | 启用源位置追踪 |

关闭后相关 API 将标记为 `[[deprecated]]` 并返回默认值，实现零开销的编译期裁剪。

---

## 单元测试

测试文件：`tests/config/error_config_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `default_stacktrace_level_is_error` | 默认堆栈等级为 error |
| `set_stacktrace_level_changes_value` | 设置堆栈等级生效 |
| `default_validation_is_enabled` | 默认启用验证 |
| `set_validation_enabled_changes_value` | 设置验证开关生效 |
| `default_source_location_is_enabled` | 默认启用源位置 |
| `set_source_location_enabled_changes_value` | 设置源位置开关生效 |
| `custom_formatter_can_be_set_and_retrieved` | 自定义格式化器可设置和获取 |

---

## 相关文档

- [Core 层 API](./core.md) - 错误上下文和错误码
- [架构设计](../architecture.md) - 系统架构说明
