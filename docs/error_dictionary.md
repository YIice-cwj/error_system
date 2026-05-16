# 📖 全局业务错误码字典
> 🕒 **最后更新时间**: 2026-05-17 01:05:33
> ⚠️ **注意**: 本文档由脚本自动生成，请勿手动修改！如需新增错误码，请在 `config/errors/` 目录下的 JSON 文件中维护。

---
## 📑 目录
- [Redis组件 (ID: 10)](#redis组件-subsys-10)
- [交易服务 (ID: 101)](#交易服务-subsys-101)
- [用户服务 (ID: 102)](#用户服务-subsys-102)
- [支付服务 (ID: 103)](#支付服务-subsys-103)

---

<h2 id="Redis组件-subsys-10">🚀 Redis组件 <span>(Subsys: 10 | Domain: middleware)</span></h2>

| 错误宏名称 (Code) | 级别 (Level) | 所属模块 | 业务描述 (Description) |
|---|:---:|---|---|
| `ERR_POOL_EXHAUSTED` | 💀 FATAL | 连接池模块 | Redis 连接池已耗尽，无法获取新连接 |
| `ERR_KEY_NOT_FOUND` | 🔵 INFO | 指令执行模块 | 查询的 Redis Key 不存在 |

[⬆️ 回到顶部](#-目录)

---

<h2 id="交易服务-subsys-101">🚀 交易服务 <span>(Subsys: 101 | Domain: application)</span></h2>

| 错误宏名称 (Code) | 级别 (Level) | 所属模块 | 业务描述 (Description) |
|---|:---:|---|---|
| `ERR_ORDER_NOT_FOUND` | 🔴 ERROR | 订单模块 | 订单不存在或已删除 |
| `ERR_CART_IS_EMPTY` | 🟠 WARN | 购物车模块 | 购物车为空，无法进行结算 |

[⬆️ 回到顶部](#-目录)

---

<h2 id="用户服务-subsys-102">🚀 用户服务 <span>(Subsys: 102 | Domain: application)</span></h2>

| 错误宏名称 (Code) | 级别 (Level) | 所属模块 | 业务描述 (Description) |
|---|:---:|---|---|
| `ERR_PASSWORD_MISMATCH` | 🟠 WARN | 鉴权模块 | 用户密码输入错误 |
| `ERR_TOKEN_EXPIRED` | 🔵 INFO | 鉴权模块 | 登录态 Token 已过期，需要重新登录 |
| `ERR_USER_BANNED` | 🔴 ERROR | 资料模块 | 用户账号已被封禁 |

[⬆️ 回到顶部](#-目录)

---

<h2 id="支付服务-subsys-103">🚀 支付服务 <span>(Subsys: 103 | Domain: third_party)</span></h2>

| 错误宏名称 (Code) | 级别 (Level) | 所属模块 | 业务描述 (Description) |
|---|:---:|---|---|
| `ERR_WX_PAY_TIMEOUT` | 🔴 ERROR | 支付网关 | 调用微信支付接口超时 |
| `ERR_INSUFFICIENT_BALANCE` | 🟠 WARN | 内部钱包 | 钱包余额不足，无法扣款 |
| `ERR_ACCOUNT_FROZEN` | 💀 FATAL | 内部钱包 | 支付账户存在风险，已被冻结 |

[⬆️ 回到顶部](#-目录)

---
