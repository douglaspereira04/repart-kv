# Architecture

This document describes the current technical structure of Repart-KV and how the main components fit together.

## High-level flow

1. `repart-kv-core` exposes `run_repart_kv(int argc, char *argv[])`, which parses CLI arguments (workload path, partitions, workers, storage type/engine, storage paths) and orchestrates workload execution.
2. (Optional) `main.cpp` is compiled into `repart-kv-runner`, a lightweight test/CLI utility that forwards `argc/argv` into `run_repart_kv`.
3. `workload/Workload.h` parses the workload file into operations (READ/WRITE/SCAN).
4. The selected storage stack executes operations using:
   - a `StorageEngine` backend (`storage/`)
   - optional partitioning/repartitioning logic (`kvstorage/`)
   - optional access-pattern tracking (`kvstorage/Tracker.h` + `graph/`)
5. The runner writes `metrics.csv` during execution.

## Key modules

### `storage/`: StorageEngine backends (String to String)

All storage engines implement the same CRTP-style interface exposed by `storage/StorageEngine.h`.

Implemented backends:

- **`MapStorageEngine`**: in-memory `std::map`
- **`TkrzwHashStorageEngine`**: TKRZW HashDBM (persistent)
- **`TkrzwTreeStorageEngine`**: TKRZW TreeDBM (persistent, ordered keys)
- **`LmdbStorageEngine`**: LMDB (persistent, ordered keys)
- **`TbbStorageEngine`**: in-memory `tbb::concurrent_hash_map`

Operational note:

- “scan” is implemented as a **lower_bound-style traversal** from a start key, limited to `N` results. It is not a strict prefix filter unless the underlying backend implements it that way.

### `keystorage/`: KeyStorage (String to integral/pointer)

KeyStorage is a separate abstraction used for internal maps and indexes where values are constrained by the `KeyStorageValueType` concept (integral types or pointers).

Core types:

- `keystorage/KeyStorage.h`: CRTP base API (`get`, `put`, `lower_bound`)
- `keystorage/KeyStorageIterator.h`: iterator API (`get_key`, `get_value`, `++`, `is_end`)
- `keystorage/KeyStorageConcepts.h`: type constraints

Implementations:

- `MapKeyStorage`
- `TkrzwHashKeyStorage`, `TkrzwTreeKeyStorage`
- `LmdbKeyStorage`
- `UnorderedDenseKeyStorage` (hash-based; builds sorted iteration by collecting and sorting keys)

### `graph/`: Access graph + METIS adapter

Tracking builds a weighted directed graph where:

- **Vertices** represent keys (weights accumulate access frequency).
- **Edges** represent co-access (weights accumulate co-occurrence).

Files:

- `graph/Graph.h`: graph structure (uses `ankerl::unordered_dense`)
- `graph/MetisGraph.h`: converts `Graph` to CSR format and partitions it using METIS

### `kvstorage/`: Partitioned and repartitioning storage

This layer coordinates:

- assigning keys to partitions
- executing operations against a per-partition storage engine
- (optionally) collecting access patterns and repartitioning

The `repart-kv` CLI selects the primary strategy via `storage_type`:

- **`soft`**: `SoftRepartitioningKeyValueStorage` (single logical storage with partition coordination)
- **`hard`**: `HardRepartitioningKeyValueStorage` (harder boundaries per partition / engine instances)
- **`threaded`**: `SoftThreadedRepartitioningKeyValueStorage` (threaded variant)
- **`hard_threaded`**: `HardThreadedRepartitioningKeyValueStorage` (threaded + hard repartitioning)
- **`engine`**: direct `StorageEngine` usage (no repartitioning)

Supporting components:

- `kvstorage/Tracker.h`: access tracking and queueing
- `kvstorage/threaded/`: worker infrastructure and operation types

## Concurrency model (current)

Repart-KV mixes several concurrency mechanisms, chosen per component:

- **Worker execution**: configurable worker count in the runner.
- **Threaded partitioning** (`kvstorage/threaded/`):
  - `boost::lockfree::spsc_queue` is used for some producer/consumer paths.
  - TBB queues/containers are used where appropriate (`tbb::concurrent_*`).
- **Backend thread safety**: depends on the selected `StorageEngine` and storage strategy.

## Build and target selection

`CMakeLists.txt` defines the core `repart-kv-core` library and an opt-in `repart-kv-runner` executable. Most storage/tests build by default; `repart-kv-runner` is enabled via `BUILD_REPART_KV_RUNNER=ON` or `build.sh -t`. Optional dependencies gate specific graph/test targets (METIS, LMDB, TBB, etc.).

## Extending: adding a StorageEngine backend

Repart-KV can be extended to support additional embedded databases or storage engines. Integration requires two parts:

1. Implement a new `StorageEngine` backend (compile-time polymorphism via CRTP).
2. Register it with the build and the `repart-kv` CLI so it can be selected.

### 1) Implement the backend

Create a new header in `storage/` (for example: `storage/MyDbStorageEngine.h`) and implement the interface from `storage/StorageEngine.h`:

- Implement the required methods:
  - `Status read_impl(const std::string& key, std::string& value)`
  - `Status write_impl(const std::string& key, const std::string& value)`
  - `Status scan_impl(const std::string& key_start, size_t limit, std::vector<std::pair<std::string, std::string>>& out)`
- Respect the locking contract:
  - `read()`, `write()`, and `scan()` **do not lock automatically**. Callers lock manually using `lock()` / `lock_shared()` on the engine when required.
- Match the scan semantics:
  - `scan` is treated as **lower_bound-style**: return keys >= `key_start`, up to `limit`.
- If your backend needs a filesystem location for its files:
  - use the base class `path()` (populated from the CLI `storage_paths` argument).

### 2) Wire it into the build

Update `CMakeLists.txt` to ensure your engine compiles and links:

- Add include paths if needed.
- Link your database library (for example via `find_package(...)` or `pkg_check_modules(...)` and `target_link_libraries(...)`).

If the dependency is optional, follow the pattern used for METIS/TBB/LMDB: detect it, then build/enable the relevant targets conditionally.

### 3) Expose it via the CLI

Update `main.cpp` so `repart-kv` can select the backend:

- Add a new accepted value to the `storage_engine` argument validation.
- Add a new branch that maps that value to your engine type (similar to `tkrzw_tree`, `tkrzw_hash`, `lmdb`, `map`, `tbb`).
- Update `print_usage()` so the new engine appears in `--help` output.

### 4) Document and test

- Update [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md) with any new packages required to build/link your engine.
- Consider adding coverage to `storage/test/test_storage_engine.cpp` if the backend should participate in the generic engine test suite.

See:

- [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md)
- [INDEX.md](INDEX.md)

