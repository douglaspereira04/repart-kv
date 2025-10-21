# Quick Start Guide

## Installation (Ubuntu/Debian)

```bash
# Install dependencies (including METIS for graph partitioning)
sudo apt-get update && sudo apt-get install -y \
    cmake build-essential pkg-config \
    libtkrzw-dev liblzma-dev liblz4-dev libzstd-dev \
    libmetis-dev libboost-dev

# Clone and build
cd repart-kv
./build.sh
```

## Running the Main Application

### Execute a Workload

```bash
cd build
./repart-kv <workload_file> [partition_count] [test_workers]
```

**Example:**
```bash
./repart-kv ../sample_workload.txt 8 4
```

This runs the workload with 8 partitions and 4 worker threads.

### Create a Workload File

Create a file `workload.txt` with operations (one per line):

```
1,key0000001          # WRITE operation
0,key0000001          # READ operation
2,key0000000,10       # SCAN operation (limit 10)
1,key0000002
0,key0000002
```

**Operation Format:**
- `0,<key>` - READ operation
- `1,<key>` - WRITE operation (uses 1KB default value)
- `2,<key>,<limit>` - SCAN operation with result limit

### View Metrics

After execution, check the generated `metrics.csv`:

```bash
cat metrics.csv
```

Output format:
```csv
elapsed_time_ms,executed_count,memory_kb,disk_kb
0,0,4084,45244
1002,11,4380,45244
```

## Running Examples and Tests

```bash
cd build

# Repartitioning tests (RECOMMENDED)
./test_repartitioning        # 5 repartitioning tests
./test_graph_tracking        # 8 graph tracking tests
./test_partitioned_kv_storage # 15 partitioned storage tests

# Interactive tester
./interactive_storage_test
./interactive_keystorage_test

# Storage engine tests
./test_storage_engine

# Examples
./example_storage
./example_tkrzw_storage
./example_tkrzw_tree_storage
./example_keystorage
./example_tkrzw_keystorage
```

## Quick Code Examples

### Dynamic Repartitioning Storage (Main Feature)

```cpp
#include "kvstorage/RepartitioningKeyValueStorage.h"
#include "storage/TkrzwHashStorageEngine.h"
#include "keystorage/TkrzwHashKeyStorage.h"

// Create partitioned storage with dynamic repartitioning
RepartitioningKeyValueStorage<
    TkrzwHashStorageEngine, 
    TkrzwHashKeyStorage, 
    TkrzwHashKeyStorage
> storage(4);  // 4 partitions

// Enable access pattern tracking
storage.enable_tracking(true);

// Perform operations
storage.write("user:1001", "Alice");
storage.write("user:1002", "Bob");
storage.read("user:1001");

// Repartition based on access patterns
storage.repartition();
```

### Automatic Background Repartitioning

```cpp
using namespace std::chrono_literals;

// Repartition every 60s, track for 10s before each repartition
RepartitioningKeyValueStorage<...> storage(
    4,                    // num_partitions
    std::hash<string>(),  // hash_func
    10s,                  // tracking_duration
    60s                   // repartition_interval
);

// Background thread automatically repartitions
// Your application continues normally
```

### Storage Engine (String → String)

```cpp
#include "storage/TkrzwHashStorageEngine.h"

TkrzwHashStorageEngine engine;
engine.write("user:1001", "Alice");
std::string name = engine.read("user:1001");
auto users = engine.scan("user:", 10);
```

### Key Storage (String → Generic Type)

```cpp
#include "keystorage/TkrzwHashKeyStorage.h"

TkrzwHashKeyStorage<int> counters;
counters.put("visits", 100);

int visits;
counters.get("visits", visits);

auto it = counters.lower_bound("counter:");
while (!it.is_end()) {
    std::cout << it.get_key() << " = " << it.get_value() << std::endl;
    ++it;
}
```

### Multi-threaded Workload Execution

```cpp
#include "workload/Workload.h"

// Parse workload file
auto operations = workload::read_workload("workload.txt");

// Create storage
RepartitioningKeyValueStorage<...> storage(8);

// Create worker threads (strided execution)
std::vector<std::thread> workers;
std::vector<size_t> counters(4, 0);

for (size_t worker_id = 0; worker_id < 4; worker_id++) {
    workers.emplace_back(worker_function, worker_id, 
                        std::ref(operations), 
                        std::ref(storage), 
                        std::ref(counters));
}

for (auto& w : workers) w.join();
```

### Thread-Safe Operations

```cpp
TkrzwHashStorageEngine engine;

// Manual locking for thread safety
engine.lock();
engine.write("shared_key", "value");
engine.unlock();

engine.lock_shared();
std::string val = engine.read("shared_key");
engine.unlock_shared();
```

