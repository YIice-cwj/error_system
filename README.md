# Error System (错误码系统)

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.15%2B-green.svg)](https://cmake.org)
[![GoogleTest](https://img.shields.io/badge/Tests-GoogleTest-yellow.svg)](https://github.com/google/googletest)

这是一个基于 C++17 标准开发的高性能、高可扩展的全局错误码管理系统。它通过位域（Bitfields）将丰富的错误上下文信息封装在一个 64 位的整数中，提供了零开销的错误码构建与解析能力，并内置了基于 JSON 的多语言（i18n）翻译支持。

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
*   **零开销位域封装**: 采用 `union` 与位域结合的设计，直接在寄存器层面完成 64 位整数与各错误字段的转换。
*   **丰富的错误上下文**: 统一的 64 位错误码结构，包含了等级（Level）、系统域（Domain）、子系统（Subsystem）、模块（Module）和错误编号（Number），轻松实现问题的快速定位。
*   **结构化负载 (Payload)**: `error_context_t` 支持键值对形式的结构化负载，方便附加任意调试信息。
*   **自动堆栈跟踪**: 可配置的错误等级阈值，当错误等级达到阈值时自动捕获当前调用栈，加速问题定位。
*   **源位置追踪**: 自动记录错误发生的文件名、函数名和行号，支持完整路径或短文件名模式。
*   **多语言支持 (i18n)**: 内置 `json_translator`，支持动态加载 JSON 字典，将冷冰冰的错误码转换为对用户友好的多语言文本。
*   **插件系统 (Plugin)**: 提供可扩展的插件接口，支持日志、统计、告警等自定义错误处理能力的接入。
*   **模块化预定义**: 已经内置了 18 大系统域（Kernel、Network、Database、Middleware、AI、Cloud 等），每个域下包含丰富的子系统和模块枚举，开箱即用。
*   **Traits 类型萃取**: 为所有子系统和模块提供统一的 `traits` 接口，支持编译期枚举与字符串的双向转换。
*   **对象池优化**: `error_context_t` 的因果链包装使用线程局部对象池，减少高频场景下的堆分配开销。
*   **完备的测试**: 深度集成 GoogleTest，200+ 单元测试确保核心逻辑坚如磐石。
*   **异常封装**: 提供 `error_exception_t` 继承 `std::exception`，在需要异常机制的场景中无缝传递错误上下文。

---

## 错误码结构设计 (Error Code Structure)

本系统将错误码定义为一个 64 位无符号整数 (`uint64_t`)，其内部位域划分如下：

| 位区间 (Bits) | 长度 (Bits) | 字段名 (Field) | 描述 (Description) |
| :--- | :--- | :--- | :--- |
| `63` | 1 | `sign` | 符号位（0=成功，1=错误） |
| `60-62` | 3 | `reserved` | 预留位，供未来扩展使用 |
| `56-59` | 4 | `level` | 错误等级（如 DEBUG, INFO, WARN, ERROR, FATAL） |
| `48-55` | 8 | `system` | 系统域（区分 Database, Network, App 等） |
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
}
```

### 3. 使用错误上下文 (error_context_t)

`error_context_t` 封装了完整的错误信息，支持消息格式化、结构化负载、堆栈跟踪和因果链：

```cpp
#include "error_system/core/error_context.h"

using namespace error_system::core;

// 设置堆栈捕获阈值（WARN 及以上自动捕获堆栈）
error_config::set_stacktrace_level(error_level_t::warn);

// 创建错误上下文
auto code = error_builder_t::make_error_code(
    error_level_t::error, domain::system_domain_t::database, 0, 0, 1001);

error_context_t ctx(code, "数据库连接失败: {}", "timeout");

// 添加结构化负载
ctx.with("host", "192.168.1.100")
   .with("port", "3306")
   .with("database", "user_db");

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

### 6. 使用 Traits 进行枚举转换

系统为所有子系统和模块提供了类型萃取，支持编译期枚举与字符串的双向转换：

```cpp
#include "error_system/traits/subsystem/database_subsystem_traits.h"
#include "error_system/subsystem/database_subsystem.h"

using namespace error_system::traits;
using namespace error_system::subsystem;

// 枚举转字符串
constexpr const char* name = subsystem_traits<database_subsystem_t>::to_string(database_subsystem_t::mysql);
// 结果: "mysql"

// 字符串转枚举
constexpr auto subsys = subsystem_traits<database_subsystem_t>::from_string("redis");
// 结果: database_subsystem_t::redis

// 枚举转整数
constexpr auto value = subsystem_traits<database_subsystem_t>::to_int(database_subsystem_t::mysql);
// 结果: 0x0701
```

### 7. 多语言翻译 (i18n)

内置的 `json_translator_t` 支持将错误码映射为具体语言的字符串：

```cpp
#include "error_system/i18n/json_translator.h"
#include "error_system/i18n/translator_registry.h"

using namespace error_system::i18n;

// 创建翻译器
json_translator_t translator(language_t::zh_cn);
std::string error_msg = translator.translate(db_err);

// 或使用全局注册表
translator_registry_t::instance().set(&translator);
std::string msg = ctx.to_string(); // 自动使用全局翻译器
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

---

## 架构概览 (Architecture)

```
┌─────────────────────────────────────────────────────────────┐
│                        Error System                          │
├─────────────────────────────────────────────────────────────┤
│  Core Layer                                                  │
│  ├── error_code_t    (64位错误码数据类)                      │
│  ├── error_builder_t (编译期错误码构建器)                    │
│  ├── error_level_t   (错误等级枚举)                          │
│  ├── error_context_t (错误上下文：码+消息+因果链+负载+堆栈)  │
│  ├── error_config_t  (全局错误配置：堆栈阈值、默认语言)      │
│  ├── result_t<T>     (类Rust Result，替代异常传递错误)       │
│  └── error_registry_t(错误码注册器)                          │
├─────────────────────────────────────────────────────────────┤
│  Domain Layer                                                │
│  └── system_domain_t (18大系统域定义)                        │
├─────────────────────────────────────────────────────────────┤
│  Subsystem / Module Layer                                    │
│  ├── 36+ 个子系统枚举 (kernel_cpu, database_sql, ai_llm...) │
│  └── 17 个模块枚举   (kernel, network, database..., common) │
├─────────────────────────────────────────────────────────────┤
│  Traits Layer                                                │
│  ├── subsystem_traits<> (枚举↔字符串↔整数 转换)             │
│  └── module_traits<>    (枚举↔字符串↔整数 转换)             │
├─────────────────────────────────────────────────────────────┤
│  i18n Layer                                                  │
│  ├── i_translator_t     (翻译器接口)                         │
│  ├── json_translator_t  (JSON字典翻译实现)                   │
│  ├── translator_registry_t(全局翻译器单例注册表)             │
│  └── language_t         (支持语言枚举)                       │
├─────────────────────────────────────────────────────────────┤
│  Plugin Layer                                                │
│  ├── i_error_plugin_t   (插件抽象接口)                       │
│  └── plugin_registry_t  (插件单例注册表，负责广播)           │
├─────────────────────────────────────────────────────────────┤
│  Memory Layer                                                │
│  └── object_pool_t<T>   (线程局部对象池，优化高频分配)       │
├─────────────────────────────────────────────────────────────┤
│  Utils Layer                                                 │
│  ├── string_utils_t     (字符串处理: hash, trim, format...) │
│  ├── json_utils_t       (JSON解析与字典)                     │
│  ├── file_utils_t       (文件读写操作)                       │
│  └── stack_trace_utils_t(跨平台堆栈跟踪)                     │
└─────────────────────────────────────────────────────────────┘
```

### 支持的系统域

| 系统域 | 枚举值 | 说明 |
|--------|--------|------|
| `system` | 0x00 | 系统基础 |
| `kernel` | 0x01 | 内核/操作系统 |
| `middleware` | 0x02 | 中间件 |
| `application` | 0x03 | 应用程序 |
| `service` | 0x04 | 服务层 |
| `network` | 0x05 | 网络 |
| `storage` | 0x06 | 存储 |
| `database` | 0x07 | 数据库 |
| `security` | 0x08 | 安全 |
| `ai` | 0x09 | 人工智能 |
| `cloud` | 0x0A | 云计算 |
| `edge` | 0x0B | 边缘计算 |
| `iot` | 0x0C | 物联网 |
| `blockchain` | 0x0D | 区块链 |
| `bigdata` | 0x0E | 大数据 |
| `devops` | 0x0F | DevOps |
| `distributed` | 0x10 | 分布式系统 |
| `monitoring` | 0x11 | 监控告警 |

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
auto content = utils::file_utils::read_file("config.json");
utils::file_utils::write_file("output.txt", "Hello, World!");
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

### 作为子项目使用

```cmake
find_package(error_system 1.0.0 REQUIRED)

add_executable(my_business_app main.cpp)
target_link_libraries(my_business_app PRIVATE error_system::error_system)
```

---

## 目录结构 (Directory Structure)

```text
error_system/
├── CMakeLists.txt              # CMake 配置
├── README.md                   # 项目说明
├── include/error_system/       # 对外公开的头文件
│   ├── core/                   # 核心定义 (error_code_t, error_builder_t, error_level_t, error_context_t, result_t, error_config_t, error_exception_t)
│   ├── domain/                 # 系统域定义 (system_domain_t)
│   ├── module/                 # 17 个模块枚举定义
│   ├── subsystem/              # 36+ 个子系统枚举定义
│   ├── traits/                 # 类型萃取
│   │   ├── module/             # 模块 traits (枚举↔字符串↔整数)
│   │   └── subsystem/          # 子系统 traits
│   ├── i18n/                   # 多语言翻译接口与实现
│   │   └── languages/          # JSON 翻译字典 (zh_cn.json, en_us.json)
│   ├── plugin/                 # 插件系统接口
│   ├── memory/                 # 内存管理 (对象池)
│   └── utils/                  # 辅助工具 (string, json, file, stack_trace, source_location, error_formatter)
├── src/                        # 核心实现代码
│   ├── core/                   # error_context 实现
│   ├── i18n/                   # json_translator, translator_registry 实现
│   ├── plugin/                 # plugin_registry 实现
│   └── utils/                  # 工具函数实现
├── tests/                      # GoogleTest 单元测试
│   ├── core/                   # 核心层测试
│   ├── i18n/                   # i18n 层测试
│   ├── plugin/                 # 插件层测试
│   ├── traits/                 # traits 测试
│   └── utils/                  # 工具库测试
├── examples/                   # 示例代码
│   ├── demo_01.cc              # 基础功能演示
│   └── demo_02.cc              # 高级功能演示
├── script/                     # 代码生成脚本
│   ├── generate_i18n.sh        # 生成翻译字典
│   └── generate_traits.sh      # 生成 traits 文件
└── LICENSE                     # 许可证
```

---

## 许可证 (License)

本项目遵循相关开源许可证（详见项目根目录下的 [LICENSE](LICENSE) 文件）。
