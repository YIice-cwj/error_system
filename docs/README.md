# 📚 文档索引

---

## 📖 API 参考

| 📄 文档 | 📝 内容 |
|------|------|
| [🧩 Core 层](api/core.md) | `error_code_t` `error_context_t` `result_t` `error_registry_t` 等 |
| [⚙️ Config 层](api/config.md) | 全局配置、per-code 覆盖、通知模式 |
| [🔌 Plugin 层](api/plugin.md) | 插件接口、注册表、路由分发、开发指南 |
| [🛠️ Utils 层](api/utils.md) | 字符串工具、JSON 解析、文件操作、异步队列、堆栈跟踪 |

## 🏗️ 设计文档

| 📄 文档 | 📝 内容 |
|------|------|
| [架构设计](architecture.md) | 分层架构、模块职责、关键设计决策、编译配置 |

## 🧭 快速导航

- 🔢 **构建错误码** → [error_code_t](api/core.md#error_code_t)
- 📦 **错误上下文** → [error_context_t](api/core.md#error_context_t)
- 🎯 **Result 错误传递** → [result_t](api/core.md#result_tt)
- 📋 **错误码注册** → [error_registry_t](api/core.md#error_registry_t)
- ⚙️ **全局配置** → [error_config_t](api/config.md)
- 🔌 **异步插件通知** → [plugin_registry_t](api/plugin.md#plugin_registry_t)
- 🤖 **代码生成工具** → [架构设计](architecture.md#14-代码生成)

## 🧪 运行测试

```bash
cd build && ctest --output-on-failure
```

| 模块 | 文件数 | 用例数 |
|------|:---:|:---:|
| 🧩 Core | 7 | 100+ |
| 🔌 Plugin | 2 | 20 |
| 🛠️ Utils | 4 | 37+ |
| ⚙️ Config | 1 | 11 |
| 🌐 Domain | 1 | 3 |
| **📊 总计** | **15** | **271** |