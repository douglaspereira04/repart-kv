# Architecture

This document describes the current technical structure of Repart-KV and how the main components fit together.

## High-level flow

1. `repart-kv-core` exposes `run_repart_kv(int argc, char *argv[])` in `src/repart_kv.cpp`. It parses a LoadGen TOML path plus optional CLI arguments (partitions, workers, storage type/engine, thinking time, storage paths, intervals, duration, sync) and orchestrates execution.
2. (Optional) `main.cpp` is compiled into `repart-kv-runner`, a lightweight CLI that forwards `argc/argv` into `run_repart_kv`.
3. A LoadGen `workload::RequestGenerator` is built from the TOML file. It yields a **LOADING** phase (`n_records`) then an **OPERATIONS** phase (`n_operations` per worker).
4. The selected storage stack executes operations using:
   - a `StorageEngine` backend (`storage/`)
   - optional partitioning/repartitioning logic (`kvstorage/`)
   - optional access-pattern tracking (`kvstorage/Tracker.h` + `graph/`)
5. The runner writes a parameter-encoded time-series CSV and a matching `__latency.csv`.

## Key modules

### `storage/`: StorageEngine backends (String to String)

All storage engines implement the same CRTP-style interface exposed by `storage/StorageEngine.h`. Engines are parameterized by a `SYNC` flag (`StorageEngine<Derived, SYNC>`): when `true`, disk-backed engines use durable writes; in-memory engines ignore it.

Implemented backends:

- **`MapStorageEngine`**: in-memory `std::map`
- **`AbslBtreeStorageEngine`**: in-memory `absl::btree_map` (used in tests; not a CLI `storage_engine` value)
- **`TkrzwHashStorageEngine`**: TKRZW HashDBM (persistent)
- **`TkrzwTreeStorageEngine`**: TKRZW TreeDBM (persistent, ordered keys)
- **`LmdbStorageEngine`**: LMDB (persistent, ordered keys)
- **`LmdbNolockStorageEngine`**: LMDB with `MDB_NOLOCK` (caller-owned sync)
- **`LevelDBStorageEngine`**: LevelDB (persistent, ordered keys, LSM-tree)
- **`RocksDBStorageEngine`**: RocksDB (persistent, ordered keys, LSM-tree)
- **`TbbStorageEngine`**: in-memory `tbb::concurrent_hash_map`

Operational notes:

- “scan” is a **lower_bound-style traversal** from a start key, limited to `N` results. It is not a strict prefix filter unless the underlying backend implements it that way.
- `read()`, `write()`, `scan()`, `async_write()`, and `remove()` **do not lock automatically**. Callers lock with `lock()` / `lock_shared()` when required.

### `keystorage/`: KeyStorage (String to trivially copyable values)

KeyStorage is a separate abstraction used for internal maps and indexes. Values must satisfy `KeyStorageValueType`: trivially copyable and default constructible (scalars, pointers, or small POD structs; see `keystorage/KeyStorageValueBinary.h`).

Core types:

- `keystorage/KeyStorage.h`: CRTP base API (`get`, `put`, `scan`, `get_or_insert`, `lower_bound`)
- `keystorage/KeyStorageIterator.h`: iterator API (`get_key`, `get_value`, `++`, `is_end`)
- `keystorage/KeyStorageConcepts.h`: type constraints

Implementations:

- `MapKeyStorage`, `AbslBtreeKeyStorage`
- `TkrzwHashKeyStorage`, `TkrzwTreeKeyStorage`
- `LmdbKeyStorage`, `LmdbNolockKeyStorage`, `LevelDBKeyStorage`
- `UnorderedDenseKeyStorage` (hash-based; builds sorted iteration by collecting and sorting keys)

### `graph/`: Access graph + METIS adapter

Tracking builds a weighted **undirected** graph (METIS requires a symmetric adjacency structure):

- **Vertices** represent keys (weights accumulate access frequency).
- **Edges** represent co-access (weights accumulate co-occurrence; both directions are stored).

Files:

- `graph/Graph.h`: graph structure (uses `ankerl::unordered_dense`)
- `graph/MetisGraph.h`: converts `Graph` to CSR format and partitions it using METIS (`METIS_PartGraphKway`)

