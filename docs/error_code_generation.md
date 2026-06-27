# 错误码自动生成指南

> 从 JSON 配置一键生成 C++ 头文件、全局字典与 Markdown 文档。

---

## 目录

- [设计理念](#设计理念)
- [目录结构](#目录结构)
- [JSON 配置格式](#json-配置格式)
- [生成产物](#生成产物)
- [使用方式](#使用方式)
- [CMake 集成](#cmake-集成)
- [ID 冲突检测](#id-冲突检测)
- [完整示例](#完整示例)
- [常见问题](#常见问题)

---

## 设计理念

错误码的传统维护方式存在以下痛点：

| 痛点 | 传统方式 | 自动生成方案 |
|------|------|------|
| 错误码散落各处 | 头文件、文档、运维表分别维护 | JSON 单一数据源 |
| 编号冲突 | 人工避免，难以发现 | 编译期冲突检测，CI 失败 |
| 文档与代码不同步 | 文档靠人工维护 | 自动生成 Markdown 字典 |
| 跨团队协作 | 需口头/文档沟通 | PR 评审 JSON 改动即可 |

核心思路：**JSON 配置是唯一数据源**，C++ 头文件、字典注册表、Markdown 文档均由脚本自动生成。

---

## 目录结构

```
error_system/
├── config/
│   └── errors/                          # JSON 错误码配置（数据源）
│       ├── payment_service_errors.json
│       ├── redis_component_errors.json
│       ├── trade_service_errors.json
│       └── user_service_errors.json
├── script/
│   ├── generate_errors.sh               # Shell 入口（调用三个 Python 脚本）
│   └── script_py/
│       ├── generate_all.py              # Python 统一入口
│       ├── generate_error_codes.py      # JSON → C++ 头文件
│       ├── generate_error_dict.py       # 汇总 → error_dict.h + ID 冲突检测
│       └── generate_error_docs.py       # 汇总 → Markdown 字典文档
└── build/generated_errors/              # 生成产物（不入版本控制）
    ├── include/
    │   ├── trade_service_errors.h       # 业务错误码定义
    │   ├── payment_service_errors.h
    │   ├── redis_component_errors.h
    │   ├── user_service_errors.h
    │   └── error_dict.h                 # 子系统/模块名称注册表
    └── error_dictionary.md              # Markdown 错误码字典
```

---

## JSON 配置格式

每个服务对应一个 JSON 文件，文件名即为生成的 C++ 头文件名（例如 `trade_service_errors.json` → `trade_service_errors.h`）。

### 字段说明

```json
{
    "namespace": "biz::trade_errors",
    "service_name": "交易服务",
    "domain": "application",
    "subsystem_id": 101,
    "modules": {
        "order": { "id": 1, "desc": "订单模块" },
        "cart":  { "id": 2, "desc": "购物车模块" }
    },
    "errors": [
        {
            "name": "ERR_ORDER_NOT_FOUND",
            "module": "order",
            "level": "error",
            "number": 1,
            "desc": "订单不存在或已删除"
        }
    ]
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|:---:|------|
| `namespace` | string | 是 | C++ 命名空间（如 `biz::trade_errors`） |
| `service_name` | string | 是 | 服务中文名称（用于文档和错误输出） |
| `domain` | string | 是 | 系统域，取值见下表 |
| `subsystem_id` | int | 是 | 子系统 ID（1-65535） |
| `modules` | object | 是 | 模块映射表 |
| `modules.<key>.id` | int | 是 | 模块 ID（1-65535） |
| `modules.<key>.desc` | string | 是 | 模块中文名称 |
| `errors` | array | 是 | 错误码列表 |
| `errors[].name` | string | 是 | 错误码宏名（必须以 `ERR_` 开头） |
| `errors[].module` | string | 是 | 模块 key（必须存在于 `modules` 中） |
| `errors[].level` | string | 是 | 错误级别（debug/info/warn/error/fatal） |
| `errors[].number` | int | 是 | 错误编号（1-65535，模块内唯一） |
| `errors[].desc` | string | 是 | 错误中文描述 |

### 系统域取值（`domain` 字段）

| 取值 | 含义 |
|------|------|
| `none` | 未分类 |
| `system` | 系统级 |
| `middleware` | 中间件 |
| `database` | 数据库 |
| `application` | 应用层 |
| `third_party` | 第三方依赖 |

### 错误级别取值（`level` 字段）

| 取值 | 含义 | 严重程度 |
|------|------|:---:|
| `debug` | 调试信息 | 最低 |
| `info` | 一般提示 | ↑ |
| `warn` | 警告 | ↑ |
| `error` | 错误 | ↑ |
| `fatal` | 致命错误 | 最高 |

---

## 生成产物

### 1. 业务错误码头文件（`<service>_errors.h`）

每个 JSON 配置文件生成一个对应的 C++ 头文件，使用 `DEFINE_ERROR_CODE` 宏定义错误码常量并自动注册到全局注册表。

```cpp
// 自动生成的 trade_service_errors.h
#pragma once
#include "error_system/core/error_registry.h"
#include "error_system/domain/system_domain.h"

namespace biz::trade_errors {

    // 模块: 订单模块
    DEFINE_ERROR_CODE(
        ERR_ORDER_NOT_FOUND,
        error_system::core::error_level_t::error,
        error_system::domain::system_domain_t::application,
        101,  // subsystem: 交易服务
        1,    // module: order
        1,
        "订单不存在或已删除",
        "交易服务",  // 子系统名称
        "订单模块"    // 模块名称
    );

} // namespace biz::trade_errors
```

**`DEFINE_ERROR_CODE` 宏做了什么？**

```cpp
#define DEFINE_ERROR_CODE(NAME, LEVEL, SYSTEM, SUBSYS, MODULE, NUMBER, DESC, SUBSYS_NAME, MODULE_NAME)
    // 1. 定义 constexpr 错误码常量（编译期构建，进入只读数据段）
    constexpr ::error_system::core::error_code_t NAME(LEVEL, SYSTEM, SUBSYS, MODULE, NUMBER);
    // 2. 静态注册器：在 main() 之前自动注册到 error_registry_t
    inline const ::error_system::core::error_registrar_t NAME##_registrar_(NAME, #NAME, DESC, SUBSYS_NAME, MODULE_NAME);
```

> 注册利用 C++ 静态初始化在 `main()` 之前完成，无需手动调用注册函数。

### 2. 全局字典（`error_dict.h`）

汇总所有 JSON 配置，生成子系统/模块名称注册表。当 `error_context_serializer_t::to_string()` 输出错误信息时，会通过此注册表将 ID 转换为可读名称。

```cpp
// 自动生成的 error_dict.h
namespace error_system::core {
    struct subsystem_module_registrar_t {
        subsystem_module_registrar_t() {
            auto& registry = error_registry_t::instance();
            registry.register_subsystem_module(10, 1, "Redis组件", "连接池模块");
            registry.register_subsystem_module(10, 2, "Redis组件", "指令执行模块");
            registry.register_subsystem_module(101, 1, "交易服务", "订单模块");
            // ...
        }
    };
    inline subsystem_module_registrar_t global_subsystem_module_registrar_{};
}
```

### 3. Markdown 字典文档（`error_dictionary.md`）

生成人类可读的错误码字典，适合作为研发协作文档。每个服务对应一张表格，包含错误宏名、级别、所属模块和业务描述。

---

## 使用方式

### 方式一：CMake 自动构建（推荐）

CMake 配置阶段自动扫描 `config/errors/*.json`，构建时自动运行生成脚本，无需手动操作。

```bash
cmake -S . -B build
cmake --build build
# 生成产物位于 build/generated_errors/
```

**生成时机**：CMake 的 `add_custom_command` 配置了 `DEPENDS`，当 JSON 文件或 Python 脚本变更时，下次构建会自动重新生成。

### 方式二：Shell 脚本

```bash
./script/generate_errors.sh
```

输出到 `build/generated_errors/`。

### 方式三：Python 入口

```bash
# 默认输出到 build/generated_errors/
python script/script_py/generate_all.py

# 指定输出目录
python script/script_py/generate_all.py build-release
python script/script_py/generate_all.py /absolute/path/to/output
```

### 方式四：单独调用某个脚本

```bash
# 仅生成某个服务的头文件
python script/script_py/generate_error_codes.py \
    config/errors/trade_service_errors.json \
    build/generated_errors/include

# 仅生成全局字典（含 ID 冲突检测）
python script/script_py/generate_error_dict.py \
    config/errors \
    build/generated_errors/include/error_dict.h

# 仅生成 Markdown 文档
python script/script_py/generate_error_docs.py \
    config/errors \
    build/generated_errors/error_dictionary.md
```

---

## CMake 集成

### 在 error_system 项目内

构建系统已自动集成，无需额外配置。关键代码位于根 `CMakeLists.txt`：

```cmake
if(Python3_FOUND)
    file(GLOB ERROR_JSON_FILES CONFIGURE_DEPENDS "${ERROR_JSON_DIR}/*.json")
    foreach(JSON_FILE ${ERROR_JSON_FILES})
        # 为每个 JSON 文件生成对应的 .h
        add_custom_command(
            OUTPUT ${GENERATED_HEADER}
            COMMAND Python3::Interpreter ${generate_error_codes.py} ${JSON_FILE} ${ERROR_GENERATED_DIR}
            DEPENDS ${JSON_FILE} ${generate_error_codes.py}
        )
    endforeach()

    # 生成全局字典
    add_custom_command(OUTPUT ${DICT_HEADER} ...)

    # 生成 Markdown 文档
    add_custom_command(OUTPUT ${DOCS_MD_OUT} ...)

    # 汇总目标，主库依赖此目标
    add_custom_target(GenerateErrorAssets DEPENDS ...)
    add_dependencies(${PROJECT_NAME} GenerateErrorAssets)
endif()
```

### 在你的项目中使用 error_system 的代码生成

如果你的项目引入 error_system 作为依赖，可以为你的项目生成专属错误码：

```cmake
# 通过 FetchContent 或 add_subdirectory 引入 error_system 后
error_system_generate_codes(
    TARGET my_app                           # 你的目标名
    JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config/errors  # 你的 JSON 配置目录
)
```

生成的头文件会自动添加到 `my_app` 的 include 路径中。

---

## ID 冲突检测

`generate_error_dict.py` 在生成全局字典前会扫描所有 JSON 文件，检测 `(subsystem_id, module_id, number)` 三元组是否全局唯一。

**冲突示例**：

假设 `trade_service_errors.json` 和 `payment_service_errors.json` 都定义了 `(103, 2, 1)`：

```
======================================================================
  [错误] 检测到错误码 ID 冲突！
  (subsystem_id, module_id, number) 三元组必须在所有配置中唯一
======================================================================

  冲突: subsystem_id=103, module_id=2, number=0x0001
    - file='payment_service_errors.json', service='支付服务',
      module='内部钱包' (id=2),
      error='ERR_INSUFFICIENT_BALANCE' (number=0x0001)
    - file='xxx_errors.json', service='...',
      module='...' (id=2),
      error='ERR_XXX' (number=0x0001)

  请修改冲突的错误码 number，确保全局唯一
======================================================================
```

**冲突时构建会失败**，避免错误码路由错误进入生产环境。

---

## 完整示例

### 步骤 1：编写 JSON 配置

新增文件 `config/errors/inventory_service_errors.json`：

```json
{
    "namespace": "biz::inventory_errors",
    "service_name": "库存服务",
    "domain": "application",
    "subsystem_id": 201,
    "modules": {
        "stock": { "id": 1, "desc": "库存管理" },
        "warehouse": { "id": 2, "desc": "仓库管理" }
    },
    "errors": [
        {
            "name": "ERR_STOCK_INSUFFICIENT",
            "module": "stock",
            "level": "warn",
            "number": 1,
            "desc": "库存不足，无法满足订单需求"
        },
        {
            "name": "ERR_STOCK_NOT_FOUND",
            "module": "stock",
            "level": "error",
            "number": 2,
            "desc": "商品库存记录不存在"
        },
        {
            "name": "ERR_WAREHOUSE_OFFLINE",
            "module": "warehouse",
            "level": "fatal",
            "number": 1,
            "desc": "仓库系统离线，无法发货"
        }
    ]
}
```

### 步骤 2：构建

```bash
cmake --build build
# CMake 检测到新 JSON 文件，自动运行生成脚本
```

### 步骤 3：在代码中使用

```cpp
#include "error_system.h"
#include "inventory_service_errors.h"  // 自动生成的头文件

using namespace error_system::core;
using namespace error_system::domain;

result_t<void> deduct_stock(int product_id, int qty) {
    if (qty > get_available_stock(product_id)) {
        return result_t<void>::make_error(
            biz::inventory_errors::ERR_STOCK_INSUFFICIENT,
            "商品 {} 库存不足，请求扣减 {} 件", product_id, qty);
    }
    return result_t<void>::make_success();
}

// 在 main() 之前，ERR_STOCK_INSUFFICIENT 已自动注册到 error_registry_t
// error_context_serializer_t::to_string() 会输出 "库存服务 / 库存管理" 而非原始 ID
```

### 步骤 4：查看生成的字典文档

打开 `build/generated_errors/error_dictionary.md`，新增的库存服务条目会自动出现在目录中。

---

## 常见问题

### Q1：生成的头文件应该提交到版本控制吗？

**不应该**。生成产物位于 `build/generated_errors/`，由 CMake 在构建时自动生成。`.gitignore` 已排除此目录。

JSON 配置文件（`config/errors/*.json`）才是数据源，必须提交。

### Q2：Python 没有安装怎么办？

CMake 会检测 Python3，缺失时跳过代码生成并输出提示：

```
Python3 未找到，跳过错误码/字典/文档代码生成
```

此时会生成一个空的 `error_dict.h` 占位，编译不报错，但无法使用业务错误码。建议安装 Python 3.6+。

### Q3：如何修改已生成的错误码描述？

**只修改 JSON 配置**，下次构建会自动重新生成头文件。不要手动修改 `build/generated_errors/` 下的任何文件 —— 文件头部有 `请勿手动修改` 标记。

### Q4：删除 JSON 文件后，对应的头文件会自动删除吗？

不会自动删除。但 CMake 的 `add_custom_command` 配置了 `DEPENDS`，如果 JSON 文件被删除，下次构建时该头文件不会再被重新生成。建议手动删除 `build/generated_errors/include/<service>_errors.h` 后重新构建。

### Q5：如何为多个微服务统一管理错误码？

将所有服务的 JSON 配置放在同一个 `config/errors/` 目录下，每个服务一个文件。生成脚本会自动汇总所有文件，生成统一的全局字典和文档，并检测跨服务的 ID 冲突。

### Q6：错误码 `number` 字段可以重复吗？

可以 —— **只要 `(subsystem_id, module_id, number)` 三元组不同即可**。例如：

- 交易服务（101）订单模块（1）的 number=1
- 支付服务（103）内部钱包（2）的 number=1

这两个错误码是不同的，不冲突。

### Q7：能否在 JSON 中使用中文？

可以。所有 JSON 文件使用 UTF-8 编码，中文字段（`service_name`、`modules[].desc`、`errors[].desc`）会原样输出到生成的头文件和文档中。
