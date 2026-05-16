# Memory 层 API 文档

> 命名空间：`error_system::memory`

Memory 层提供高性能的内存管理组件，主要用于优化高频错误上下文创建场景下的内存分配。

---

## object_pool_t

**头文件**：`error_system/memory/object_pool.h`

线程安全的对象池模板类，预先分配固定大小的内存，避免频繁的动态内存分配。适用于高频创建/销毁 `error_context_t` 等对象的场景。

### 模板参数

| 参数 | 说明 |
|------|------|
| `T` | 对象类型 |
| `Capacity` | 对象池容量，编译期确定 |

### 类型定义

```cpp
using value_type = T;
using size_type = size_t;
using difference_type = std::ptrdiff_t;
using pointer = T*;
using const_pointer = const T*;
using reference = T&;
using const_reference = const T&;
```

### 方法

```cpp
// 构造函数
object_pool_t() noexcept;

// 析构函数
~object_pool_t();

// 从对象池获取对象
pointer acquire() noexcept;

// 归还对象到对象池
void release(pointer& object) noexcept;

// 获取对象池容量
size_type capacity() const noexcept;

// 获取对象池当前对象数量
size_type size() const noexcept;

// 检查对象池是否为空
bool empty() const noexcept;

// 检查对象池是否已满
bool full() const noexcept;

// 获取单例对象池（全局唯一）
static object_pool_t<value_type, Capacity>& instance() noexcept;

// 获取线程局部对象池（每个线程独立）
static object_pool_t<value_type, Capacity>& instance_thread_local() noexcept;
```

### 工作原理

对象池使用**空闲链表**算法管理内存：

1. **初始化**：预先分配 `Capacity` 个对象和索引数组
2. **获取对象** (`acquire`)：从空闲链表头部取出一个对象
3. **归还对象** (`release`)：将对象放回空闲链表头部
4. **池满处理**：当池满时 `acquire()` 返回 `nullptr`

```
对象池内存布局：
┌─────────────────────────────────────────────────────┐
│  objects_ [Capacity]                                │
│  ┌─────┬─────┬─────┬─────┬─────┬─────┐             │
│  │ T[0]│ T[1]│ T[2]│ T[3]│ ... │T[N-1]│             │
│  └─────┴─────┴─────┴─────┴─────┴─────┘             │
├─────────────────────────────────────────────────────┤
│  next_indexs_ [Capacity]                            │
│  ┌─────┬─────┬─────┬─────┬─────┬─────┐             │
│  │  0  │  1  │  2  │  3  │ ... │ N-1 │  ← 空闲索引 │
│  └─────┴─────┴─────┴─────┴─────┴─────┘             │
├─────────────────────────────────────────────────────┤
│  next_index_ = 0                                    │
│  count_ = 0                                         │
└─────────────────────────────────────────────────────┘
```

### 使用示例

```cpp
#include "error_system/memory/object_pool.h"

using namespace error_system::memory;

// 定义对象池类型
using context_pool_t = object_pool_t<error_context_t, 100>;

// 获取全局单例对象池
auto& pool = context_pool_t::instance();

// 获取对象
error_context_t* ctx = pool.acquire();
if (ctx != nullptr) {
    // 使用对象
    new (ctx) error_context_t(code, "错误信息");
    
    // ... 使用 ctx ...
    
    // 销毁对象（调用析构函数）
    ctx->~error_context_t();
    
    // 归还对象池
    pool.release(ctx);
}

// 获取线程局部对象池（无锁，更高性能）
auto& thread_pool = context_pool_t::instance_thread_local();
```

### 性能特点

| 特性 | 说明 |
|------|------|
| O(1) 获取/归还 | 空闲链表实现，常数时间复杂度 |
| 无锁线程局部 | `instance_thread_local()` 提供无锁访问 |
| 零动态分配 | 构造时一次性分配，运行时无堆分配 |
| 内存连续性 | 对象数组连续存储，缓存友好 |

### 注意事项

1. **对象生命周期管理**：获取的对象需要手动调用构造函数和析构函数
2. **容量限制**：池满时 `acquire()` 返回 `nullptr`，需要处理回退逻辑
3. **线程安全**：全局单例使用内部同步机制，线程局部实例无锁
4. **对象重置**：归还前确保对象状态已清理，避免数据泄漏

### 单元测试

测试文件：`tests/memory/object_pool_test.cc`

| 测试用例 | 描述 |
|----------|------|
| `instance_returns_singleton` | 单例返回相同实例 |
| `initial_state_is_empty` | 初始状态为空 |
| `acquire_returns_valid_object` | 获取返回有效对象 |
| `release_returns_object_to_pool` | 归还对象到对象池 |
| `full_pool_returns_nullptr` | 池满返回 nullptr |
| `acquire_after_release_works` | 释放后重新获取 |
| `multiple_acquire_release_cycles` | 多次获取释放循环 |
| `instance_thread_local_returns_different_instance` | 线程局部返回不同实例 |
| `release_invalid_pointer_does_nothing` | 释放无效指针无操作 |
| `capacity_matches_template_parameter` | 容量匹配模板参数 |

---

## 测试总结

Memory 层共包含 **1 个测试文件**，**10 个测试用例**：

| 模块 | 测试文件 | 测试数量 |
|------|----------|----------|
| object_pool | `tests/memory/object_pool_test.cc` | 10 |

**运行测试**:

```bash
cd build
ctest --output-on-failure
```
