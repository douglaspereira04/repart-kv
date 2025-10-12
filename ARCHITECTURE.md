# Repart-KV Architecture

## Overview

Repart-KV is a high-performance key-value storage system built with C++20, featuring multiple storage backends with zero-overhead abstractions using CRTP (Curiously Recurring Template Pattern).

## Core Design Principles

1. **Zero-Cost Abstractions** - CRTP eliminates virtual function overhead
2. **No Virtual Functions** - All polymorphism resolved at compile time
3. **C++20 Concepts** - Type safety enforced at compile time
4. **Manual Locking** - Users control thread-safety granularity
5. **Template-Based** - Maximum flexibility and performance

## Architecture Layers

### 1. Storage Engine Layer (String Keys → String Values)

**Purpose**: Provides basic key-value storage for string data.

**Abstract Base Class**: `StorageEngine<Derived>` (CRTP)

**Interface**:
```cpp
std::string read(const std::string& key) const;
void write(const std::string& key, const std::string& value);
std::vector<std::pair<std::string, std::string>> scan(const std::string& prefix, size_t limit) const;
void lock() const;
void unlock() const;
void lock_shared() const;
void unlock_shared() const;
```

**Implementations**:

| Implementation | Backend | Order | Use Case |
|----------------|---------|-------|----------|
| `MapStorageEngine` | `std::map` | Sorted | In-memory, simple, sorted scans |
| `TkrzwHashStorageEngine` | TKRZW HashDBM | Unordered | Persistent, fast random access |
| `TkrzwTreeStorageEngine` | TKRZW TreeDBM | Sorted | Persistent, efficient range queries |

### 2. Key Storage Layer (String Keys → Generic Values)

**Purpose**: Generic key-value storage supporting integral types and pointers.

**Abstract Base Classes**:
- `KeyStorage<Derived, IteratorType, ValueType>` (CRTP)
- `KeyStorageIterator<Derived, ValueType>` (CRTP)

**Storage Interface**:
```cpp
bool get(const std::string& key, ValueType& value) const;
void put(const std::string& key, const ValueType& value);
IteratorType lower_bound(const std::string& key);
```

**Iterator Interface**:
```cpp
std::string get_key() const;
ValueType get_value() const;
Derived& operator++();
bool is_end() const;
```

**Implementations**:

| Implementation | Backend | Order | Value Types |
|----------------|---------|-------|-------------|
| `MapKeyStorage<T>` | `std::map` | Sorted | Integral, pointers |
| `TkrzwHashKeyStorage<T>` | TKRZW HashDBM | Unordered | Integral, pointers |
| `TkrzwTreeKeyStorage<T>` | TKRZW TreeDBM | Sorted | Integral, pointers |

### 3. Partitioned Key-Value Storage Layer (Future)

**Purpose**: Distributed partitioned storage across multiple engines.

**Status**: Planned (currently placeholder in `kvstorage/PartitionedKeyValueStorage.h`)

## CRTP Pattern Explained

### Traditional Virtual Functions (NOT USED)
```cpp
class Base {
public:
    virtual void operation() = 0;  // Runtime dispatch via vtable
};
```

**Overhead**: vtable lookup, prevents inlining, cache misses

### CRTP Pattern (USED)
```cpp
template<typename Derived>
class Base {
public:
    void operation() {
        static_cast<Derived*>(this)->operation_impl();  // Compile-time dispatch
    }
};
```

**Benefits**: Zero overhead, full inlining, compile-time type checking

## Type Safety with C++20 Concepts

### KeyStorageValueType Concept
```cpp
template<typename T>
concept KeyStorageValueType = std::integral<T> || std::is_pointer_v<T>;
```

**Enforces**: Only integral types (int, long, etc.) or pointers allowed

**Example**:
```cpp
MapKeyStorage<int> valid;       // ✓ Compiles
MapKeyStorage<double> invalid;  // ✗ Concept violation at compile time
```

## Thread Safety Model

All storage engines provide manual locking primitives:

### Read Operations (Shared Lock)
```cpp
engine.lock_shared();
std::string value = engine.read("key");
engine.unlock_shared();
```

Multiple readers can hold shared locks simultaneously.

### Write Operations (Exclusive Lock)
```cpp
engine.lock();
engine.write("key", "value");
engine.unlock();
```

Only one writer can hold an exclusive lock.

### Why Manual Locking?

- Fine-grained control over critical sections
- Batch operations under single lock
- Avoid lock overhead for single-threaded use
- User decides the granularity

## Storage Engine Comparison

### MapStorageEngine
- **Data Structure**: Red-black tree (`std::map`)
- **Order**: Lexicographically sorted
- **Persistence**: None (in-memory only)
- **Write Speed**: Fast (O(log n))
- **Read Speed**: Fast (O(log n))
- **Scan Speed**: Fast (O(k) for k results)
- **Best For**: Simple in-memory applications, development

