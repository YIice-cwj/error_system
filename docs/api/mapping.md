# 🔗 Mapping 层 API

> `error_system::mapping` ｜ 头文件目录：`error_system/mapping/`

错误码映射模块 — 将内部 `error_code_t` 翻译为标准 HTTP / gRPC 状态码，便于在 Web/RPC 边界统一对外暴露错误。所有方法均为 `constexpr`，可在编译期求值，零运行时开销。

---

## 🌐 http_status_t

> 头文件：`error_system/mapping/http_status.h`

HTTP 状态码包装类，仅包含错误系统映射所需的子集（非完整 HTTP 标准覆盖）。枚举值与 HTTP 标准（RFC 7231）一致。

### 枚举值

```cpp
class http_status_t {
public:
    enum class value_t : uint16_t {
        ok = 200,
        bad_request = 400, unauthorized = 401, forbidden = 403, not_found = 404,
        method_not_allowed = 405, request_timeout = 408, conflict = 409, gone = 410,
        payload_too_large = 413, uri_too_long = 414, too_many_requests = 429,
        internal_server_error = 500, not_implemented = 501, bad_gateway = 502,
        service_unavailable = 503, gateway_timeout = 504,
    };
};
```

### API

```cpp
class http_status_t {
public:
    constexpr http_status_t() noexcept = default;                 // 默认 ok
    constexpr explicit http_status_t(value_t value) noexcept;
    constexpr explicit http_status_t(uint16_t code) noexcept;     // 非法值回退 internal_server_error

    [[nodiscard]] constexpr value_t value() const noexcept;
    [[nodiscard]] constexpr uint16_t to_int() const noexcept;
    [[nodiscard]] const char* c_str() const noexcept;             // 如 "Service Unavailable"

    [[nodiscard]] static constexpr http_status_t from_int(int value) noexcept;  // 未知返回 internal_server_error
    [[nodiscard]] static constexpr bool is_valid(int value) noexcept;

    [[nodiscard]] constexpr bool is_success() const noexcept;       // 2xx
    [[nodiscard]] constexpr bool is_client_error() const noexcept;  // 4xx
    [[nodiscard]] constexpr bool is_server_error() const noexcept;  // 5xx

    constexpr bool operator==(http_status_t other) const noexcept;
    constexpr bool operator!=(http_status_t other) const noexcept;
};
```

**使用示例**

```cpp
http_status_t s{http_status_t::service_unavailable};
s.to_int();       // → 503
s.c_str();        // → "Service Unavailable"
http_status_t::from_int(503);  // → http_status_t::service_unavailable
s.is_server_error();           // → true
```

---

## 🛰️ grpc_status_t

> 头文件：`error_system/mapping/grpc_status.h`

gRPC 状态码包装类，数值与 `grpc::StatusCode` 一致，避免引入 gRPC 依赖。

### 枚举值

```cpp
class grpc_status_t {
public:
    enum class value_t : uint16_t {
        ok = 0, cancelled = 1, unknown = 2, invalid_argument = 3, deadline_exceeded = 4,
        not_found = 5, already_exists = 6, permission_denied = 7, resource_exhausted = 8,
        failed_precondition = 9, aborted = 10, out_of_range = 11, unimplemented = 12,
        internal = 13, unavailable = 14, data_loss = 15, unauthenticated = 16,
    };
};
```

### API

```cpp
class grpc_status_t {
public:
    constexpr grpc_status_t() noexcept = default;              // 默认 ok
    constexpr explicit grpc_status_t(value_t value) noexcept;

    [[nodiscard]] constexpr value_t value() const noexcept;
    [[nodiscard]] constexpr uint16_t to_int() const noexcept;
    [[nodiscard]] const char* c_str() const noexcept;           // 如 "INTERNAL"

    [[nodiscard]] static constexpr grpc_status_t from_int(int value) noexcept;  // 越界返回 unknown
    [[nodiscard]] static constexpr bool is_valid(int value) noexcept;           // 0..16

    constexpr bool operator==(grpc_status_t other) const noexcept;
    constexpr bool operator!=(grpc_status_t other) const noexcept;
};
```

**使用示例**

```cpp
grpc_status_t s{grpc_status_t::internal};
s.c_str();                  // → "INTERNAL"
grpc_status_t::from_int(13); // → grpc_status_t::internal
grpc_status_t::is_valid(13); // → true
```

---

## 🔗 status_mapper_t

> 头文件：`error_system/mapping/status_mapper.h`

错误码到 HTTP / gRPC 状态码的映射器 — 纯函数工具类，不可实例化。

> **注意**：构造/析构/拷贝/移动全部 `= delete`，禁止实例化。

### 映射策略

`retryable` / `transient` 优先映射为 `503 / UNAVAILABLE`，提示客户端重试；其余按错误等级与系统域细分。database 在 gRPC 侧映射为 `DATA_LOSS`（数据层不可用语义更精确），其余 `UNAVAILABLE`。

| 错误等级 | 系统域 | HTTP | gRPC |
|------|------|:---:|:---:|
| 成功码 / debug·info·warn | — | 200 OK | OK |
| error | application | 400 Bad Request | INVALID_ARGUMENT |
| error | third_party | 502 Bad Gateway | UNAVAILABLE |
| error | database / middleware | 503 Service Unavailable | UNAVAILABLE / DATA_LOSS |
| error | system / none | 500 Internal Server Error | INTERNAL |
| fatal | — | 500 Internal Server Error | INTERNAL |
| 任意（retryable/transient） | — | 503 Service Unavailable | UNAVAILABLE |

### API

```cpp
class status_mapper_t {
public:
    status_mapper_t() = delete;
    ~status_mapper_t() = delete;
    status_mapper_t(const status_mapper_t&) = delete;
    status_mapper_t& operator=(const status_mapper_t&) = delete;
    status_mapper_t(status_mapper_t&&) = delete;
    status_mapper_t& operator=(status_mapper_t&&) = delete;

    [[nodiscard]] static constexpr http_status_t to_http_status(error_code_t code) noexcept;
    [[nodiscard]] static constexpr grpc_status_t to_grpc_status(error_code_t code) noexcept;
};
```

**使用示例**

```cpp
error_code_t code{error_level_t::error, system_domain_t::database, 1, 2, 0x0010};
status_mapper_t::to_http_status(code);  // → 503 Service Unavailable
status_mapper_t::to_grpc_status(code);  // → DATA_LOSS
code.set_retryable(true);
status_mapper_t::to_http_status(code);  // → 503（retryable 优先）
```

映射决策树详见 [决策树 · 8](../decision_tree.md#8-httpgrpc-状态码映射选择)。
