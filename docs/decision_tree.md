# 决策树

> 🌳 面向使用者的场景化选型指南。每棵决策树覆盖一个常见决策点，给出推荐路径与取舍说明。

---

## 1. 🔔 通知模式选择

何时使用 `sync` / `async_queue` / `sync_deferred` 三种通知模式。

```
是否在 error_context_t 构造时立即通知插件？
├─ 是 → 插件 on_error() 是否可能阻塞（I/O、网络、磁盘）？
│       ├─ 否 → sync                  ← 日志、内存计数、指标收集
│       └─ 是 → 是否可接受通知丢失（队列满拒绝）？
│               ├─ 是 → async_queue   ← 网络上报、持久化、外发告警
│               └─ 否 → sync_deferred ← 请求处理批处理、事务内累积
└─ 否（显式控制 flush 时机）→ sync_deferred
```

| 🏷️ 模式 | ⏰ 调用时机 | ⚠️ 丢失风险 | 🎯 适用场景 |
|------|----------|:---:|------|
| `sync` | 构造 `error_context_t` 时同步调用 | 无 | 日志、指标（轻量、非阻塞） |
| `async_queue` | 构造时入队，后台线程异步消费 | 队列满时拒绝入队 | 网络上报、持久化（可能阻塞） |
| `sync_deferred` | 构造时入线程本地缓冲，显式 `flush_deferred_notifications()` | 缓冲满时丢弃（标记 overflow） | 请求处理内累积、事务边界批处理 |

**切换示例**

```cpp
using namespace error_system::config;
feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync);          // 默认
feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::async_queue);   // 后台消费
feature_flags_t::set_notify_mode(feature_flags_t::notify_mode_t::sync_deferred); // 累积 + flush
plugin::plugin_registry_t::instance().flush_deferred_notifications();            // 请求结束前 flush
```