### TkrzwHashStorageEngine
- **Data Structure**: Hash table
- **Order**: Unordered (hash-based)
- **Persistence**: Optional (file-based)
- **Write Speed**: Very fast (O(1) average)
- **Read Speed**: Very fast (O(1) average)
- **Scan Speed**: Slow (O(n) - must check all keys)
- **Best For**: Write-heavy workloads, random access patterns

### TkrzwTreeStorageEngine
- **Data Structure**: B+ tree
- **Order**: Lexicographically sorted
- **Persistence**: Optional (file-based)
- **Write Speed**: Fast (O(log n))
- **Read Speed**: Fast (O(log n))
- **Scan Speed**: Very fast (O(log n + k) for k results)
- **Best For**: Scan-heavy workloads, range queries, sorted data

## Performance Characteristics

Based on benchmarks with 10,000 entries:

| Operation | MapStorage | TkrzwHash | TkrzwTree |
|-----------|------------|-----------|-----------|
| Write | ~2M ops/sec | ~2M ops/sec | ~1.4M ops/sec |
| Read | ~2.5M ops/sec | ~3.3M ops/sec | ~1.7M ops/sec |
| Scan (1000) | Fast | ~11ms | ~732μs |

**Key Takeaway**: TkrzwTreeStorageEngine is **15x faster** for prefix scans!

## File Organization

```
storage/
├── StorageEngine.h              # CRTP base class
├── StorageEngineConcepts.h      # Concepts (currently unused due to CRTP circular dependency)
├── MapStorageEngine.h           # std::map implementation
├── TkrzwHashStorageEngine.h     # TKRZW HashDBM implementation
├── TkrzwTreeStorageEngine.h     # TKRZW TreeDBM implementation
└── interactive_storage_test.cpp # Interactive CLI tester

keystorage/
├── KeyStorage.h                 # CRTP base class
├── KeyStorageIterator.h         # CRTP iterator base
├── KeyStorageConcepts.h         # Type constraints
├── MapKeyStorage.h              # std::map implementation
├── TkrzwHashKeyStorage.h        # TKRZW HashDBM implementation
└── TkrzwTreeKeyStorage.h        # TKRZW TreeDBM implementation
```

## Usage Patterns

### Basic Usage - MapStorageEngine
```cpp
MapStorageEngine engine;
engine.write("key", "value");
std::string val = engine.read("key");
auto results = engine.scan("prefix:", 10);  // Returns vector<pair<string, string>>
for (const auto& [key, value] : results) {
    // Process key-value pairs
}
```

### Persistent Storage - TkrzwTreeStorageEngine
```cpp
TkrzwTreeStorageEngine engine("/path/to/db.tkt");
engine.write("key", "value");
engine.sync();  // Flush to disk
// ... program restart ...
TkrzwTreeStorageEngine engine2("/path/to/db.tkt");
std::string val = engine2.read("key");  // Data persisted!
```

### Generic Storage - MapKeyStorage
```cpp
MapKeyStorage<int> counters;
counters.put("visits", 100);
int visits;
counters.get("visits", visits);

auto it = counters.lower_bound("counter:");
while (!it.is_end()) {
    std::cout << it.get_key() << " = " << it.get_value() << std::endl;
    ++it;
}
```

### Thread-Safe Operations
```cpp
MapStorageEngine engine;

// Thread 1 (writer)
engine.lock();
engine.write("shared_key", "value");
engine.unlock();

// Thread 2 (reader)
engine.lock_shared();
std::string val = engine.read("shared_key");
engine.unlock_shared();
```

## Adding New Storage Backends

To add a new storage engine:

1. Create a class that inherits from `StorageEngine<YourEngine>`
2. Implement three methods:
   - `std::string read_impl(const std::string& key) const`
   - `void write_impl(const std::string& key, const std::string& value)`
   - `std::vector<std::pair<std::string, std::string>> scan_impl(const std::string& prefix, size_t limit) const`

3. The base class automatically provides the public interface and locking primitives

**Example**:
```cpp
class MyCustomEngine : public StorageEngine<MyCustomEngine> {
public:
    std::string read_impl(const std::string& key) const {
        // Your implementation
    }
    
    void write_impl(const std::string& key, const std::string& value) {
        // Your implementation
    }
    
    std::vector<std::pair<std::string, std::string>> scan_impl(const std::string& prefix, size_t limit) const {
        // Your implementation - return key-value pairs
    }
};
```

No virtual functions needed - everything is resolved at compile time!

## Future Enhancements

1. **Async Operations** - Add async/await support for I/O operations
2. **Batch Operations** - Optimized batch writes and reads
3. **Compression** - Transparent value compression
4. **Snapshots** - Point-in-time snapshots for backup
5. **Replication** - Multi-node replication support
6. **More Backends**:
   - RocksDB integration
   - LevelDB integration
   - Redis protocol support
   - Network-based distributed storage

All future backends will maintain the same zero-overhead CRTP design!

