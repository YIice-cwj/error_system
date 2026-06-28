# 🔁 Migration 层 API

> `error_system::migration` ｜ 头文件：`error_system/migration/error_migration.h`

错误码废弃与迁移模块 — 管理错误码的废弃状态与旧码→新码迁移映射，支撑版本演进下的平滑过渡。

---

## 📋 deprecation_info_t

废弃信息描述结构。

```cpp
struct deprecation_info_t {
    bool deprecated{false};                       // 是否已废弃
    std::string reason;                            // 废弃原因
    std::optional<error_code_t> replacement{};     // 替代错误码（可选）
    std::string since_version;                     // 起始废弃版本
    std::string removal_version;                   // 计划移除版本
};
```

---

## 🔄 error_migration_registry_t

错误码废弃与迁移注册器单例。

> 💡 废弃状态与迁移映射**分离存储**：废弃不一定有替代码，迁移也不一定意味着源码已废弃（可能是别名）。

### API

```cpp
class error_migration_registry_t {
public:
    static error_migration_registry_t& instance() noexcept;

    // 标记废弃（若提供 replacement，同时建立 migration 映射）
    void mark_deprecated(error_code_t code, std::string_view reason,
                         std::optional<error_code_t> replacement = std::nullopt,
                         std::string_view since_version = "",
                         std::string_view removal_version = "") noexcept;

    // 仅建立迁移映射（不标记废弃，适用于别名场景）
    void register_migration(error_code_t old_code, error_code_t new_code) noexcept;

    // 查询
    [[nodiscard]] std::optional<deprecation_info_t> get_deprecation_info(error_code_t code) const noexcept;
    [[nodiscard]] bool is_deprecated(error_code_t code) const noexcept;

    // 迁移（单跳，不递归）
    [[nodiscard]] std::optional<error_code_t> migrate(error_code_t old_code) const noexcept;
    // 递归迁移（沿链跳转到终点，最大深度 16，环检测后返回当前码）
    [[nodiscard]] error_code_t migrate_recursive(error_code_t old_code) const noexcept;

    // 移除
    bool unmark_deprecated(error_code_t code) noexcept;       // 不清除迁移映射
    bool unregister_migration(error_code_t old_code) noexcept;
    void clear_all() noexcept;

    // 统计
    [[nodiscard]] size_t deprecated_count() const noexcept;
    [[nodiscard]] size_t migration_count() const noexcept;
    [[nodiscard]] std::vector<code_t> get_deprecated_codes() const noexcept;
};
```

### 单跳 vs 递归迁移

| 方法 | 行为 | 适用 |
|------|------|------|
| `migrate()` | 仅一次映射跳转（a → b），即使 b 也有映射也停止 | 单步版本升级 |
| `migrate_recursive()` | 沿链跳转到终点（a → b → c → … → 终点），最大深度 16，环检测安全 | 追溯最终替代码 |

**使用示例**

```cpp
auto& reg = error_migration_registry_t::instance();

// 场景 1：版本演进，下线旧码（同时建立迁移）
reg.mark_deprecated(ERR_OLD_DB_POOL,
                    "v2.0 起改用 ERR_DB_POOL_V2",
                    ERR_DB_POOL_V2, "2.0.0", "3.0.0");

// 场景 2：仅标记废弃，无替代
reg.mark_deprecated(ERR_LEGACY_AUTH, "鉴权流程已重构，下版本移除");

// 场景 3：别名映射（不标记废弃）
reg.register_migration(ERR_USER_V1, ERR_USER_V2);

if (auto info = reg.get_deprecation_info(ERR_OLD_DB_POOL); info && info->deprecated) {
    log_warn("错误码已废弃：{}", info->reason);
}

auto migrated = reg.migrate(ERR_OLD_DB_POOL);           // 单跳到 ERR_DB_POOL_V2
auto terminal = reg.migrate_recursive(ERR_OLD_DB_POOL); // 递归到终点
```

> 📝 `unmark_deprecated()` 不会清除迁移映射，便于先停止废弃警告再逐步下线。

> 🔗 废弃/迁移决策树详见 [决策树 · 3](../decision_tree.md#3-错误码废弃与迁移决策)。
