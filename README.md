# Error System

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.15%2B-green.svg)](https://cmake.org)
[![GoogleTest](https://img.shields.io/badge/Tests-294%20passing-brightgreen.svg)](https://github.com/google/googletest)

> 高性能 C++17 错误码管理系统 — 将完整错误上下文封装在一个 64 位整数中，零开销构建与解析。

---

## 目录

- [核心特性](#核心特性)
- [错误码位域](#错误码位域)
- [快速上手](#快速上手)
- [安装教程](#安装教程)
- [架构总览](#架构总览)
- [目录结构](#目录结构)
- [示例索引](#示例索引)
- [错误码自动生成](#错误码自动生成)
- [文档索引](#文档索引)

---

## 核心特性

| 特性 | 说明 |
|:---|------|
| `constexpr` 全链路 | 编译期错误码构建，进入只读数据段 |
| 位移 + 掩码 | 64 位字段拆解，100% 避免位域未定义行为 |
| 结构化负载 | `with()` 支持 int/bool/double 多类型，`with_batch()` 批量添加，SSO 栈上零堆分配 |
| Per-Code 堆栈 | 全局阈值 + 按错误码粒度覆盖堆栈触发策略 |
| 源位置追踪 | 自动捕获文件名、函数名、行号，支持短文件名模式 |
| 插件系统 | 同步/异步通知、级别过滤、RCU 零拷贝读取、背压控制 |
| `result_t<T>` | 类 Rust Result，零异常错误传递，支持 map/map_error/and_then/match 链式操作 |
| 因果链 | `wrap()` 构建多层错误因果链，序列化时递归输出 |
| 代码生成 | Python 脚本从 JSON 配置一键生成 C++ 头文件 + 字典 + 文档 |
| 安全性 | 全 `noexcept` 设计，`std::get_if` + 哨兵值避免异常，`std::bad_alloc` 内部捕获 |

---

## 错误码位域

> 一个 `uint64_t` 承载完整路由信息

| 位区间 | 长度 | 字段 | 说明 |
|:---|:---:|:---|:---|
| `63` | 1 | **Sign** | `0` = 错误 &nbsp; `1` = 成功 |
| `60-62` | 3 | Reserved | 预留 |
| `56-59` | 4 | **Level** | debug · info · warn · error · fatal |
| `48-55` | 8 | **System** | 6 大系统域 |
| `32-47` | 16 | Subsystem | 最大 65535 |
| `16-31` | 16 | Module | 最大 65535 |
| `0-15` | 16 | Number | 最大 65535 |

---

## 快速上手

### 1. 构建错误码

```cpp
#include "error_system.h"
using namespace error_system;

// 编译期构建，零运行时开销
constexpr auto db_err = error_code_t{
    core::error_level_t::fatal,
    domain::system_domain_t::database,
    100, 200, 404
};

auto level  = db_err.get_level();   // fatal
auto system = db_err.get_system();  // database
```

### 2. 使用错误上下文

```cpp
error_context_t ctx(db_err, "数据库连接失败: {}", "timeout");

ctx.with("host", "192.168.1.100")
   .with("port", 3306)
   .with("retry", true);

// 序列化由独立的 serializer 类提供（SRP）
std::cout << error_context_serializer_t::to_string(ctx);   // 人类可读
std::string json = error_context_serializer_t::to_json(ctx);    // JSON 格式
std::string bin  = error_context_serializer_t::to_binary(ctx);  // 紧凑二进制
```

### 3. Result 错误传递

```cpp
result_t<int> divide(int a, int b) {
    if (b == 0)
        return result_t<int>::make_error(
            error_code_t(error_level_t::error, system_domain_t::application, 0, 0, 1),
            "除数不能为零");
    return a / b;
}

auto r = divide(10, 0);
if (!r) { std::cerr << error_context_serializer_t::to_string(r.error()); }

int val = r.unwrap_or(0);           // 失败返回默认值
if (auto* p = r.value_pointer()) {} // 失败返回 nullptr
r.map([](int v) { return std::to_string(v); });  // 链式转换

// match() 模式匹配，强制同时处理成功与错误路径
auto msg = r.match(
    [](int v) { return "ok: " + std::to_string(v); },
    [](const error_context_t& e) { return "fail: " + std::string(e.message); });
```

### 4. 注册插件

```cpp
class log_plugin_t : public plugin::i_error_plugin_t {
    std::string_view name() const noexcept override { return "logger"; }
    core::error_level_t min_level() const noexcept override {
        return core::error_level_t::error;  // 仅接收 error 及以上级别
    }
    void on_error(const core::error_context_t& ctx) noexcept override {
        std::cerr << error_context_serializer_t::to_string(ctx) << "\n";
    }
};

log_plugin_t logger;
plugin::plugin_registry_t::instance().register_plugin_ref(logger);
// error_context_t 构造时自动通知所有插件
```

### 5. 因果链（Cause Chain）

```cpp
error_context_t root{infra::redis_errors::ERR_KEY_NOT_FOUND, "Redis 键缺失"};
error_context_t wrapper{biz::payment_errors::ERR_ACCOUNT_FROZEN, "支付失败"};
auto chained = wrapper.wrap(root);
// to_string() 会递归输出 "Caused by:" 链
```

---

## 安装教程

### 环境要求

| 依赖 | 最低版本 | 说明 |
|------|:---:|------|
| C++ 编译器 | C++17 | GCC 9+ / Clang 10+ / MSVC 19.20+ |
| CMake | 3.15 | 构建系统 |
| Python | 3.6 | 错误码自动生成（可选，缺失时跳过代码生成） |
| GoogleTest | - | 测试框架（构建系统自动拉取，无需手动安装） |

> macOS 堆栈追踪依赖系统库；Linux 需 `libdl`（自动链接）；Windows 需 `dbghelp`（自动链接）。

### 方式一：源码构建（推荐）

```bash
# 1. 克隆仓库
git clone https://github.com/YIice-cwj/error_system.git
cd error_system

# 2. 配置（默认 Release + 全特性开启）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 3. 编译（自动从 JSON 生成错误码头文件）
cmake --build build -j$(nproc)        # Linux
cmake --build build -j$(sysctl -n hw.ncpu)   # macOS

# 4. 运行测试（可选）
cd build && ctest --output-on-failure && cd ..

# 5. 安装到系统（可选，默认 /usr/local）
cmake --install build
```

**安装产物**：

| 路径 | 内容 |
|------|------|
| `include/error_system/` | 公共头文件 |
| `lib/liberror_system.a` / `.so` | 静态/动态库 |
| `lib/cmake/error_system/` | CMake 配置文件（`error_systemConfig.cmake` 等） |
| `share/error_system/scripts/` | Python 代码生成脚本 |

### 方式二：作为子项目引入（FetchContent）

```cmake
# 在你的 CMakeLists.txt 中
include(FetchContent)
FetchContent_Declare(error_system
    GIT_REPOSITORY https://github.com/YIice-cwj/error_system.git
    GIT_TAG v2.3.0)
FetchContent_MakeAvailable(error_system)

target_link_libraries(my_app PRIVATE error_system::error_system)

# 一键生成你自己的错误码头文件（可选）
error_system_generate_codes(
    TARGET my_app
    JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config/errors
)
```

### 方式三：作为子目录引入

```cmake
# 你的项目根 CMakeLists.txt
add_subdirectory(third_party/error_system)

target_link_libraries(my_app PRIVATE error_system::error_system)
```

### 编译选项

| CMake 选项 | 默认 | 说明 |
|------|:---:|------|
| `ERROR_SYSTEM_ENABLE_STACKTRACE` | ON | 堆栈追踪 + per-code API |
| `ERROR_SYSTEM_ENABLE_VALIDATION` | ON | 错误码注册校验 |
| `ERROR_SYSTEM_ENABLE_LOCATION` | ON | 源位置追踪 |
| `ERROR_SYSTEM_ENABLE_LTO` | OFF | LTO 优化 |
| `ERROR_SYSTEM_ENABLE_ASAN` | OFF | AddressSanitizer |
| `ERROR_SYSTEM_ENABLE_UBSAN` | OFF | UBSan |
| `ERROR_SYSTEM_WARNINGS_AS_ERRORS` | ON | 警告视为错误 |
| `BUILD_SHARED_LIBS` | OFF | 构建动态库 |
| `ERROR_SYSTEM_BUILD_TESTS` | ON | 构建单元测试 |
| `ERROR_SYSTEM_BUILD_EXAMPLES` | ON | 构建示例 |
| `ERROR_SYSTEM_BUILD_PERF_TESTS` | ON | 构建性能测试 |

**示例：定制构建**

```bash
# 最小化构建（关闭堆栈/位置/验证，适合嵌入式）
cmake -S . -B build-min \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DERROR_SYSTEM_ENABLE_STACKTRACE=OFF \
    -DERROR_SYSTEM_ENABLE_LOCATION=OFF \
    -DERROR_SYSTEM_ENABLE_VALIDATION=OFF \
    -DERROR_SYSTEM_BUILD_TESTS=OFF \
    -DERROR_SYSTEM_BUILD_EXAMPLES=OFF

# 调试构建（带 ASan/UBSan）
cmake -S . -B build-asan \
    -DCMAKE_BUILD_TYPE=Debug \
    -DERROR_SYSTEM_ENABLE_ASAN=ON \
    -DERROR_SYSTEM_ENABLE_UBSAN=ON

# 性能基线构建（带 LTO + PGO）
cmake -S . -B build-perf \
    -DCMAKE_BUILD_TYPE=Release \
    -DERROR_SYSTEM_ENABLE_LTO=ON
```

### 在你的项目中使用

```cpp
// my_app.cc
#include "error_system.h"
#include "trade_service_errors.h"  // 自动生成的业务错误码

result_t<void> create_order(int user_id) {
    if (user_id <= 0) {
        return result_t<void>::make_error(
            biz::trade_errors::ERR_ORDER_NOT_FOUND, "无效的用户 ID: {}", user_id);
    }
    return result_t<void>::make_success();
}
```

```bash
g++ -std=c++17 my_app.cc -o my_app \
    -I<path-to-error_system>/include \
    -I<path-to-error_system>/build/generated_errors/include \
    -L<path-to-error_system>/build -lerror_system
```

### 卸载

```bash
# 卸载已安装的文件并清理空目录
cmake --build build --target uninstall
```

---

## 架构总览

```
┌──────────────────────────────────────────────────────────┐
│                     Error System                          │
├──────────┬───────────────────────────────────────────────┤
│ Config   │ 全局配置 + per-code 覆盖 + 通知模式           │
│          │ feature_flags_t · stacktrace_config_t         │
│          │ formatter_config_t                            │
├──────────┼───────────────────────────────────────────────┤
│ Core     │ error_code_t · error_context_t · result_t<T>  │
│          │ error_context_serializer_t · error_registry_t │
│          │ error_builder_t · error_exception_t           │
├──────────┼───────────────────────────────────────────────┤
│ Domain   │ none · system · middleware · database         │
│          │ application · third_party                     │
├──────────┼───────────────────────────────────────────────┤
│ Plugin   │ i_error_plugin_t · plugin_registry_t          │
│          │ error_router_plugin_t · async_notification_*  │
├──────────┼───────────────────────────────────────────────┤
│ Utils    │ async_queue_t · string_utils_t · json_dict_t  │
│          │ file_utils_t · stack_trace_utils_t            │
│          │ source_location_t · json_lexer_t              │
└──────────┴───────────────────────────────────────────────┘
```

---

## 目录结构

```
error_system/
├── include/error_system/   # 头文件
│   ├── config/             #   配置（feature_flags / stacktrace / formatter）
│   ├── core/               #   错误码、上下文、序列化器、Result、注册表
│   ├── domain/             #   6 大系统域
│   ├── plugin/             #   插件接口、注册表、路由、异步通知通道
│   └── utils/              #   字符串、JSON、文件、堆栈、异步队列
├── src/                    # 核心实现
├── tests/                  # GoogleTest (19 文件 · 294 用例)
├── config/errors/          # JSON 错误码配置
├── script/script_py/       # Python 代码生成工具
├── examples/               # 示例代码 (6 个 demo)
└── docs/                   # API 文档 + 架构设计 + 错误码生成
```

---

## 示例索引

| 示例 | 文件 | 演示内容 |
|------|------|------|
| Demo 1 | [examples/demo01.cc](examples/demo01.cc) | 基础用法：错误码构建、上下文、payload |
| Demo 2 | [examples/demo02.cc](examples/demo02.cc) | Result 链式操作、错误传递 |
| Demo 3 | [examples/demo03.cc](examples/demo03.cc) | 插件系统、同步/异步通知 |
| Demo 4 | [examples/demo04.cc](examples/demo04.cc) | JSON/二进制序列化、跨进程传递 |
| Demo 5 | [examples/demo05.cc](examples/demo05.cc) | 自动生成错误码、注册表查询 |
| Demo 6 | [examples/demo06.cc](examples/demo06.cc) | 高级用法：match / 因果链 / 自定义格式化器 / 重复策略 / 端到端综合场景 |

运行示例：

```bash
cmake --build build --target demo06
./build/examples/demo06
```

---

## 错误码自动生成

详细文档见 [错误码自动生成指南](docs/error_code_generation.md)。

**快速开始**：

```bash
# 1. 在 config/errors/ 下编写 JSON 配置
#    （参考 config/errors/trade_service_errors.json）

# 2. 一键生成（CMake 构建时自动触发）
cmake --build build

# 3. 或手动运行
python script/script_py/generate_all.py

# 4. 生成产物位于 build/generated_errors/
#    ├── include/
#    │   ├── trade_service_errors.h       # C++ 错误码定义（DEFINE_ERROR_CODE 宏）
#    │   ├── payment_service_errors.h
#    │   └── error_dict.h                 # 子系统/模块名称注册表
#    └── error_dictionary.md              # Markdown 错误码字典文档
```

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [Core 层 API](docs/api/core.md) | `error_code_t` `error_context_t` `result_t` `error_registry_t` `error_context_serializer_t` |
| [Config 层 API](docs/api/config.md) | `feature_flags_t` `stacktrace_config_t` `formatter_config_t` |
| [Plugin 层 API](docs/api/plugin.md) | 插件接口、注册表、路由分发、异步通知通道 |
| [Utils 层 API](docs/api/utils.md) | 字符串工具、JSON 解析、文件操作、异步队列、堆栈跟踪 |
| [架构设计](docs/architecture.md) | 模块划分、依赖关系、关键设计决策 |
| [错误码自动生成](docs/error_code_generation.md) | JSON 配置格式、生成脚本、CMake 集成、自定义错误码 |

---

## 许可证

详见 [LICENSE](LICENSE)。