> 📝 详见 [Plugin 层 API](api/plugin.md#通知模式)。

---

## 2. 🔍 错误码元数据查询路径选择

何时使用 `get_info()` vs `get_info_cached()`。

```
是否处于热路径（每次错误构造/校验都会查询）？
├─ 否 → 是否需要绝对实时一致（查询前刚 unregister）？
│       ├─ 是 → get_info()           ← 每次直接读注册表
│       └─ 否 → get_info()           ← 简单场景无需缓存
└─ 是 → 错误码注册后是否在运行时频繁注销？
        ├─ 是 → get_info()           ← 缓存频繁失效反而更慢
        └─ 否 → get_info_cached()    ← 线程本地缓存，命中零锁开销
```

| 🛤️ 路径 | 🔒 锁开销 | 📐 一致性 | 🎯 适用场景 |
|------|:---:|:---:|------|
| `get_info()` | `shared_lock` | 强一致 | 管理工具、偶尔查询 |
| `get_info_cached()` | 命中时零锁，未命中一次 `shared_lock` | 纪元失效后最终一致 | `error_context_t` 构造、`fill_validation_fields_` |

```cpp
auto& reg = core::error_registry_t::instance();
auto meta = reg.get_info_cached(code);  // 热路径：命中零锁
auto fresh = reg.get_info(code);        // 管理工具：强一致
reg.invalidate_metadata_cache();        // 显式刷新（测试场景）
```

> 💡 纪元失效机制：任何 `register_*` / `unregister_*` 调用都会 `bump_epoch_()`（release 序），线程本地缓存检测纪元变化（acquire 序）后整体失效重建，同时缓存"已注册"与"未注册"结果。
>
> 📝 详见 [Core 层 API · error_registry_t](api/core.md#error_registry_t)。

---

## 3. 🔁 错误码废弃与迁移决策

何时使用 `mark_deprecated()` / `register_migration()` / 单跳 vs 递归迁移。

```
该错误码是否需要下线？
├─ 否 → 是否需要别名映射（旧码与新码共存，查询旧码自动跳到新码）？
│       ├─ 是 → register_migration()      ← 仅建立映射，不标记废弃
│       └─ 否 → 无需操作
└─ 是 → 是否有直接替代码？
        ├─ 是 → mark_deprecated(code, {reason, replacement, ...})
        │       └─ 同时自动建立 migration 映射
        └─ 否 → mark_deprecated(code, {reason})   ← 仅标记废弃，无替代
```

**单跳 vs 递归迁移**

```
是否需要沿迁移链跳到终点？
├─ 否（只需一次映射） → migrate()       ← a → b（即使 b 也有映射也停止）
└─ 是（需要最终码）   → migrate_recursive()  ← a → b → c → ... → 终点
    └─ 内置环检测：最大深度 16，超限返回当前码
```

```cpp
auto& reg = migration::error_migration_registry_t::instance();
reg.mark_deprecated(ERR_OLD_DB_POOL, {"v2.0 起改用 V2", ERR_DB_POOL_V2, "2.0.0", "3.0.0"});
reg.mark_deprecated(ERR_LEGACY_AUTH, {"鉴权流程已重构，下版本移除"});   // 仅标记
reg.register_migration(ERR_USER_NOT_FOUND_V1, ERR_USER_NOT_FOUND_V2); // 别名映射
auto migrated = reg.migrate(ERR_OLD_DB_POOL);          // 单跳
auto terminal = reg.migrate_recursive(ERR_OLD_DB_POOL); // 递归到终点
```

> ⚠️ 废弃状态与迁移映射**分离存储**：`unmark_deprecated()` 不会清除迁移映射，便于先停止废弃警告再逐步下线。
>
> 📝 详见 [Migration 层 API · error_migration_registry_t](api/migration.md#error_migration_registry_t)。

---

## 4. 🌐 i18n 消息查询回退路径

`i18n_t::get_message()` 的 locale 回退顺序。

```
查询消息 get_message(code)（使用 active locale）
├─ active locale 是否设置（非 nullopt）？
│   ├─ 是 → active locale 下是否有该 code 的消息？
│   │       ├─ 是 → 返回 active locale 的消息
│   │       └─ 否 → 继续回退
│   └─ 否 → 继续回退
└─ default locale 下是否有该 code 的消息？
    ├─ 是 → 返回 default locale 的消息
    └─ 否 → 返回空 string_view

查询消息 get_message(locale, code)（显式指定 locale）
└─ 指定 locale 下是否有该 code 的消息？
    ├─ 是 → 返回该 locale 的消息
    └─ 否 → 回退到 default locale（同上路径）
```

```cpp
using namespace error_system::i18n;
auto& catalog = i18n_t::instance();
catalog.set_default_locale(locale_t::zh_CN);
catalog.register_message(locale_t::zh_CN, ERR_DB_TIMEOUT, "数据库连接超时");
catalog.register_message(locale_t::en_US, ERR_DB_TIMEOUT, "Database connection timeout");
catalog.set_active_locale(locale_t::en_US);
catalog.get_message(ERR_DB_TIMEOUT);                          // → "Database connection timeout"
catalog.clear_active_locale();                                // 回退到 default
catalog.get_message(locale_t::fr_FR, ERR_DB_TIMEOUT);         // fr_FR 无 → 回退 zh_CN
```

> 💡 `clear_active_locale()` 表示不使用 active locale，直接走 default 回退。
>
> 📝 详见 [i18n 层 API · i18n_t](api/i18n.md#i18n_t)。

---

## 5. 📦 序列化格式选择

何时使用 `to_string()` / `to_json()` / `to_binary()`。

```
输出目标？
├─ 人类可读日志 / 终端 → to_string()
│   （含因果链 ↳ Caused by:，含堆栈/源位置）
├─ 结构化日志 / Web API → to_json()
│   （JSON 对象，code 为字符串，含 cause 递归字段）
└─ RPC 跨语言 / 持久化存储 → to_binary()
    （紧凑二进制，小端序，magic + version 头，含因果链）
```

| 🏷️ 格式 | 📦 体积 | 👁️ 可读性 | 🌍 跨语言 | 🔗 因果链 |
|------|:---:|:---:|:---:|:---:|
| `to_string()` | 大 | 优 | 否 | 含 |
| `to_json()` | 中 | 良 | 是（JSON） | 含 |
| `to_binary()` | 小 | 无 | 是（需对应解析器） | 含 |

```cpp
auto ctx = core::error_context_serializer_t::from_binary(binary_string);  // 校验 magic + version
auto ctx2 = core::error_context_serializer_t::from_json(json_string);     // JSON 格式校验
if (!ctx) { /* magic/version 不匹配或数据损坏 */ }
```

> 📝 详见 [Core 层 API · error_context_serializer_t](api/core.md#error_context_serializer_t)。

---

## 6. 🔌 插件开发模式选择

开发插件时的实现策略选择。

```
通知模式？
├─ sync / sync_deferred → on_error() 在调用线程执行
│   ├─ 是否需要按错误码/域/模块路由到不同处理函数？
│   │   ├─ 是 → 继承 error_router_plugin_t        ← 框架内置路由
│   │   └─ 否 → 自定义插件
│   └─ 是否需要去重/采样？
│       ├─ 是 → error_dedup_sampler_t              ← 内置去重+采样
│       └─ 否 → 自定义插件
└─ async_queue → on_error() 在后台工作线程执行
    └─ 是否需要跨进程/跨网络转发？
        ├─ 是 → 自定义插件 + 网络发送（I/O 在后台线程，安全）
        └─ 否 → 自定义插件
```

```cpp
class my_plugin_t : public i_error_plugin_t {
    core::error_level_t min_level() const noexcept override { return core::error_level_t::error; }  // 框架级过滤
    void on_error(const core::error_context_t& ctx) noexcept override { /* 仅收到 error+ */ }
    std::string_view name() const noexcept override { return "my_plugin"; }
};
```

> 📝 详见 [Plugin 层 API · 插件开发指南](api/plugin.md#插件开发指南)。

---

## 7. 🎯 错误传递方式选择

何时使用 `result_t<T>` vs `error_exception_t` vs 直接返回 `error_context_t`。

```
错误传递方式？
├─ 异常不可用 / 性能敏感 → result_t<T>
│   （零异常，variant + getif + 哨兵值，链式 map/and_then/or_else）
├─ 需要与异常生态集成 → error_exception_t
│   （std::exception 子类，what() 返回缓存消息）
└─ 仅需传递错误上下文 → error_context_t
    （值语义，可直接作为函数参数/返回值）
```

| 🏷️ 方式 | ⚡ 异常开销 | 🔗 链式操作 | 🔌 与异常生态兼容 | 🎯 适用场景 |
|------|:---:|:---:|:---:|------|
| `result_t<T>` | 无 | map/and_then/or_else/match | 否 | 库内部、性能敏感路径 |
| `error_exception_t` | 有 | 无 | 是 | 跨异常/非异常边界 |
| `error_context_t` | 无 | wrap（因果链） | 否 | 简单传递 |

```cpp
result_t<user_t> fetch_user(id_t id) noexcept {
    if (id == 0) return result_t<user_t>::make_error(ERR_INVALID_ID, "id 不能为 0");
    return result_t<user_t>::make_success(user_t{id});
}
auto name = fetch_user(42).map([](const user_t& u) { return u.name; }).value_or("unknown");

auto r = fetch_user(id);
if (r.is_error()) throw error_exception_t(r.error());  // 跨异常边界
```

> 📝 详见 [Core 层 API · result_t](api/core.md#result_tt) 与 [error_exception_t](api/core.md#error_exception_t)。

---

## 8. 🛰️ HTTP/gRPC 状态码映射选择

何时使用 `status_mapper_t`。

```
是否需要将错误码映射到 HTTP 状态码或 gRPC 状态码？
├─ 是 → 直接调用 status_mapper_t::to_http_status(code) / to_grpc_status(code)
│        ← 使用内置默认映射（constexpr，按错误等级 + 系统域）
└─ 否 → 不调用
```

> ⚠️ `status_mapper_t` 为纯静态工具类（不可实例化），所有映射方法均为 `constexpr`，**不支持运行时自定义映射**。如需特殊映射规则，请调用方自行判断后调用 `http_status_t::from_int()` / `grpc_status_t::from_int()` 构造。

**默认映射规则（按错误等级 + 系统域）**

| 📊 错误等级 | 🏷️ 系统域 | 🛰️ HTTP 状态码 | 📡 gRPC 状态码 |
|------|------|:---:|:---:|
| debug / info | * | 200 OK | OK |
| warn | * | 200 OK | OK |
| error（retryable / transient） | * | 503 Service Unavailable | UNAVAILABLE |
| error | application | 400 Bad Request | INVALID_ARGUMENT |
| error | database | 503 Service Unavailable | DATA_LOSS |
| error | middleware | 503 Service Unavailable | UNAVAILABLE |
| error | third_party | 502 Bad Gateway | UNAVAILABLE |
| error | system / none | 500 Internal Server Error | INTERNAL |
| fatal | * | 500 Internal Server Error | INTERNAL |

```cpp
using namespace error_system::mapping;
http_status_t http = status_mapper_t::to_http_status(ERR_DB_TIMEOUT);   // 503 Service Unavailable
grpc_status_t grpc = status_mapper_t::to_grpc_status(ERR_DB_TIMEOUT);  // UNAVAILABLE
http.c_str();      // "Service Unavailable"
http.to_int();     // 503
http_status_t::is_valid(404);  // true
// 如需特殊映射：调用方自行判断
http_status_t custom = http_status_t::is_valid(404) ? http_status_t::from_int(404)
                          : status_mapper_t::to_http_status(ERR_NOT_FOUND);
```

> 💡 retryable / transient 标志在所有等级上**优先**映射为 503 / UNAVAILABLE，提示客户端重试。
>
> 📝 详见 [Mapping 层 API · status_mapper_t](api/mapping.md#status_mapper_t)。

---

## 🗂️ 快速索引

| 🎯 决策点 | 📑 章节 |
|------|------|
| 通知模式（sync/async/deferred） | [1](#1-通知模式选择) |
| 元数据查询（cached vs direct） | [2](#2-错误码元数据查询路径选择) |
| 废弃/迁移（mark/migrate/recursive） | [3](#3-错误码废弃与迁移决策) |
| i18n locale 回退 | [4](#4-i18n-消息查询回退路径) |
| 序列化格式（string/json/binary） | [5](#5-序列化格式选择) |
| 插件开发模式 | [6](#6-插件开发模式选择) |
| 错误传递方式（result/exception/context） | [7](#7-错误传递方式选择) |
| HTTP/gRPC 状态映射 | [8](#8-httpgrpc-状态码映射选择) |
