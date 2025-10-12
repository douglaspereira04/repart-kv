# Quick Start Guide

## Installation (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update && sudo apt-get install -y \
    cmake build-essential pkg-config \
    libtkrzw-dev liblzma-dev liblz4-dev libzstd-dev

# Clone and build
cd repart-kv
./build.sh
```

## Running Examples

```bash
cd build

# Main demo
./repart-kv

# Interactive tester (RECOMMENDED)
./interactive_storage_test

# Run tests
./test_map_storage_engine

# Other examples
./example_storage
./example_tkrzw_storage
./example_tkrzw_tree_storage
./example_keystorage
./example_tkrzw_keystorage
```

## Quick Code Examples

### Storage Engine (String → String)

```cpp
#include "storage/MapStorageEngine.h"

MapStorageEngine engine;
engine.write("user:1001", "Alice");
std::string name = engine.read("user:1001");
auto users = engine.scan("user:", 10);
```

### Key Storage (String → Generic Type)

```cpp
#include "keystorage/MapKeyStorage.h"

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

### Persistent Storage with TKRZW

```cpp
#include "storage/TkrzwTreeStorageEngine.h"

TkrzwTreeStorageEngine engine("/tmp/mydb.tkt");
engine.write("key", "value");
engine.sync();  // Persist to disk
```

### Thread-Safe Operations

```cpp
MapStorageEngine engine;

// Manual locking for thread safety
engine.lock();
engine.write("shared_key", "value");
engine.unlock();

engine.lock_shared();
std::string val = engine.read("shared_key");
engine.unlock_shared();
```

## Choosing a Storage Engine

### Use MapStorageEngine when:
- You need simple in-memory storage
- You want sorted key iteration
- Persistence is not required
- You're prototyping or testing

### Use TkrzwHashStorageEngine when:
- You need persistent storage
- Random access patterns dominate
- Write performance is critical
- Scan operations are rare

### Use TkrzwTreeStorageEngine when:
- You need persistent storage
- Range queries are common
- You need sorted key iteration
- Scan operations are frequent
- Prefix-based queries are important

## Interactive Testing Quick Reference

```bash
./interactive_storage_test
```

**Commands**:
```
write("key", "value")  → OK
get("key")             → value
scan("prefix", 10)     → matching keys
help                   → show help
exit                   → quit
```

## Performance Tips

1. **Batch writes** under a single lock to reduce overhead
2. Use **TkrzwTreeStorageEngine** for scan-heavy workloads
3. Use **TkrzwHashStorageEngine** for write-heavy workloads
4. Use **MapStorageEngine** for small datasets or prototyping
5. Call `sync()` on TKRZW engines to ensure data is persisted

## Troubleshooting

### Build fails with "tkrzw not found"
```bash
sudo apt-get install libtkrzw-dev
```

### Build fails with "cannot find -llzma"
```bash
sudo apt-get install liblzma-dev liblz4-dev libzstd-dev
```

### TKRZW database fails to open
Check file permissions and disk space:
```bash
ls -la /tmp/tkrzw_*.tkh
df -h /tmp
```

## Next Steps

1. Read [ARCHITECTURE.md](ARCHITECTURE.md) for design details
2. Try [storage/INTERACTIVE_USAGE.md](storage/INTERACTIVE_USAGE.md) for interactive testing
3. See [keystorage/README.md](keystorage/README.md) for KeyStorage details
4. Check [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md) for detailed installation

## Key Files

- `storage/StorageEngine.h` - CRTP base for storage engines
- `keystorage/KeyStorage.h` - CRTP base for generic storage
- `storage/interactive_storage_test.cpp` - Interactive CLI tester
- `CMakeLists.txt` - Build configuration

