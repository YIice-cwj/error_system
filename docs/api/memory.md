# Memory 层 API 参考

> 命名空间: `error_system::memory`

---

## object_pool_t\<T, Capacity\>

线程局部对象池，用于减少高频场景下的堆内存分配开销。特别适用于 `error_context_t` 因果链节点的分配。

### 核心 API

```cpp
template<typename T, size_t Capacity = 64>
class object_pool_t {
public:
    static object_pool_t& instance_thread_local() noexcept;

    T* acquire() noexcept;
    bool release(T* ptr) noexcept;
    void clear() noexcept;

    size_t size() const noexcept;
    size_t capacity() const noexcept;
    bool empty() const noexcept;
    bool full() const noexcept;
};
```

### 模板参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `T` | - | 池化对象类型 |
| `Capacity` | `64` | 对象池容量 |

### 线程安全

`object_pool_t` 使用线程局部存储（TLS），每个线程拥有独立的池实例，无需加锁：

```cpp
// 线程 A
auto& pool_a = object_pool_t<error_context_t>::instance_thread_local();
auto* ptr = pool_a.acquire();  // 无锁操作

// 线程 B
auto& pool_b = object_pool_t<error_context_t>::instance_thread_local();
auto* ptr2 = pool_b.acquire();  // 无锁操作，与线程 A 互不干扰
```

### 使用示例

```cpp
using namespace error_system::memory;

// 获取线程局部对象池
auto& pool = object_pool_t<error_context_t>::instance_thread_local();

// 从池中获取对象
error_context_t* ptr = pool.acquire();
if (ptr) {
    // 使用对象
    *ptr = error_context_t(code, "错误信息");

    // 使用完后归还到池中
    bool released = pool.release(ptr);
    // released == true 表示归还成功
}
```

### 与 error_context_t 集成

`error_context_t::wrap()` 内部使用对象池优化因果链节点分配：

```cpp
error_context_t error_context_t::wrap(error_code_t new_code, std::string new_message) const {
    error_context_t new_code_context;
    new_code_context.code = new_code;
    new_code_context.message = std::move(new_message);

    // 尝试从对象池获取 cause 节点
    object_pool_t& pool = object_pool_t::instance_thread_local();
    error_context_t* ptr = pool.acquire();
    if (ptr) {
        *ptr = *this;
        new_code_context.cause = std::shared_ptr<error_context_t>(
            ptr,
            [&pool](error_context_t* p) { pool.release(p); }
        );
    } else {
        // 池已满，回退到堆分配
        new_code_context.cause = std::make_shared<error_context_t>(*this);
    }

    return new_code_context;
}
```

### 性能优势

| 场景 | 无对象池 | 有对象池 |
|------|----------|----------|
| 单次分配 | `new`/`delete` 系统调用 | 栈/预分配数组操作 |
| 高频错误链（1000 次/秒） | 高 CPU、内存碎片 | 接近零分配开销 |
| 线程竞争 | 需要锁或无锁算法 | 无需同步（TLS） |

### 测试覆盖

| 测试文件 | 用例数 | 覆盖内容 |
|----------|--------|----------|
| `tests/memory/object_pool_test.cc` | 10 | 获取/释放、容量限制、清空、线程安全 |

---

## 使用建议

### 1. 适用场景

- **高频错误链构造**：如 RPC 调用链中的逐层错误包装
- **短生命周期对象**：如临时 `error_context_t` 节点
- **性能敏感路径**：如交易核心路径中的错误处理

### 2. 不适用场景

- **长生命周期对象**：对象池中的对象不应长期持有
- **大对象**：对象池适用于小型对象（如 `error_context_t`）
- **跨线程共享**：每个线程有独立池，不适合跨线程传递

### 3. 容量调优

```cpp
// 高频场景：增大容量
using high_freq_pool = object_pool_t<error_context_t, 256>;

// 低频场景：减小容量以节省内存
using low_freq_pool = object_pool_t<error_context_t, 16>;
```

### 4. 自定义类型支持

对象池支持任何可默认构造、可复制的类型：

```cpp
struct my_error_node_t {
    int error_id{0};
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

auto& pool = object_pool_t<my_error_node_t, 128>::instance_thread_local();
auto* node = pool.acquire();
if (node) {
    node->error_id = 404;
    node->message = "Not Found";
    node->timestamp = std::chrono::system_clock::now();
    pool.release(node);
}
```
