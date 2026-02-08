# Repart-KV

Repart-KV is a C++20 project that executes key-value workloads against a partitioned storage layer. It supports multiple storage backends and can repartition based on observed access patterns using METIS.

## Documentation

- **Getting started**: [QUICK_START.md](QUICK_START.md)
- **Dependencies**: [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md)
- **Architecture**: [ARCHITECTURE.md](ARCHITECTURE.md)
- **Index**: [INDEX.md](INDEX.md)
- **Project summary**: [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md)

## Build

### Using the build script

`build.sh` configures CMake, builds the core library + tests, and runs `run_tests.sh` by default. Pass `-t`/`--runner` to also build the optional CLI/test utility (`repart-kv-runner`).

```bash
./build.sh           # builds repart-kv-core + tests
./build.sh -t        # also builds repart-kv-runner
```

### Manual build

```bash
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_REPART_KV_RUNNER=ON
cmake --build build -j"$(nproc)"
```

## Run workloads

The CLI runner is optional (`repart-kv-runner`). Either build it via `./build.sh -t` or set `-DBUILD_REPART_KV_RUNNER=ON` when configuring CMake.

```bash
cd build
./repart-kv-runner [workload_files] [partition_count] [test_workers] [storage_type] [storage_engine] [storage_paths] [repartition_interval_ms]
```

### Arguments

- **workload_files**: comma-separated paths to workload files (one per worker thread). The number of files must match `test_workers`.
- **partition_count**: number of partitions (default: `4`)
- **test_workers**: worker threads for workload execution (default: `1`)
- **storage_type**: `hard`, `soft`, `threaded`, `hard_threaded`, or `engine` (default: `soft`)
- **storage_engine**: `tkrzw_tree`, `tkrzw_hash`, `lmdb`, `map`, or `tbb` (default: `tkrzw_tree`)
- **storage_paths**: comma-separated directories used for embedded DB files (default: `/tmp`)
- **repartition_interval_ms**: interval in milliseconds between repartitioning cycles and tracking duration (default: `1000`). Sets both `TRACKING_DURATION` and `REPARTITION_INTERVAL` to this value.

### Examples

```bash
# Defaults: 4 partitions, 1 worker, soft + tkrzw_tree, /tmp
./repart-kv-runner ../sample_workload.txt

# 8 partitions, 4 workers, threaded storage, tbb backend (in-memory)
./repart-kv-runner ../sample_workload.txt 8 4 threaded tbb

# Use two storage directories for partition files (embedded backends only)
./repart-kv-runner ../sample_workload.txt 8 4 soft tkrzw_tree 0 /mnt/d1,/mnt/d2
```

## Workload format

Workload files are CSV-style, one operation per line:

- **READ**: `0,<key>`
- **WRITE**: `1,<key>` (writes a default 1KB value)
- **SCAN**: `2,<start_key>,<limit>` (lower_bound-style scan starting at `start_key`)

Example:

```text
1,user:1001
1,user:1002
0,user:1001
2,user:,10
```

## Output

The executor writes `metrics.csv` (time series):

```csv
elapsed_time_ms,executed_count,memory_kb,disk_kb
0,0,4084,45244
1002,11,4380,45244
```

## Linking against the library

`repart-kv-core` is a static library that exports the entry point `run_repart_kv(argc, argv)` declared in `src/repart_kv_api.h`. Link it into your own binary, pass command-line arguments programmatically, and reuse the workload executor without depending on the optional runner.

Use the core library target from this repository:

```cmake
add_subdirectory(/path/to/repart-kv)
target_link_libraries(my_binary PRIVATE repart-kv-core)
target_include_directories(my_binary PRIVATE ${CMAKE_SOURCE_DIR}/src)
```

`run_repart_kv(argc, argv)` mirrors the CLI argument validation described above.

## Tests

Build and run all tests (runner build flag is unrelated):

```bash
./build.sh
```

Or run selected tests from `build/`:

```bash
cd build
./test_storage_engine
./test_keystorage
./test_partitioned_kv_storage
./test_graph_tracking
./test_repartitioning_storage
```

## Interactive tools

```bash
cd build
./interactive_storage_test
./interactive_keystorage_test
```

See:

- [storage/INTERACTIVE_USAGE.md](storage/INTERACTIVE_USAGE.md)
- [keystorage/INTERACTIVE_USAGE.md](keystorage/INTERACTIVE_USAGE.md)

## Dependencies (summary)

See [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md) for full instructions.

- **Required**: CMake ≥ 3.20, C++20 compiler, `pkg-config`, TKRZW, METIS
- **Used for specific backends/features**: LMDB (`lmdb`), Intel TBB (`tbb`), Boost Lockfree (threaded workers)
- **Build script convenience**: `clang-format` (used by `build.sh`)

## Extending: adding a new storage engine backend

Repart-KV is designed to be extensible: **you can integrate another embedded database or storage engine by implementing the `StorageEngine` interface** (see `storage/StorageEngine.h`) and then wiring it into the build and CLI.

For the concrete steps, see **“Extending: adding a StorageEngine backend”** in [ARCHITECTURE.md](ARCHITECTURE.md).

## Repository layout

```text
repart-kv/
  graph/        Graph + METIS adapter
  keystorage/   KeyStorage abstractions and implementations
  kvstorage/    Partitioned and (re)partitioning storage layers
  storage/      StorageEngine backends
  workload/     Workload parser and operation types
  utils/        Shared test utilities
  main.cpp      Workload executor (CLI)
  build.sh      Configure/build/test script
  run_tests.sh  Test runner for build artifacts
```
