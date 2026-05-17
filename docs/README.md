# 文档索引

欢迎来到 Error System 文档中心。

---

## API 参考

| 文档 | 命名空间 | 描述 |
|------|----------|------|
| [Core 层](api/core.md) | `error_system::core` | 错误码、等级、上下文、Result、异常、注册表 |
| [Config 层](api/config.md) | `error_system::config` | 全局配置、堆栈追踪、验证、自定义格式化 |
| [Plugin 层](api/plugin.md) | `error_system::plugin` | 插件系统、日志/统计扩展 |
| [Memory 层](api/memory.md) | `error_system::memory` | 对象池、内存管理优化 |
| [Utils 层](api/utils.md) | `error_system::utils` | 字符串工具、JSON 解析、文件操作、堆栈跟踪 |

## 设计文档

| 文档 | 描述 |
|------|------|
| [架构设计](architecture.md) | 模块划分、依赖关系、关键设计决策 |

## 快速导航

- **构建错误码** → [Core API: error_builder_t](api/core.md#error_builder_t)
- **使用错误上下文** → [Core API: error_context_t](api/core.md#error_context_t)
- **错误码注册** → [Core API: error_registry_t](api/core.md#error_registry_t)
- **全局配置** → [Config API](api/config.md)
- **接入日志/统计** → [Plugin API](api/plugin.md)
- **对象池优化** → [Memory API: object_pool_t](api/memory.md#object_pool_t)
- **字符串格式化** → [Utils API: string_utils_t](api/utils.md#string_utils_t)
- **代码生成工具** → [架构设计：代码生成](architecture.md#13-代码生成工具)

## 测试覆盖

所有 API 文档均包含对应的单元测试说明，运行测试：

```bash
cd build
ctest --output-on-failure
```

测试统计：

| 模块 | 测试文件数 | 测试用例数 |
|------|-----------|-----------|
| Core 层 | 7 | 59+ |
| Plugin 层 | 2 | 17 |
| Memory 层 | 1 | 10 |
| Utils 层 | 5 | 37+ |
| Config 层 | 1 | 7 |
| **总计** | **16** | **199** |
