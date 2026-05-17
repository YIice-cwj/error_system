# Translator 层 API 文档

> 命名空间：`error_system::translator`

Translator 层提供错误码翻译功能，将子系统ID和模块ID转换为可读的字符串描述，支持静态和动态两种注册方式。

---

## error_translator_t

**头文件**：`error_system/translator/error_translator.h`

错误翻译器单例类，用于管理和查询子系统、模块的名称映射。

### 方法

| 方法 | 返回值 | 描述 |
|------|--------|------|
| `instance()` | `error_translator_t&` | 获取单例实例 |
| `register_subsystem(uint16_t id, std::string name)` | `void` | 动态注册子系统名称 |
| `register_module(uint16_t subsys_id, uint16_t module_id, std::string name)` | `void` | 动态注册模块名称 |
| `translate(uint16_t subsys_id, uint16_t module_id)` | `std::string` | 翻译为可读字符串 |

### 静态注册宏

```cpp
REGISTER_LEGACY_MODULE(SUBSYS_ID, MODULE_ID, SUBSYS_NAME, MODULE_NAME)
```

在静态初始化阶段自动注册子系统和模块描述。

### 示例

```cpp
#include "error_system/translator/error_translator.h"
using namespace error_system::translator;

// 方式1：使用宏静态注册（推荐用于已知模块）
REGISTER_LEGACY_MODULE(101, 1, "交易服务", "订单模块");
REGISTER_LEGACY_MODULE(102, 1, "支付服务", "支付网关");

// 方式2：动态注册（用于运行时配置）
auto& translator = error_translator_t::instance();
translator.register_subsystem(201, "用户服务");
translator.register_module(201, 1, "认证模块");

// 使用翻译器
std::string desc = translator.translate(101, 1);
// 输出: SubSys: 交易服务, Module: 订单模块
```

---

## 辅助函数

### get_static_subsys_name

```cpp
std::string_view get_static_subsys_name(uint16_t subsys_id) noexcept;
```

获取静态注册的子系统名称。

### get_static_module_name

```cpp
std::string_view get_static_module_name(uint16_t subsys_id, uint16_t module_id) noexcept;
```

获取静态注册的模块名称。

### 示例

```cpp
auto subsys_name = get_static_subsys_name(101);  // "交易服务"
auto module_name = get_static_module_name(101, 1);  // "订单模块"
```

---

## 名称查找优先级

1. **静态注册表** - 编译期确定的名称（通过 `get_static_*_name`）
2. **动态注册表** - 运行时注册的名称（通过 `register_*`）
3. **数字ID** - 未注册时返回数字ID字符串

---

## 单元测试

测试文件：`tests/translator/error_translator_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `instance_returns_singleton` | 单例模式正确 |
| `register_subsystem_stores_name` | 子系统注册生效 |
| `register_module_stores_name` | 模块注册生效 |
| `translate_unknown_subsystem_returns_id` | 未知子系统返回ID |
| `translate_unknown_module_returns_id` | 未知模块返回ID |
| `get_static_subsys_name_returns_known_name` | 静态子系统名称查询 |
| `get_static_module_name_returns_known_name` | 静态模块名称查询 |
| `translate_uses_static_names_when_available` | 优先使用静态名称 |
| `register_overrides_existing` | 重复注册覆盖 |

---

## 相关文档

- [Core 层 API](./core.md) - 错误上下文和错误码
- [Config 层 API](./config.md) - 全局配置
