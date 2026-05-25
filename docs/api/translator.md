# Translator 层 API 参考

> 命名空间: `error_system::translator`

---

## error_translator_t

错误翻译器，用于将子系统 ID 和模块 ID 翻译为人类可读的字符串。

### 核心 API

```cpp
class error_translator_t {
public:
    static error_translator_t& instance() noexcept;

    void register_subsystem(uint16_t subsys_id, std::string name) noexcept;
    void register_module(uint16_t subsys_id, uint16_t module_id, std::string name) noexcept;

    std::string translate(uint16_t subsys_id, uint16_t module_id) const noexcept;
};
```

### 方法说明

| 方法 | 说明 |
|------|------|
| `register_subsystem()` | 注册子系统 ID 与名称的映射 |
| `register_module()` | 注册模块 ID（含所属子系统）与名称的映射 |
| `translate()` | 根据子系统 ID 和模块 ID 翻译为可读字符串 |

### 使用示例

```cpp
using namespace error_system::translator;

auto& translator = error_translator_t::instance();

// 注册子系统名称
translator.register_subsystem(101, "交易服务");
translator.register_subsystem(102, "用户服务");

// 注册模块名称（需指定所属子系统）
translator.register_module(101, 1, "订单模块");
translator.register_module(101, 2, "持仓模块");
translator.register_module(102, 1, "认证模块");

// 翻译
std::string name = translator.translate(101, 1);
// 输出: "交易服务/订单模块"

std::string name2 = translator.translate(102, 1);
// 输出: "用户服务/认证模块"
```

### 线程安全

`error_translator_t` 使用 `std::shared_mutex` 保护内部映射表：
- `register_subsystem()` / `register_module()`：写锁
- `translate()`：读锁

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/translator/error_translator_test.cc` | 9 | 注册、翻译、静态名称查询、线程安全 |

---

## 静态名称查询

系统预定义了静态的子系统和模块名称查询函数，用于无需注册即可获取常见系统的名称。

```cpp
// 获取静态子系统名称
std::string_view get_static_subsys_name(uint16_t subsys_id) noexcept;

// 获取静态模块名称
std::string_view get_static_module_name(uint16_t subsys_id, uint16_t module_id) noexcept;
```

### 使用示例

```cpp
using namespace error_system::translator;

// 查询静态定义的名称
auto subsys_name = get_static_subsys_name(101);  // 可能返回 "交易服务"
auto module_name = get_static_module_name(101, 1); // 可能返回 "订单模块"
```

---

## 宏定义

### REGISTER_MODULE

在静态初始化阶段自动注册子系统和模块描述。

```cpp
REGISTER_MODULE(
    101,          // 子系统 ID
    1,            // 模块 ID
    "交易服务",   // 子系统名称
    "订单模块"    // 模块名称
);
```

宏展开后等价于：

```cpp
static struct legacy_module_registrar_101_1 {
    legacy_module_registrar_101_1() {
        error_translator_t::instance().register_subsystem(101, "交易服务");
        error_translator_t::instance().register_module(101, 1, "订单模块");
    }
} legacy_module_registrar_101_1_instance;
```

---

## 最佳实践

### 1. 集中注册翻译

建议在服务初始化时集中注册所有子系统和模块名称：

```cpp
void init_translations() {
    auto& translator = error_translator_t::instance();

    // 交易服务 (101)
    translator.register_subsystem(101, "交易服务");
    translator.register_module(101, 1, "订单模块");
    translator.register_module(101, 2, "持仓模块");
    translator.register_module(101, 3, "行情模块");
    translator.register_module(101, 4, "风控模块");

    // 用户服务 (102)
    translator.register_subsystem(102, "用户服务");
    translator.register_module(102, 1, "认证模块");
    translator.register_module(102, 2, "资料模块");
    translator.register_module(102, 3, "钱包模块");

    // Redis 组件 (51)
    translator.register_subsystem(51, "Redis组件");
    translator.register_module(51, 1, "连接模块");
    translator.register_module(51, 2, "操作模块");
    translator.register_module(51, 3, "集群模块");
}
```

### 2. 与代码生成工具配合

使用 `generate_error_codes.py` 生成的头文件自动包含翻译注册：

```cpp
// generated/trade_errors.h
namespace biz::trade_errors {
    inline constexpr error_code_t ERR_ORDER_NOT_FOUND = ...;

    inline void register_translations() {
        auto& t = error_translator_t::instance();
        t.register_subsystem(101, "交易服务");
        t.register_module(101, 1, "订单模块");
        // ...
    }
}
```

### 3. 在错误输出中使用翻译

```cpp
error_code_t code = ERR_ORDER_NOT_FOUND;

// 获取翻译后的名称
auto& translator = error_translator_t::instance();
std::string translated = translator.translate(code.get_subsystem(), code.get_module());
// 输出: "交易服务/订单模块"

// 结合 error_context_t 输出完整信息
error_context_t ctx(code, "订单不存在");
std::cout << "[" << translated << "] " << ctx.message << std::endl;
// 输出: [交易服务/订单模块] 订单不存在
```