### `kvstorage/`: Partitioned and repartitioning storage

This layer coordinates:

- assigning keys to partitions
- executing operations against a per-partition storage engine
- (optionally) collecting access patterns and repartitioning

The CLI selects the primary strategy via `storage_type`:

- **`soft`**: `SoftRepartitioningKeyValueStorage` (single logical storage with partition coordination)
- **`hard`**: `HardRepartitioningKeyValueStorage` (harder boundaries per partition / engine instances)
- **`threaded`**: `SoftThreadedRepartitioningKeyValueStorage` (threaded variant)
- **`hard_threaded`**: `HardThreadedRepartitioningKeyValueStorage` (threaded + hard repartitioning)
- **`engine`**: direct `StorageEngine` usage (no partitioning/repartitioning)
- **`lock_stripping`**: `LockStrippingKeyValueStorage` (static hashed partitions + per-partition locks; no repartitioning)
- **`range_lock_stripping`**: `RangeLockStrippingKeyValueStorage` (static range partitions from `workload.n_records` + per-partition locks; no repartitioning)

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

`CMakeLists.txt` defines the core `repart-kv-core` library and an opt-in `repart-kv-runner` executable. Most storage/tests build by default; `repart-kv-runner` is enabled via `BUILD_REPART_KV_RUNNER=ON` or `build.sh -t`.

CMake **requires** TKRZW and Abseil. LoadGen and `unordered_dense` are fetched with FetchContent. Optional libraries (METIS, LMDB, Tokyo Cabinet, LevelDB, RocksDB, TBB) are detected and warn if missing; METIS gates `test_repartitioning_storage` and the METIS graph targets.

## Extending: adding a StorageEngine backend

Repart-KV can be extended to support additional embedded databases or storage engines. Integration requires two parts:

1. Implement a new `StorageEngine` backend (compile-time polymorphism via CRTP).
2. Register it with the build and the CLI so it can be selected.

### 1) Implement the backend

Create a new header in `storage/` (for example: `storage/MyDbStorageEngine.h`) and implement the interface from `storage/StorageEngine.h`:

- Implement the required methods:
  - `Status read_impl(const std::string& key, std::string& value)`
  - `Status write_impl(const std::string& key, const std::string& value)`
  - `Status async_write_impl(const std::string& key, const std::string& value)` (relaxed/fast path for preload)
  - `Status force_sync_impl()` (durable flush; noop for pure in-memory backends)
  - `Status remove_impl(const std::string& key, std::string& removed_value)`
  - `Status scan_impl(const std::string& key_start, size_t limit, std::vector<std::pair<std::string, std::string>>& out)`
- Optional: `iterator_impl()` for locality-optimized scans.
- Respect the locking contract: public `read()` / `write()` / `scan()` do **not** lock automatically.
- Match the scan semantics: **lower_bound-style** — return keys >= `key_start`, up to `limit`.
- If the backend needs a filesystem location, use the base class `path()` (populated from the CLI `storage_paths` argument).

### 2) Wire it into the build

Update `CMakeLists.txt` to ensure your engine compiles and links:

- Add include paths if needed.
- Link your database library (for example via `find_package(...)` or `pkg_check_modules(...)` and `target_link_libraries(...)`).

If the dependency is optional, follow the pattern used for METIS/TBB/LMDB/RocksDB: detect it, then build/enable the relevant targets conditionally.

### 3) Expose it via the CLI

Update `src/repart_kv.cpp` so `repart-kv-runner` can select the backend:

- Add a new accepted value to the `storage_engine` argument validation.
- Add a new branch in `execute_with_storage_config()` that maps that value to your engine type (similar to `tkrzw_tree`, `rocksdb`, `tbb`).
- Update `print_usage()` so the new engine appears in `--help` output.

`main.cpp` is only a trampoline; do not put engine dispatch there.

### 4) Document and test

- Update [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md) with any new packages required to build/link your engine.
- Consider adding coverage to `storage/test/test_storage_engine.cpp` if the backend should participate in the generic engine test suite.

See:

- [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md)
- [INDEX.md](INDEX.md)
