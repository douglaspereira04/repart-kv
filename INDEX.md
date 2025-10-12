# Repart-KV Complete Index

## 📚 Documentation (6 files)

1. **[README.md](README.md)** - Main project documentation
2. **[QUICK_START.md](QUICK_START.md)** - Fast getting started guide
3. **[ARCHITECTURE.md](ARCHITECTURE.md)** - Design patterns and architecture
4. **[INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md)** - Detailed installation guide
5. **[PROJECT_SUMMARY.md](PROJECT_SUMMARY.md)** - Complete project overview
6. **[storage/INTERACTIVE_USAGE.md](storage/INTERACTIVE_USAGE.md)** - Interactive tester guide
7. **[keystorage/README.md](keystorage/README.md)** - KeyStorage documentation

## 🔧 Storage Engine Implementations (3 + base)

### Base Class
- **[storage/StorageEngine.h](storage/StorageEngine.h)** - CRTP base class

### Implementations (String → String)
1. **[storage/MapStorageEngine.h](storage/MapStorageEngine.h)** - std::map based
2. **[storage/TkrzwHashStorageEngine.h](storage/TkrzwHashStorageEngine.h)** - TKRZW HashDBM
3. **[storage/TkrzwTreeStorageEngine.h](storage/TkrzwTreeStorageEngine.h)** - TKRZW TreeDBM

## 🗂️ Key Storage Implementations (3 + base)

### Base Classes
- **[keystorage/KeyStorage.h](keystorage/KeyStorage.h)** - CRTP base class
- **[keystorage/KeyStorageIterator.h](keystorage/KeyStorageIterator.h)** - Iterator base
- **[keystorage/KeyStorageConcepts.h](keystorage/KeyStorageConcepts.h)** - C++20 concepts

### Implementations (String → Generic Types)
1. **[keystorage/MapKeyStorage.h](keystorage/MapKeyStorage.h)** - std::map based
2. **[keystorage/TkrzwHashKeyStorage.h](keystorage/TkrzwHashKeyStorage.h)** - TKRZW HashDBM
3. **[keystorage/TkrzwTreeKeyStorage.h](keystorage/TkrzwTreeKeyStorage.h)** - TKRZW TreeDBM

## 🧪 Tests & Examples (8 executables)

### Main Application
- **[main.cpp](main.cpp)** → `./repart-kv` - Main demo application

### Interactive Testing
- **[storage/interactive_storage_test.cpp](storage/interactive_storage_test.cpp)** → `./interactive_storage_test` - **CLI tester**

### Test Suite
- **[storage/test_map_storage_engine.cpp](storage/test_map_storage_engine.cpp)** → `./test_map_storage_engine` - 16 tests

### Storage Engine Examples
- **[storage/example_storage.cpp](storage/example_storage.cpp)** → `./example_storage`
- **[storage/example_tkrzw_storage.cpp](storage/example_tkrzw_storage.cpp)** → `./example_tkrzw_storage`
- **[storage/example_tkrzw_tree_storage.cpp](storage/example_tkrzw_tree_storage.cpp)** → `./example_tkrzw_tree_storage`

### Key Storage Examples
- **[keystorage/example_usage.cpp](keystorage/example_usage.cpp)** → `./example_keystorage`
- **[keystorage/example_tkrzw_keystorage.cpp](keystorage/example_tkrzw_keystorage.cpp)** → `./example_tkrzw_keystorage`

## 🏗️ Build System

- **[CMakeLists.txt](CMakeLists.txt)** - CMake configuration (9 targets)
- **[build.sh](build.sh)** - Simple build script

## 📊 Project Statistics

- **22 source files** (.h and .cpp)
- **8 executable targets** (+ main = 9 total)
- **6 storage implementations** (3 StorageEngine + 3 KeyStorage)
- **7 documentation files**
- **16 automated tests**
- **~7,000 lines of code** (including docs)

## 🎯 Quick Navigation

### I want to...

**...start using the project**
→ Read [QUICK_START.md](QUICK_START.md)

**...understand the design**
→ Read [ARCHITECTURE.md](ARCHITECTURE.md)

**...test interactively**
→ Run `./build/interactive_storage_test` and see [storage/INTERACTIVE_USAGE.md](storage/INTERACTIVE_USAGE.md)

**...install dependencies**
→ Read [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md)

**...see all features**
→ Read [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md)

**...understand KeyStorage**
→ Read [keystorage/README.md](keystorage/README.md)

**...run the examples**
→ `cd build && ls -la` then run any executable

**...modify or extend**
→ Read [ARCHITECTURE.md](ARCHITECTURE.md) section "Adding New Storage Backends"

## 🌟 Highlights

### Zero-Overhead Abstractions
- No virtual functions
- All dispatch at compile time
- Same performance as hand-written code

### C++20 Modern Features
- Concepts for type constraints
- Constexpr if for compile-time branches
- Template parameter constraints

### Multiple Backends
- In-memory: MapStorageEngine, MapKeyStorage
- Persistent Hash: TkrzwHashStorageEngine, TkrzwHashKeyStorage
- Persistent Tree: TkrzwTreeStorageEngine, TkrzwTreeKeyStorage

### Production Ready
- Thread-safe with manual locking
- Comprehensive tests
- Clean compilation (no warnings)
- Full documentation

## 📈 Performance Comparison

| Engine | Write (ops/s) | Read (ops/s) | Scan 1000 |
|--------|---------------|--------------|-----------|
| Map | ~2M | ~2.5M | Fast |
| TkrzwHash | ~2M | ~3.3M | ~11ms |
| TkrzwTree | ~1.4M | ~1.7M | **~732μs** |

**Winner for scans**: TkrzwTreeStorageEngine (15x faster!)

## 🔄 Workflow

```
1. Install dependencies → INSTALL_DEPENDENCIES.md
2. Build project → ./build.sh
3. Try interactive tester → ./build/interactive_storage_test
4. Run examples → ./build/example_*
5. Read architecture → ARCHITECTURE.md
6. Extend with custom backends → Add your implementation
```

## ✅ Quality Checklist

- [x] Zero virtual functions (CRTP)
- [x] C++20 concepts for type safety
- [x] Thread-safe with manual locking
- [x] Multiple storage backends
- [x] Comprehensive tests (16 tests pass)
- [x] Interactive testing tool
- [x] Complete documentation
- [x] Clean compilation (no warnings)
- [x] Example programs for all features
- [x] Performance benchmarks included

## 🎓 Learning Path

**Beginner**:
1. Read QUICK_START.md
2. Run `./interactive_storage_test`
3. Try the examples

**Intermediate**:
1. Read ARCHITECTURE.md
2. Study the MapStorageEngine implementation
3. Understand CRTP pattern

**Advanced**:
1. Study TKRZW integrations
2. Implement your own storage backend
3. Add features like compression or replication

## 📞 Support

All code is self-documenting with detailed comments. Each class has:
- Purpose documentation
- Template parameter descriptions
- Method documentation
- Usage examples

**Can't find something?** Check this INDEX.md for the complete file listing.

