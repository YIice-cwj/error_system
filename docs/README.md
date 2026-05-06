# 文档索引

欢迎来到 Error System 文档中心。

---

## API 参考

| 文档 | 命名空间 | 描述 |
|------|----------|------|
| [Core 层](api/core.md) | `error_system::core` | 错误码、等级、上下文、Result、异常、注册表 |
| [i18n 层](api/i18n.md) | `error_system::i18n` | 多语言翻译、翻译器接口与注册 |
| [Plugin 层](api/plugin.md) | `error_system::plugin` | 插件系统、日志/统计扩展 |
| [Utils 层](api/utils.md) | `error_system::utils` | 字符串工具、JSON 解析、文件操作、堆栈跟踪 |

## 设计文档

| 文档 | 描述 |
|------|------|
| [架构设计](architecture.md) | 模块划分、依赖关系、关键设计决策、扩展指南 |

## 快速导航

- **构建错误码** → [Core API: error_builder_t](api/core.md#error_builder_t)
- **翻译错误码** → [i18n API: json_translator_t](api/i18n.md#json_translator_t)
- **注册全局翻译器** → [i18n API: translator_registry_t](api/i18n.md#translator_registry_t)
- **接入日志/统计** → [Plugin API](api/plugin.md)
- **字符串格式化** → [Utils API: string_utils_t](api/utils.md#string_utils_t)
- **新增系统域/语言/插件** → [架构设计：扩展指南](architecture.md#扩展指南)
