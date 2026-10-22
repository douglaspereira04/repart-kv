# Project summary

Repart-KV is a C++20 codebase that executes a LoadGen workload against a partitioned key-value storage layer. It supports multiple storage backends and can repartition based on observed access patterns (via an undirected graph + METIS) for the storage types that enable repartitioning.

## What this repository contains

### Workload executor

- `src/repart_kv.cpp`: core executor logic and `run_repart_kv(argc, argv)` implementation
- `src/repart_kv_api.h`: exported header used by downstream binaries
- `main.cpp`: optional `repart-kv-runner` CLI that forwards arguments to the core entry point
- LoadGen (FetchContent): TOML-driven `RequestGenerator` (LOADING then OPERATIONS)
- `ycsb_a.toml` … `ycsb_e.toml`: bundled YCSB-style LoadGen configs (~1 GiB payload)
- `ycsb_a_large.toml`, `ycsb_d_large.toml`, `ycsb_e_large.toml`: same mixes with 32 GiB of values (`experiment_set_large.sh`)
- `workload/Workload.h`: default 1KB write payload (CSV parser is unused by the runner)

### Storage engines (`storage/`)

String key/value backends:

- `MapStorageEngine`: in-memory `std::map`
- `AbslBtreeStorageEngine`: in-memory `absl::btree_map` (tests; not selectable from the CLI)
- `TkrzwHashStorageEngine`: TKRZW HashDBM
- `TkrzwTreeStorageEngine`: TKRZW TreeDBM
- `LmdbStorageEngine`: LMDB
- `LmdbNolockStorageEngine`: LMDB + `MDB_NOLOCK` (caller-owned sync)
- `LevelDBStorageEngine`: LevelDB (LSM-tree, sorted keys)
- `RocksDBStorageEngine`: RocksDB (LSM-tree, sorted keys)
- `TbbStorageEngine`: in-memory `tbb::concurrent_hash_map`

CLI `storage_engine` values: `tkrzw_tree`, `tkrzw_hash`, `lmdb`, `lmdb_nolock`, `leveldb`, `rocksdb`, `map`, `tbb`.

### KeyStorage (`keystorage/`)

String keys mapping to trivially copyable, default-constructible values, with a CRTP iterator interface:

- `MapKeyStorage`, `AbslBtreeKeyStorage`
- `TkrzwHashKeyStorage`, `TkrzwTreeKeyStorage`
- `LmdbKeyStorage`, `LmdbNolockKeyStorage`, `LevelDBKeyStorage`
- `UnorderedDenseKeyStorage`

### Partitioned / repartitioning storage (`kvstorage/`)

The runner selects these via the `storage_type` CLI argument:

- `soft`: `SoftRepartitioningKeyValueStorage`
- `hard`: `HardRepartitioningKeyValueStorage`
- `threaded`: `SoftThreadedRepartitioningKeyValueStorage`
- `hard_threaded`: `HardThreadedRepartitioningKeyValueStorage`
- `engine`: direct `StorageEngine` usage (no partitioning)
- `lock_stripping`: `LockStrippingKeyValueStorage` (static hash partitions, no repartitioning)
- `range_lock_stripping`: `RangeLockStrippingKeyValueStorage` (static ranges from `workload.n_records`, no repartitioning)

Supporting components:

- `graph/Graph.h`: weighted undirected graph (uses `ankerl::unordered_dense`)
- `graph/MetisGraph.h`: METIS adapter (CSR conversion + k-way partitioning)
- `kvstorage/Tracker.h`: tracking support used by repartitioning strategies

## How to validate locally

```bash
./build.sh
```

This configures CMake, builds targets (core library plus tests), and runs the test suite via `run_tests.sh`. Add `-t` to `./build.sh` when you also want to compile the optional `repart-kv-runner` CLI.