## Choosing a Storage Backend

### Use TkrzwHashStorageEngine (Default) when:
- You need persistent storage
- Random access patterns dominate
- Write performance is critical
- Scan operations are rare

### Use TkrzwTreeStorageEngine when:
- You need persistent storage
- Range queries are common
- You need sorted key iteration
- Scan operations are frequent (15x faster!)

### Use MapStorageEngine when:
- You need simple in-memory storage
- You want sorted key iteration
- Persistence is not required
- You're prototyping or testing

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

1. **Use multiple worker threads** for parallel workload execution
2. **Monitor metrics.csv** to track performance over time
3. **Enable tracking before repartitioning** to capture access patterns
4. Use **TkrzwTreeStorageEngine** for scan-heavy workloads
5. Use **TkrzwHashStorageEngine** for write-heavy workloads
6. Use **per-worker counters** to avoid atomic contention
7. Call `sync()` on TKRZW engines to ensure data is persisted

## Understanding Repartitioning

### When to Repartition?
- After significant workload changes
- When access patterns stabilize
- Periodically with background thread
- After initial data loading

### How It Works:
1. **Track** - Records which keys are accessed together
2. **Partition** - METIS finds optimal key placement
3. **Migrate** - Data moves lazily to new partitions

### Benefits:
- Co-accessed keys end up in same partition
- Reduces cross-partition operations
- Improves scan performance
- Adapts to changing access patterns

## Troubleshooting

### Build fails with "tkrzw not found"
```bash
sudo apt-get install libtkrzw-dev
```

### Build fails with "cannot find -llzma"
```bash
sudo apt-get install liblzma-dev liblz4-dev libzstd-dev
```

### Build fails with "undefined reference to METIS_*"
```bash
sudo apt-get install libmetis-dev
```

### TKRZW database fails to open
Check file permissions and disk space:
```bash
ls -la *.tkh
df -h .
```

### Workload execution too slow
- Increase worker threads: `./repart-kv workload.txt 4 8`
- Check metrics.csv for bottlenecks
- Consider using TkrzwHashStorageEngine for better write performance

## Command Line Reference

### Main Executable
```bash
./repart-kv <workload_file> [partition_count] [test_workers]

# Examples:
./repart-kv workload.txt           # 4 partitions, 1 worker (default)
./repart-kv workload.txt 8         # 8 partitions, 1 worker
./repart-kv workload.txt 8 4       # 8 partitions, 4 workers
```

### Test Suite
```bash
./test_repartitioning              # Test repartitioning logic
./test_graph_tracking              # Test access pattern tracking
./test_partitioned_kv_storage      # Test partitioned storage
```

## Next Steps

1. **Read** [ARCHITECTURE.md](ARCHITECTURE.md) for design details and repartitioning algorithm
2. **Try** [storage/INTERACTIVE_USAGE.md](storage/INTERACTIVE_USAGE.md) for interactive testing
3. **See** [keystorage/README.md](keystorage/README.md) for KeyStorage details
4. **Check** [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md) for detailed installation
5. **Run** test suite to verify your installation

## Key Files

### Core Components
- `kvstorage/RepartitioningKeyValueStorage.h` - Dynamic repartitioning storage
- `graph/Graph.h` - Access pattern graph
- `graph/MetisGraph.h` - METIS partitioning interface
- `workload/Workload.h` - Workload file parser
- `main.cpp` - Multi-threaded workload executor

### Storage Backends
- `storage/StorageEngine.h` - CRTP base for storage engines
- `storage/TkrzwHashStorageEngine.h` - Default persistent storage
- `storage/TkrzwTreeStorageEngine.h` - Sorted persistent storage
- `keystorage/KeyStorage.h` - CRTP base for generic storage

### Build & Tests
- `CMakeLists.txt` - Build configuration (15+ targets)
- `build.sh` - Simple build script
- `kvstorage/test_*.cpp` - Test suites

## Sample Workload

Try the included sample workload:

```bash
cd build
./repart-kv ../sample_workload.txt 4 2

# View results
cat metrics.csv
```

Expected output:
```
=== Execution Complete ===
Operations executed: 10
Execution time: ~900 microseconds/op
Metrics saved to: metrics.csv
```

## Getting Help

- **Architecture questions?** → Read ARCHITECTURE.md
- **Installation issues?** → Check INSTALL_DEPENDENCIES.md
- **Performance tuning?** → See metrics.csv and adjust workers/partitions
- **API usage?** → Check header file comments (fully documented)
- **Can't find something?** → See INDEX.md for complete file listing

Happy partitioning! 🚀
