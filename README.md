# Error System (错误码系统)

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.15%2B-green.svg)](https://cmake.org)
[![GoogleTest](https://img.shields.io/badge/Tests-GoogleTest-yellow.svg)](https://github.com/google/googletest)

这是一个基于 C++17 标准开发的高性能、高可扩展的全局错误码管理系统。它通过位移操作将丰富的错误上下文信息封装在一个 64 位的整数中，提供了零开销的错误码构建与解析能力。

---

## 目录

- [核心特性](#核心特性-core-features)
- [错误码结构设计](#错误码结构设计-error-code-structure)
- [快速入门](#快速入门-quick-start)
- [架构概览](#架构概览-architecture)
- [工具库](#工具库-utils)
- [编译与安装](#编译与安装-build--install)
- [目录结构](#目录结构-directory-structure)
- [许可证](#许可证-license)

---

## 核心特性 (Core Features)

*   **现代 C++17**: 大量使用 `constexpr`，支持编译期错误码构建，提供极致性能。
*   **零开销位移封装**: 采用明确的位移和掩码操作实现 64 位错误码字段拆解，100% 避免严格别名规则与位域排布未定义行为。
*   **丰富的错误上下文**: 统一的 64 位错误码结构，包含了等级（Level）、系统域（Domain）、子系统（Subsystem）、模块（Module）和错误编号（Number），轻松实现问题的快速定位。
*   **结构化负载 (Payload)**: `error_context_t` 支持键值对形式的结构化负载，方便附加任意调试信息。
*   **自动堆栈跟踪**: 可配置的错误等级阈值，当错误等级达到阈值时自动捕获当前调用栈，加速问题定位。
*   **源位置追踪**: 自动记录错误发生的文件名、函数名和行号，支持完整路径或短文件名模式。
*   **插件系统 (Plugin)**: 提供可扩展的插件接口，支持日志、统计、告警等自定义错误处理能力的接入。
*   **代码生成工具**: 提供 Python 脚本从 JSON 配置自动生成错误码定义头文件。
*   **对象池优化**: `error_context_t` 的因果链包装使用线程局部对象池，减少高频场景下的堆分配开销。
*   **完备的测试**: 深度集成 GoogleTest，16 个测试文件覆盖所有核心模块，确保逻辑坚如磐石。
*   **异常封装**: 提供 `error_exception_t` 继承 `std::exception`，在需要异常机制的场景中无缝传递错误上下文。

---

## 错误码结构设计 (Error Code Structure)

本系统将错误码定义为一个 64 位无符号整数 (`uint64_t`)，其内部位域划分如下：

| 位区间 (Bits) | 长度 (Bits) | 字段名 (Field) | 描述 (Description) |
| :--- | :--- | :--- | :--- |
| `63` | 1 | `sign` | 符号位（0=成功，1=错误） |
| `60-62` | 3 | `reserved` | 预留位，供未来扩展使用 |
| `56-59` | 4 | `level` | 错误等级（如 debug, info, warn, error, fatal） |
| `48-55` | 8 | `system` | 系统域（区分 none, system, middleware, database, application, third_party 等） |
| `32-47` | 16 | `subsys` | 子系统编号（最大 65535） |
| `16-31` | 16 | `module` | 模块编号（最大 65535） |
| `0-15` | 16 | `number` | 具体错误编号（最大 65535） |

这种设计允许系统在一个返回值中携带完整的路由信息，极大地方便了微服务或大型单体架构中的错误追踪。

---

## 快速入门 (Quick Start)

### 1. 构建错误码

系统提供了 `error_builder_t` 作为工厂类来构建错误码：

```cpp
#include "error_system/core/error_builder.h"
#include "error_system/core/error_level.h"
#include "error_system/domain/system_domain.h"

using namespace error_system::core;
using namespace error_system::domain;

// 在编译期构建一个数据库模块的严重错误
constexpr auto db_err = error_builder_t::make_error_code(
    error_level_t::fatal,      // 错误等级
    system_domain_t::database, // 系统域
    100,                       // 子系统ID
    200,                       // 模块ID
    404                        // 错误编号
);
```

### 2. 解析错误码

你可以随时解析一个 `error_code_t` 来获取它的各个组成部分，过程是完全零开销的：

```cpp
error_code_t code = db_err;

if (code.get_sign() == 1) { // 判断是否为错误
    auto level  = code.get_level();  // 获取错误等级
    auto system = code.get_system(); // 获取系统域
    auto module = code.get_module(); // 获取模块编号
    auto number = code.get_number(); // 获取具体的错误编号

    // 错误码比较
    if (code == another_code) { /* ... */ }
    if (code < another_code)  { /* 用于排序 */ }
}
```

### 3. 使用错误上下文 (error_context_t)

`error_context_t` 封装了完整的错误信息，支持消息格式化、结构化负载、堆栈跟踪和因果链：

```cpp
#include "error_system/core/error_context.h"

using namespace error_system::core;

// 设置堆栈捕获阈值（WARN 及以上自动捕获堆栈）
config::error_config_t::set_stacktrace_level(error_level_t::warn);

// 创建错误上下文
auto code = error_builder_t::make_error_code(
    error_level_t::error, domain::system_domain_t::database, 0, 0, 1001);

error_context_t ctx(code, "数据库连接失败: {}", "timeout");

// 添加结构化负载
ctx.with("host", "192.168.1.100")
   .with("port", "3306")
   .with("database", "user_db");

// 获取 payload 只读引用
const auto& payload = ctx.get_payload();

// 获取 C 风格错误描述
const char* desc = ctx.what();

// 输出完整错误信息（含堆栈和负载）
std::cout << ctx.to_string() << "\n";

// 序列化为 JSON（便于网络传输或日志存储）
std::string json = ctx.to_json();
// {"code":...,"message":"数据库连接失败: timeout","payload":{"host":"192.168.1.100",...}}

// 序列化为二进制（高效紧凑）
std::string binary = ctx.to_binary();
```

### 4. 使用 result_t 进行错误传递

`result_t<T>` 提供类似 Rust Result 的类型安全错误处理，无需异常：

```cpp
#include "error_system/core/result.h"

using namespace error_system::core;

result_t<int> divide(int a, int b) {
    if (b == 0) {
        auto code = error_builder_t::make_error_code(
            error_level_t::error, domain::system_domain_t::application, 0, 0, 1);
        return error_context_t(code, "除数不能为零");
    }
    return a / b;
}

// 使用
auto result = divide(10, 0);
if (result.is_error()) {
    std::cerr << result.error().to_string() << "\n";
} else {
    std::cout << "结果: " << result.value() << "\n";
}
```

### 5. 使用异常传递错误 (error_exception_t)

在需要异常机制的场景中，使用 `error_exception_t` 传递错误上下文：

```cpp
#include "error_system/core/error_exception.h"

using namespace error_system::core;

void may_throw() {
    auto code = error_builder_t::make_error_code(
        error_level_t::error, domain::system_domain_t::application, 0, 0, 2);
    error_context_t ctx(code, "操作失败");
    throw error_exception_t(std::move(ctx));
}

try {
    may_throw();
} catch (const error_exception_t& e) {
    std::cerr << e.what() << "\n";      // 完整错误信息
    std::cerr << e.code().get_code() << "\n"; // 原始错误码
    std::cerr << e.context().to_json() << "\n"; // JSON 格式上下文
}
```

### 6. 使用 DEFINE_ERROR_CODE 宏定义错误码

使用宏可以方便地定义错误码并自动注册到错误码注册表：

```cpp
#include "error_system/core/error_registry.h"

// 定义错误码（自动注册）
DEFINE_ERROR_CODE(
    ERR_DB_CONNECTION_TIMEOUT,
    error_system::core::error_level_t::error,
    error_system::domain::system_domain_t::database,
    1, 1, 0x0001,
    "数据库连接超时");

// 使用
auto& registry = error_registry_t::instance();
if (registry.is_registered(ERR_DB_CONNECTION_TIMEOUT)) {
    auto info = registry.get_info(ERR_DB_CONNECTION_TIMEOUT);
    std::cout << info->get().description << "\n";
}
```

### 7. 使用代码生成工具

系统提供了 Python 脚本从 JSON 配置自动生成错误码定义：

**JSON 配置示例** (`config/errors/trade_service_errors.json`):

```json
{
    "namespace": "biz::trade_errors",
    "service_name": "trade",
    "domain": "application",
    "subsystem_id": 101,
    "modules": {
        "order": { "id": 1, "desc": "订单模块" },
        "payment": { "id": 2, "desc": "支付模块" }
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

**生成命令**:

```bash
python script/script_py/generate_error_codes.py \
    config/errors/trade_service_errors.json \
    include/generated/
```

生成的头文件可以直接在业务代码中使用：

```cpp
#include "generated/trade_service_errors.h"

using namespace biz::trade_errors;

// 使用自动生成的错误码
error_context_t ctx(ERR_ORDER_NOT_FOUND);
```

### 8. 注册插件

通过插件系统接入自定义的错误处理能力：

```cpp
#include "error_system/plugin/i_error_plugin.h"
#include "error_system/plugin/plugin_registry.h"

class log_plugin_t : public error_system::plugin::i_error_plugin_t {
public:
    std::string_view name() const noexcept override { return "logger"; }
    void on_error(const error_system::core::error_context_t& ctx) noexcept override {
        std::cerr << "[LOG] " << ctx.to_string() << "\n";
    }
};

// 注册插件
log_plugin_t logger;
plugin_registry_t::instance().register_plugin(&logger);
```

### 9. 使用错误路由插件 (error_router_plugin_t)

错误路由插件提供按错误码、模块组 ID 或系统域路由错误事件的能力：

```cpp
#include "error_system/plugin/error_router_plugin.h"

using namespace error_system::plugin;

// 按错误码注册特定处理函数
error_router_plugin_t::instance().register_handler_by_code(
    some_error_code,
    [](const core::error_context_t& ctx) {
        std::cerr << "特定错误: " << ctx.message << "\n";
    }
);

// 按系统域注册处理函数
error_router_plugin_t::instance().register_handler_by_domain(
    domain::system_domain_t::database,
    [](const core::error_context_t& ctx) {
        std::cerr << "数据库错误: " << ctx.to_string() << "\n";
    }
);

// 将路由插件注册到全局插件注册表
plugin_registry_t::instance().register_plugin(&error_router_plugin_t::instance());

// 之后创建的错误上下文会自动路由到对应的处理函数
error_context_t ctx{db_error_code, "连接超时"};  // 触发数据库域处理函数
```

---

## 架构概览 (Architecture)

```
┌─────────────────────────────────────────────────────────────┐
│                        Error System                          │
├─────────────────────────────────────────────────────────────┤
│  Config Layer                                                │
│  └── error_config_t  (全局错误配置：堆栈阈值、源位置等)      │
├─────────────────────────────────────────────────────────────┤
│  Core Layer                                                  │
│  ├── error_code_t    (64位错误码数据类)                      │
│  ├── error_builder_t (编译期错误码构建器)                    │
│  ├── error_level_t   (错误等级枚举)                          │
│  ├── error_context_t (错误上下文：码+消息+因果链+负载+堆栈)  │
│  ├── error_registry_t(错误码注册器)                          │
│  ├── result_t<T>     (类Rust Result，替代异常传递错误)       │
│  └── error_exception_t (异常封装)                            │
├─────────────────────────────────────────────────────────────┤
│  Domain Layer                                                │
│  └── system_domain_t (6大系统域定义)                         │
├─────────────────────────────────────────────────────────────┤
│  Plugin Layer                                                │
│  ├── i_error_plugin_t   (插件抽象接口)                       │
│  ├── plugin_registry_t  (插件单例注册表，负责广播)           │
│  └── error_router_plugin_t (错误路由插件，按码/域分发)       │
├─────────────────────────────────────────────────────────────┤
│  Memory Layer                                                │
│  └── object_pool_t<T>   (线程局部对象池，优化高频分配)       │
├─────────────────────────────────────────────────────────────┤
│  Utils Layer                                                 │
│  ├── string_utils_t     (字符串处理: hash, trim, format...) │
│  ├── json_utils_t       (JSON解析与字典)                     │
│  ├── file_utils         (文件读写操作)                       │
│  ├── stack_trace_utils_t(跨平台堆栈跟踪)                     │
│  └── source_location_t  (源文件位置追踪)                     │
├─────────────────────────────────────────────────────────────┤
│  Generated Layer                                             │
│  └── 从 JSON 配置自动生成的错误码定义                        │
└─────────────────────────────────────────────────────────────┘
```

### 支持的系统域

| 系统域 | 枚举值 | 说明 |
|--------|--------|------|
| `none` | 0x00 | 无分类 / 未知 |
| `system` | 0x01 | 系统与底层基础设施层 |
| `middleware` | 0x02 | 中间件层 |
| `database` | 0x03 | 数据库层 |
| `application` | 0x04 | 内部业务应用/微服务层 |
| `third_party` | 0x05 | 外部/第三方依赖层 |

---

## 工具库 (Utils)

### string_utils_t

提供编译期字符串哈希、格式化、修剪、分割、大小写转换等工具函数：

```cpp
// 编译期 FNV-1a 哈希
constexpr auto h = utils::string_utils_t::hash("hello");

// 字符串格式化
std::string msg = utils::string_utils_t::format("Error: {}", "connection failed");

// 修剪空白
std::string_view trimmed = utils::string_utils_t::trim("  hello  ");

// 分割字符串
auto parts = utils::string_utils_t::split("a,b,c", ",");
```

### json_utils_t

轻量级 JSON 解析器，支持嵌套键访问：

```cpp
auto dict = utils::json_dict_t::parse(R"({"user": {"name": "Alice"}})");
std::string name = dict->get_value_or("user.name", "Unknown").value();
```

### file_utils_t

跨平台文件读写工具：

```cpp
auto content = utils::file_utils_t::read_file("config.json");
utils::file_utils_t::write_file("output.txt", "Hello, World!");
```

### stack_trace_utils_t

跨平台堆栈跟踪工具，支持 Linux/macOS/Windows：

```cpp
// 抓取当前调用栈（跳过前1帧）
auto frames = utils::stack_trace_utils_t::generate(1, 64);
for (const auto& frame : frames) {
    std::cout << frame << "\n";
}
```

---

## 编译与安装 (Build & Install)

本项目使用 CMake 作为构建系统，并自动处理测试依赖（GoogleTest）。

### 环境要求
*   支持 C++17 的现代编译器 (GCC 7+, Clang 5+, MSVC 19.14+)
*   CMake 3.15 或更高版本

### 编译步骤

```bash
# 1. 克隆项目
git clone https://github.com/YIice-cwj/error_system.git
cd error_system

# 2. 生成构建文件
mkdir build && cd build
cmake ..

# 3. 编译
cmake --build . -j$(nproc)

# 4. 运行单元测试
ctest --output-on-failure
```

### 测试覆盖

| 模块 | 测试文件数 | 测试用例数 |
|------|-----------|-----------|
| Core 层 | 7 | 59+ |
| Plugin 层 | 2 | 17 |
| Memory 层 | 1 | 10 |
| Utils 层 | 5 | 37+ |
| Config 层 | 1 | 7 |
| Translator 层 | 1 | 9 |
| **总计** | **17** | **149+** |

> 注：测试发现超时已调整为 30 秒（`DISCOVERY_TIMEOUT 30`），确保在复杂环境下测试稳定运行。

### 安装到系统

```bash
cd build
sudo cmake --install .
```

安装内容包括：
- 头文件 → `${CMAKE_INSTALL_PREFIX}/include/error_system/`
- 库文件 → `${CMAKE_INSTALL_PREFIX}/lib/`
- CMake 配置 → `${CMAKE_INSTALL_PREFIX}/lib/cmake/error_system/`
- 代码生成脚本 → `${CMAKE_INSTALL_PREFIX}/share/error_system/scripts/`

### 作为子项目使用

#### 方式一：使用 find_package

```cmake
find_package(error_system 1.0.0 REQUIRED)

add_executable(my_business_app main.cpp)
target_link_libraries(my_business_app PRIVATE error_system::error_system)
```

#### 方式二：使用 CMake 宏自动生成错误码（推荐）

```cmake
find_package(error_system 1.0.0 REQUIRED)

add_executable(my_business_app main.cpp)
target_link_libraries(my_business_app PRIVATE error_system::error_system)

# 自动生成错误码头文件
error_system_generate_codes(
    TARGET my_business_app
    JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config/errors
)
```

`error_system_generate_codes` 宏会自动：
1. 从 JSON 配置文件生成 C++ 错误码头文件
2. 生成 O(1) 极速错误字典
3. 生成错误码文档
4. 自动配置头文件搜索路径

### 卸载

```bash
cd build
sudo cmake --build . --target uninstall
```

---

## 目录结构 (Directory Structure)

```text
error_system/
├── CMakeLists.txt              # CMake 配置
├── README.md                   # 项目说明
├── config/                     # 错误码配置文件
│   └── errors/                 # JSON 格式的错误码定义
│       ├── payment_service_errors.json
│       ├── redis_component_errors.json
│       ├── trade_service_errors.json
│       └── user_service_errors.json
├── include/error_system/       # 对外公开的头文件
│   ├── config/                 # 配置层
│   │   └── error_config.h      # 全局错误配置
│   ├── core/                   # 核心定义
│   │   ├── error_builder.h     # 编译期错误码构建器
│   │   ├── error_code.h        # 64位错误码数据类
│   │   ├── error_context.h     # 错误上下文
│   │   ├── error_exception.h   # 异常封装
│   │   ├── error_level.h       # 错误等级枚举
│   │   ├── error_registry.h    # 错误码注册器
│   │   └── result.h            # 类Rust Result
│   ├── domain/                 # 系统域定义
│   │   └── system_domain.h     # 6大系统域
│   ├── memory/                 # 内存管理
│   │   └── object_pool.h       # 线程局部对象池
│   ├── plugin/                 # 插件系统接口
│   │   ├── error_router_plugin.h
│   │   ├── i_error_plugin.h
│   │   └── plugin_registry.h
│   ├── translator/             # 翻译器
│   │   └── error_translator.h
│  ├── utils/                  # 辅助工具
│  │   ├── error_formatter.h
│  │   ├── file_utils.h         # 文件操作 (file_utils_t)
│  │   ├── json_lexer.h
│  │   ├── json_utils.h
│  │   ├── source_location.h
│  │   ├── stack_trace_utils.h
│  │   └── string_utils.h
├── include/generated/          # 自动生成的错误码定义
├── src/                        # 核心实现代码
│   ├── core/                   # error_context, error_registry 实现
│   ├── plugin/                 # plugin_registry, error_router_plugin 实现
│   ├── translator/             # error_translator 实现
│   └── utils/                  # 工具函数实现
├── tests/                      # GoogleTest 单元测试
│   ├── config/                 # 配置层测试
│   ├── core/                   # 核心层测试
│   ├── domain/                 # 系统域测试
│   ├── memory/                 # 内存层测试
│   ├── plugin/                 # 插件层测试
│   ├── translator/             # 翻译器测试
│   └── utils/                  # 工具库测试
├── script/                     # 代码生成脚本
│   ├── generate_errors.sh      # 批量生成错误码脚本
│   └── script_py/
│       ├── generate_error_codes.py
│       ├── generate_error_dict.py
│       └── generate_error_docs.py
├── examples/                   # 示例代码
│   ├── demo01.cc               # 基础用法：错误码、上下文、序列化
│   ├── demo02.cc               # result_t 错误处理和链式操作
│   ├── demo03.cc               # 插件系统：日志、统计、路由
│   ├── demo04.cc               # 异常处理：error_exception_t
│   └── demo05.cc               # 更多示例
├── docs/                       # 文档
│   ├── api/                    # API 文档
│   ├── architecture.md         # 架构设计
│   ├── error_dictionary.md     # 错误码字典
│   └── README.md               # 文档索引
└── LICENSE                     # 许可证
```

---

## 许可证 (License)

本项目遵循相关开源许可证（详见项目根目录下的 [LICENSE](LICENSE) 文件）。
