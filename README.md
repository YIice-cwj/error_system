# Error System

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.15%2B-green.svg)](https://cmake.org)
[![GoogleTest](https://img.shields.io/badge/Tests-271%20passing-brightgreen.svg)](https://github.com/google/googletest)

> 🎯 高性能 C++17 错误码管理系统 — 将完整错误上下文封装在一个 64 位整数中，零开销构建与解析。

---

## 📑 目录

- [✨ 核心特性](#-核心特性)
- [🔢 错误码位域](#-错误码位域)
- [🚀 快速上手](#-快速上手)
- [🏗️ 架构总览](#️-架构总览)
- [🔧 编译安装](#-编译安装)
- [📁 目录结构](#-目录结构)
- [📚 文档索引](#-文档索引)

---

## ✨ 核心特性

| 🔹 特性 | 📝 说明 |
|:---|------|
| `constexpr` 全链路 | 编译期错误码构建，进入只读数据段 |
| 位移 + 掩码 | 64 位字段拆解，100% 避免位域未定义行为 |
| 结构化负载 | `with()` 支持 int/bool/double 多类型，`with_batch()` 批量添加 |
| Per-Code 堆栈 | 全局阈值 + 按错误码粒度覆盖堆栈触发策略 |
| 源位置追踪 | 自动捕获文件名、函数名、行号，支持短文件名模式 |
| 插件系统 | 同步/异步通知、级别过滤、RCU 零拷贝读取、背压控制 |
| `result_t<T>` | 类 Rust Result，零异常错误传递，支持 map/map_error 链式操作 |
| 代码生成 | Python 脚本从 JSON 配置一键生成 C++ 头文件 + 字典 + 文档 |

---

## 🔢 错误码位域

> 💡 一个 `uint64_t` 承载完整路由信息

| 位区间 | 长度 | 字段 | 说明 |
|:---|:---:|:---|:---|
| `63` | 1 | **Sign** | `0` = 错误 ❌ &nbsp; `1` = 成功 ✅ |
| `60-62` | 3 | Reserved | 预留 |
| `56-59` | 4 | **Level** | debug · info · warn · error · fatal |
| `48-55` | 8 | **System** | 6 大系统域 |
| `32-47` | 16 | Subsystem | 最大 65535 |
| `16-31` | 16 | Module | 最大 65535 |
| `0-15` | 16 | Number | 最大 65535 |

---

## 🚀 快速上手

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

std::cout << ctx.to_string();        // 人类可读
std::string json = ctx.to_json();    // JSON 格式
std::string bin  = ctx.to_binary();  // 紧凑二进制
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
if (!r) { std::cerr << r.error().to_string(); }

int val = r.value_or(0);             // 失败返回默认值
if (auto* p = r.value_pointer()) {}  // 失败返回 nullptr
r.map([](int v) { return std::to_string(v); });  // 链式转换
```

### 4. 注册插件

```cpp
class log_plugin_t : public plugin::i_error_plugin_t {
    std::string_view name() const noexcept override { return "logger"; }
    void on_error(const core::error_context_t& ctx) noexcept override {
        std::cerr << ctx.to_string() << "\n";
    }
};

log_plugin_t logger;
plugin::plugin_registry_t::instance().register_plugin(&logger);
// error_context_t 构造时自动通知所有插件
```

### 5. 代码生成

```bash
python script/script_py/generate_all.py
```

> 🤖 从 JSON 配置一键生成 C++ 头文件 + 错误字典 + Markdown 文档

---

## 🏗️ 架构总览

```
┌──────────────────────────────────────────────────────────┐
│                     Error System                          │
├──────────┬───────────────────────────────────────────────┤
│ ⚙️ Config │ 全局配置 + per-code 覆盖 + 通知模式           │
├──────────┼───────────────────────────────────────────────┤
│ 🧩 Core   │ error_code_t · error_context_t · result_t<T> │
│           │ error_registry_t · error_exception_t         │
├──────────┼───────────────────────────────────────────────┤
│ 🌐 Domain │ none · system · middleware · database        │
│           │ application · third_party                    │
├──────────┼───────────────────────────────────────────────┤
│ 🔌 Plugin │ i_error_plugin_t · plugin_registry_t         │
│           │ error_router_plugin_t                        │
├──────────┼───────────────────────────────────────────────┤
│ 🛠️ Utils  │ async_queue_t · string_utils_t · json_dict_t │
│           │ file_utils_t · stack_trace_utils_t           │
└──────────┴───────────────────────────────────────────────┘
```

---

## 🔧 编译安装

```bash
git clone https://github.com/YIice-cwj/error_system.git
cd error_system && mkdir build && cd build
cmake .. && cmake --build . -j$(nproc)
ctest --output-on-failure
```

### 作为子项目引入

```cmake
include(FetchContent)
FetchContent_Declare(error_system
    GIT_REPOSITORY https://github.com/YIice-cwj/error_system.git
    GIT_TAG v2.3.0)
FetchContent_MakeAvailable(error_system)
target_link_libraries(my_app PRIVATE error_system::error_system)

# 🤖 一键生成错误码
error_system_generate_codes(TARGET my_app JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config/errors)
```

---

## 📁 目录结构

```
error_system/
├── include/error_system/   # 📦 头文件
│   ├── config/             #   ⚙️ 全局配置
│   ├── core/               #   🧩 错误码、上下文、Result、注册表
│   ├── domain/             #   🌐 6 大系统域
│   ├── plugin/             #   🔌 插件接口、注册表、路由
│   └── utils/              #   🛠️ 字符串、JSON、文件、堆栈、异步队列
├── src/                    # 🔨 核心实现
├── tests/                  # 🧪 GoogleTest (15 文件 · 271 用例)
├── config/errors/          # 📝 JSON 错误码配置
├── script/script_py/       # 🐍 Python 代码生成工具
├── examples/               # 📖 示例代码 (5 个 demo)
└── docs/                   # 📚 API 文档 + 架构设计
```

---

## 📚 文档索引

| 📄 文档 | 📝 内容 |
|------|------|
| [Core 层 API](docs/api/core.md) | `error_code_t` `error_context_t` `result_t` `error_registry_t` |
| [Config 层 API](docs/api/config.md) | 全局配置、per-code 覆盖、通知模式 |
| [Plugin 层 API](docs/api/plugin.md) | 插件接口、注册表、路由分发 |
| [Utils 层 API](docs/api/utils.md) | 字符串工具、JSON 解析、文件操作、异步队列 |
| [架构设计](docs/architecture.md) | 模块划分、依赖关系、关键设计决策 |

---

## 📜 许可证

详见 [LICENSE](LICENSE)。