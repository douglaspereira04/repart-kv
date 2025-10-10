# Repart-KV

A high-performance key-value storage system built with C++20, featuring multiple storage backends with CRTP (Curiously Recurring Template Pattern) for zero-overhead abstractions.

## Features

- 🚀 **Zero-overhead abstractions** using CRTP (no virtual functions!)
- 🔒 **Thread-safe operations** with manual locking primitives
- 🎯 **C++20 concepts** for compile-time type safety
- 📦 **Multiple storage backends**:
  - `MapStorageEngine` - In-memory storage using `std::map`
  - `TkrzwHashStorageEngine` - High-performance hash-based storage using TKRZW HashDBM
  - `TkrzwTreeStorageEngine` - Sorted key-value storage using TKRZW TreeDBM
- 🗂️ **Generic key-storage interface** with iterators
- ✅ **Comprehensive test suite**

## Prerequisites

### Required
- CMake 3.20 or higher
- A C++20 compatible compiler (GCC 10+, Clang 10+, or MSVC 2019+)
- Make or Ninja build system
- **libtkrzw-dev** - TKRZW database library

### Quick Install (Ubuntu/Debian)

```bash
sudo apt-get update && sudo apt-get install -y \
    cmake build-essential pkg-config \
    libtkrzw-dev liblzma-dev liblz4-dev libzstd-dev
```

**⚠️ Important:** The compression libraries (`liblzma-dev`, `liblz4-dev`, `libzstd-dev`) are **required**. TKRZW depends on them.

For detailed installation instructions for other platforms, see [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md).

## Building

### Option 1: Using the build script (Recommended)

```bash
./build.sh
```

This script will:
- Create a `build` directory
- Configure the project with CMake
- Build the project using all available CPU cores

### Option 2: Manual build

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)
```

### Build Types

By default, the project builds in Release mode. To build in Debug mode:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

## Running

After building, execute the main program:

```bash
cd build
./repart-kv
```

### Available Executables

- `./repart-kv` - Main demo showcasing storage engines
- `./test_map_storage_engine` - Comprehensive test suite (16 tests)
- `./example_storage` - MapStorageEngine examples
- `./example_tkrzw_storage` - TkrzwHashStorageEngine examples (hash-based)
- `./example_tkrzw_tree_storage` - TkrzwTreeStorageEngine examples (sorted tree-based)
- `./example_keystorage` - KeyStorage examples with concepts

## Project Structure

```
repart-kv/
├── CMakeLists.txt           # CMake configuration
├── main.cpp                 # Main demo application
├── build.sh                 # Build script
├── README.md               # This file
├── storage/                # Storage engine implementations
│   ├── StorageEngine.h                # CRTP base class
│   ├── StorageEngineConcepts.h        # C++20 concepts
│   ├── MapStorageEngine.h             # std::map implementation
│   ├── TkrzwHashStorageEngine.h       # TKRZW HashDBM implementation
│   ├── TkrzwTreeStorageEngine.h       # TKRZW TreeDBM implementation
│   ├── test_map_storage_engine.cpp    # Test suite
│   ├── example_storage.cpp            # MapStorageEngine examples
│   ├── example_tkrzw_storage.cpp      # TkrzwHashStorageEngine examples
│   └── example_tkrzw_tree_storage.cpp # TkrzwTreeStorageEngine examples
├── keystorage/             # Generic key-value storage
│   ├── KeyStorage.h               # CRTP base class
│   ├── KeyStorageIterator.h       # Iterator base class
│   ├── KeyStorageConcepts.h       # C++20 concepts
│   ├── MapKeyStorage.h            # std::map implementation
│   ├── example_usage.cpp          # Examples
│   └── README.md                  # KeyStorage documentation
├── table/                  # Partitioned table layer
│   └── RepartitionTable.h
└── build/                  # Build directory (created after building)
```

## Architecture

### Storage Engine (CRTP-based)

The `StorageEngine` class uses the Curiously Recurring Template Pattern for compile-time polymorphism without virtual functions:

```cpp
template<typename Derived>
class StorageEngine {
public:
    std::string read(const std::string& key) const;
    void write(const std::string& key, const std::string& value);
    std::vector<std::string> scan(const std::string& prefix, size_t limit) const;
    // Manual locking primitives
    void lock() const;
    void unlock() const;
    void lock_shared() const;
    void unlock_shared() const;
};
```

### Key Storage (Generic with Concepts)

Generic key-value storage with C++20 concepts for integral and pointer types:

```cpp
template<KeyStorageValueType ValueType>
class MapKeyStorage : public KeyStorage<...> {
    bool get(const std::string& key, ValueType& value) const;
    void put(const std::string& key, const ValueType& value);
    Iterator lower_bound(const std::string& key);
};
```

## Development

The project is configured with:
- **C++20 standard** with concepts and CRTP
- Compiler warnings (`-Wall -Wextra -Wpedantic`)
- Optimized builds in Release mode (`-O3`)
- Debug symbols in Debug mode (`-g -O0`)
- Zero virtual function overhead

## Design Principles

1. **Zero-cost abstractions** - CRTP eliminates runtime polymorphism overhead
2. **No virtual functions** - All dispatch happens at compile time
3. **Type safety** - C++20 concepts enforce constraints at compile time
4. **Manual locking** - Users control thread-safety granularity
5. **Modern C++** - Leverages C++20 features for better performance and safety

## Clean Build

To clean and rebuild from scratch:

```bash
rm -rf build
./build.sh
```
