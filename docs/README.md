# 文档索引

错误系统全量文档导航中心，按用途分类速查。

---

## 📖 API 参考

| 文档 | 内容 |
|------|------|
| [Core 层](api/core.md) | `error_code_t` `error_context_t` `result_t` `error_registry_t` `error_context_serializer_t` `error_builder_t` |
| [i18n 层](api/i18n.md) | `locale_t` `subsystem_module_catalog_t` `i18n_t` 多语言消息回退 |
| [Migration 层](api/migration.md) | `error_migration_registry_t` 废弃标记、单跳/递归迁移 |
| [Mapping 层](api/mapping.md) | `http_status_t` `grpc_status_t` `status_mapper_t` HTTP/gRPC 映射 |
| [Config 层](api/config.md) | `feature_flags_t` `stacktrace_config_t` `formatter_config_t` `i18n_config_t` 三种通知模式 |
| [Plugin 层](api/plugin.md) | 插件接口、注册表、路由分发、延迟通知、开发指南 |
| [Utils 层](api/utils.md) | 字符串工具、JSON 解析、文件操作、异步队列、堆栈跟踪 |

## 🏗️ 设计文档

| 文档 | 内容 |
|------|------|
| [架构设计](architecture.md) | 分层架构、模块职责、关键设计决策（21 项）、编译配置 |
| [错误码自动生成](error_code_generation.md) | JSON 配置格式、生成脚本、CMake 集成、自定义错误码 |

## 🌳 决策树

| 文档 | 内容 |
|------|------|
| [决策树](decision_tree.md) | 通知模式选择、查询路径选择、废弃/迁移、i18n 回退、序列化格式、插件开发、错误传递、HTTP/gRPC 映射 |

## 🧭 快速导航

- **构建错误码** → [error_code_t](api/core.md#error_code_t)
- **错误上下文** → [error_context_t](api/core.md#error_context_t)
- **Result 错误传递** → [result_t](api/core.md#result_tt)
- **错误码注册** → [error_registry_t](api/core.md#error_registry_t)
- **多语言消息** → [i18n_t](api/i18n.md#i18n_t)
- **废弃/迁移** → [error_migration_registry_t](api/migration.md#error_migration_registry_t)
- **HTTP/gRPC 映射** → [status_mapper_t](api/mapping.md#status_mapper_t)
- **全局配置** → [feature_flags_t](api/config.md#feature_flags_t)
- **插件通知** → [plugin_registry_t](api/plugin.md#plugin_registry_t)
- **选型决策** → [决策树](decision_tree.md)
- **代码生成工具** → [架构设计](architecture.md#16-代码生成)

## 🧪 运行测试

在 `build` 目录下执行 `ctest` 即可跑通全量用例。

```bash
cd build && ctest --output-on-failure
```

| 模块 | 文件数 | 用例数 |
|------|:---:|:---:|
| Core | 7 | 171 |
| Plugin | 3 | 52 |
| Utils | 5 | 129 |
| Config | 2 | 27 |
| Domain | 1 | 9 |
| i18n | 1 | 27 |
| Mapping | 1 | 22 |
| Migration | 1 | 32 |
| **总计** | **21** | **469** |

另有 `tests/perf/` 8 个性能基准场景（共享 `perf_common.h`）：

| 场景 | 文件 | 覆盖范围 |
|---|---|---|
| 基线（全关） | `scenario_baseline_benchmark.cc` | 构造/序列化(to_string·to_json·to_binary)/拷贝/移动 |
| 仅栈追踪 | `scenario_stacktrace_benchmark.cc` | 同上 + 栈追踪开销 |
| 仅插件 | `scenario_plugin_benchmark.cc` | 同上 + 同步通知开销 |
| 栈追踪+插件 | `scenario_stacktrace_plugin_benchmark.cc` | 两特性叠加 |
| 全特性 | `scenario_full_benchmark.cc` | 栈追踪+位置+短文件名+插件 |
| 错误路由插件 | `scenario_router_benchmark.cc` | 按错误码/模块组/系统域命中分发·未命中·注册 |
| 去重采样器 | `scenario_dedup_benchmark.cc` | 全放行/纯去重/纯采样/叠加/多码轮询 |
| i18n 翻译模块 | `scenario_i18n_benchmark.cc` | 单条/批量注册·查询命中/回退/未命中·目录注册查询·locale 转换 |
