# Repart-KV

A high-performance partitioned key-value storage system with automatic repartitioning built with C++20, featuring dynamic graph-based partitioning using METIS and CRTP (Curiously Recurring Template Pattern) for zero-overhead abstractions.

**📖 Quick Navigation**: [Quick Start](QUICK_START.md) | [Architecture](ARCHITECTURE.md) | [Installation](INSTALL_DEPENDENCIES.md) | [Complete Index](INDEX.md) | [Project Summary](PROJECT_SUMMARY.md)

## Features

- 🔄 **Dynamic Repartitioning** - Automatic graph-based repartitioning using METIS
- 🔀 **Soft Repartitioning** - Non-disruptive repartitioning that preserves existing data access
- 🧵 **Multi-threaded Workload Execution** - Parallel operation processing with configurable worker threads
- 📊 **Real-time Metrics** - CSV-based metrics logging (operations/sec, memory, disk usage)
- 🚀 **Zero-overhead abstractions** using CRTP (no virtual functions!)
- 🔒 **Thread-safe operations** with fine-grained locking primitives
- 🎯 **C++20 concepts** for compile-time type safety
- ⚡ **High-performance containers** - Uses unordered_dense for optimal hash map performance
- 📦 **Multiple storage backends**:
  - `MapStorageEngine` - In-memory storage using `std::map`
  - `TkrzwHashStorageEngine` - High-performance hash-based storage using TKRZW HashDBM (default)
  - `TkrzwTreeStorageEngine` - Sorted key-value storage using TKRZW TreeDBM
- 🗂️ **Generic key-storage interface** with iterators
- 📈 **Access Pattern Tracking** - Graph-based tracking of key access patterns
- ✅ **Comprehensive test suite**

## Prerequisites

### Required
- CMake 3.20 or higher
- A C++20 compatible compiler (GCC 10+, Clang 10+, or MSVC 2019+)
- Make or Ninja build system
- **libtkrzw-dev** - TKRZW database library
- **libmetis-dev** - METIS graph partitioning library
- **libboost-dev (>= 1.83)** - Boost Lockfree SPSC queue (system-wide)
- **unordered_dense** - High-performance hash map library (automatically fetched via CMake)

### Quick Install (Ubuntu/Debian)

```bash
sudo apt-get update && sudo apt-get install -y \
    cmake build-essential pkg-config \
    libtkrzw-dev liblzma-dev liblz4-dev libzstd-dev \
    libmetis-dev libboost-dev
```

**⚠️ Important:** The compression libraries (`liblzma-dev`, `liblz4-dev`, `libzstd-dev`) and METIS are **required**.

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

## Running

### Workload Execution

The main executable processes workload files with multi-threaded execution:

```bash
cd build
./repart-kv <workload_file> [partition_count] [test_workers]
```

**Arguments:**
- `workload_file` - Path to the workload file (required)
- `partition_count` - Number of partitions (default: 4)
- `test_workers` - Number of worker threads (default: 1)

**Example:**
```bash
./repart-kv ../sample_workload.txt 8 4
```

### Workload File Format

Each line contains one operation:
- `0,<key>` - READ operation
- `1,<key>` - WRITE operation (uses 1KB default value)
- `2,<key>,<limit>` - SCAN operation with limit

**Example workload file:**
```
1,0000000105
2,0000000066,38
2,0000000001,16
1,0000000009
0,0000000008
1,0000000002
```

### Metrics Output

The system automatically generates `metrics.csv` with real-time performance data:
```csv
elapsed_time_ms,executed_count,memory_kb,disk_kb
0,0,4084,45244
1002,11,4380,45244
2004,21,4380,45244
```

### Available Executables

- `./repart-kv` - **Main workload executor with soft repartitioning**
- `./interactive_storage_test` - Interactive CLI for testing storage engines
- `./test_graph_tracking` - Graph tracking tests (8 tests)
- `./test_repartitioning` - Repartitioning tests (5 tests)
- `./test_soft_repartitioning` - Soft repartitioning tests (7 tests)
- `./test_partitioned_kv_storage` - Partitioned storage tests (15 tests)
- `./test_storage_engine` - Storage engine tests
- `./example_storage` - MapStorageEngine examples
- `./example_tkrzw_storage` - TkrzwHashStorageEngine examples
- `./example_keystorage` - MapKeyStorage examples

