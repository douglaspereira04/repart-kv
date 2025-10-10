# KeyStorage Module

This module provides a generic key-value storage abstraction with support for integral types and pointers. **Uses CRTP (Curiously Recurring Template Pattern) for zero-overhead abstraction without virtual functions!**

## Requirements

- **C++20 or later** (uses concepts for type safety)

## Architecture

The module consists of three main components using compile-time polymorphism:

### 1. KeyStorageIterator (CRTP Base Class)
A CRTP base class for traversing key-value pairs in storage without virtual function overhead.

**Template Parameters:**
- `Derived` - The derived iterator type (CRTP pattern)
- `ValueType` - The type of values stored

**Methods:**
- `get_key()` - Returns the key at the current position
- `get_value()` - Returns the value at the current position
- `operator++()` - Increments the iterator to the next element
- `is_end()` - Checks if the iterator has reached the end

**Implementation Requirements:**
Derived classes must implement:
- `get_key_impl()` const
- `get_value_impl()` const
- `increment_impl()`
- `is_end_impl()` const

### 2. KeyStorage (CRTP Base Class)
A CRTP base class for key-value storage with string keys and templated value types.

**Template Parameters:**
- `Derived` - The derived storage type (CRTP pattern)
- `IteratorType` - The iterator type for this storage
- `ValueType` - The type of values stored

**Methods:**
- `get(const std::string& key, ValueType& value)` - Retrieves a value by key
- `put(const std::string& key, const ValueType& value)` - Stores a key-value pair
- `lower_bound(const std::string& key)` - Returns an iterator to the first element with key ≥ the given key

**Implementation Requirements:**
Derived classes must implement:
- `get_impl(const std::string& key, ValueType& value)` const
- `put_impl(const std::string& key, const ValueType& value)`
- `lower_bound_impl(const std::string& key)`

### 3. MapKeyStorage (Concrete Implementation)
A simple implementation using `std::map` as the underlying data structure.

**Features:**
- Uses `std::map<std::string, ValueType>` for storage
- Provides `MapKeyStorageIterator` for iteration
- Supports all operations from `KeyStorage`
- **Zero virtual function overhead!**

## Usage Examples

### Example 1: Integer Storage
```cpp
MapKeyStorage<int> storage;
storage.put("apple", 5);
storage.put("banana", 10);

int value;
if (storage.get("apple", value)) {
    std::cout << "Value: " << value << std::endl;
}
```

### Example 2: Pointer Storage
```cpp
MapKeyStorage<std::string*> ptrStorage;
std::string* str = new std::string("Hello");
ptrStorage.put("greeting", str);

std::string* retrieved;
if (ptrStorage.get("greeting", retrieved)) {
    std::cout << *retrieved << std::endl;
}
delete str;
```

### Example 3: Range Iteration with lower_bound
```cpp
MapKeyStorage<int> storage;
storage.put("apple", 1);
storage.put("banana", 2);
storage.put("cherry", 3);

auto it = storage.lower_bound("banana");
while (!it.is_end()) {
    std::cout << it.get_key() << " -> " << it.get_value() << std::endl;
    ++it;
}
```

## Supported Value Types

The `ValueType` template parameter is constrained by the `KeyStorageValueType` concept and must be:
- Integral types (int, long, unsigned int, etc.) - validated by `std::integral`
- Pointer types (T*) - validated by `std::is_pointer_v`

Invalid types will produce clear compile-time errors thanks to C++20 concepts!

**Example of concept enforcement:**
```cpp
MapKeyStorage<int> valid;      // ✓ Compiles - int is integral
MapKeyStorage<int*> alsoValid; // ✓ Compiles - int* is a pointer
MapKeyStorage<double> invalid; // ✗ Error: constraint KeyStorageValueType not satisfied
```

## Design Benefits

1. **Zero-Overhead Abstraction**: CRTP eliminates virtual function call overhead - all calls are resolved at compile time!
2. **No V-Table**: No virtual function pointer table, resulting in smaller binary size and better cache performance
3. **C++20 Concepts**: Strong compile-time type constraints with clear error messages
4. **Type Safety**: Template-based design with concepts ensures compile-time type checking
5. **Inlining Opportunities**: Compiler can inline all calls through the CRTP interface
6. **Iterator Pattern**: Custom iterator abstraction provides consistent traversal interface
7. **Extensibility**: New storage backends can be added by implementing the CRTP interface

## Performance Characteristics

Thanks to CRTP and C++20:
- **No runtime polymorphism overhead** - all dispatch is at compile time
- **Inline-friendly** - compilers can optimize across the abstraction boundary
- **Cache-friendly** - no indirection through vtables
- **Same performance as hand-written code** - zero-cost abstraction
- **Compile-time error checking** - concepts catch type errors before instantiation

## C++20 Concepts Used

### `KeyStorageValueType`
Ensures that value types are either integral or pointer types.

**Definition:**
```cpp
template<typename T>
concept KeyStorageValueType = std::integral<T> || std::is_pointer_v<T>;
```

This provides:
- Clear compile-time error messages when invalid types are used
- Self-documenting template constraints
- Better IDE autocomplete and error highlighting

## Future Implementations

Possible alternative implementations could include:
- B-Tree based storage
- Hash table storage
- Memory-mapped file storage
- Distributed storage backends

All implementations will have **zero virtual function overhead** thanks to CRTP!