## Project Structure

```
repart-kv/
├── CMakeLists.txt           # CMake configuration
├── main.cpp                 # Main workload executor
├── build.sh                 # Build script
├── README.md               # This file
├── storage/                # Storage engine implementations
│   ├── StorageEngine.h                # CRTP base class
│   ├── MapStorageEngine.h             # std::map implementation
│   ├── TkrzwHashStorageEngine.h       # TKRZW HashDBM implementation
│   └── TkrzwTreeStorageEngine.h       # TKRZW TreeDBM implementation
├── keystorage/             # Generic key-value storage
│   ├── KeyStorage.h                  # CRTP base class
│   ├── MapKeyStorage.h               # std::map implementation
│   ├── TkrzwHashKeyStorage.h         # TKRZW HashDBM implementation
│   └── TkrzwTreeKeyStorage.h         # TKRZW TreeDBM implementation
├── kvstorage/             # Partitioned key-value storage layer
│   ├── PartitionedKeyValueStorage.h        # CRTP base for partitioned storage
│   ├── RepartitioningKeyValueStorage.h     # Dynamic repartitioning storage
│   ├── SoftRepartitioningKeyValueStorage.h # Non-disruptive repartitioning storage
│   ├── test_graph_tracking.cpp             # Graph tracking tests
│   ├── test_repartitioning.cpp             # Repartitioning tests
│   ├── test_soft_repartitioning.cpp        # Soft repartitioning tests
│   └── test_partitioned_kv_storage.cpp     # Partitioned storage tests
├── graph/                  # Graph structures for partitioning
│   ├── Graph.h                     # Access pattern graph
│   ├── MetisGraph.h                # METIS graph interface
│   └── test_graph.cpp              # Graph tests
├── workload/              # Workload file parsing
│   └── Workload.h                  # Workload parser and operations
└── build/                  # Build directory (created after building)
```

## Architecture Highlights

### Dynamic Repartitioning

The system provides two repartitioning strategies:

**RepartitioningKeyValueStorage**: Traditional repartitioning that creates new storage engines and migrates data.

**SoftRepartitioningKeyValueStorage**: Non-disruptive repartitioning that preserves existing data access while optimizing partition assignments:

1. **Access Pattern Tracking** - Tracks which keys are accessed together
2. **Graph Construction** - Builds a graph where vertices are keys and edges represent co-access
3. **METIS Partitioning** - Uses METIS to partition the graph optimally
4. **Lazy Migration** - Data migrates to new partitions on-demand

### Multi-threaded Execution

Worker threads process operations in a strided pattern:
- Worker 0: operations[0], operations[TEST_WORKERS], operations[2*TEST_WORKERS], ...
- Worker 1: operations[1], operations[1+TEST_WORKERS], operations[1+2*TEST_WORKERS], ...

This ensures no operation overlap between workers while maintaining good load distribution.

### Thread Safety

- **Graph Lock** - Protects graph updates and repartitioning operations
- **Per-worker Counters** - Eliminates atomic contention
- **Storage Locking** - Fine-grained locking for storage operations

## Performance

With Tkrzw storage backend:
- **Single-threaded**: ~900-3700 microseconds/operation (persistent storage)
- **Multi-threaded**: Linear scaling up to CPU core count
- **Metrics overhead**: Minimal (CSV write every second)

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
6. **Graph-based optimization** - Access patterns drive data placement

## Clean Build

To clean and rebuild from scratch:

```bash
rm -rf build
./build.sh
```

## Documentation

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Detailed architecture and design patterns
- **[INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md)** - Dependency installation guide
- **[INDEX.md](INDEX.md)** - Complete project index
- **[QUICK_START.md](QUICK_START.md)** - Quick start guide

## Testing

Run the comprehensive test suite:

```bash
cd build
./test_graph_tracking        # Graph tracking tests
./test_repartitioning        # Repartitioning tests
./test_soft_repartitioning   # Soft repartitioning tests
./test_partitioned_kv_storage # Partitioned storage tests
./test_storage_engine        # Storage engine tests
```

All tests pass with detailed output showing each test case.
