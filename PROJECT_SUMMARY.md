# Project summary

Repart-KV is a C++20 codebase that executes a workload file against a partitioned key-value storage layer. It supports multiple storage backends and can repartition based on observed access patterns (via a graph + METIS) for the storage types that enable repartitioning.

## What this repository contains

### Workload executor

- `src/repart_kv.cpp`: core executor logic and `run_repart_kv(argc, argv)` implementation
- `src/repart_kv_api.h`: exported header used by downstream binaries
- `main.cpp`: optional `repart-kv-runner` CLI that forwards arguments to the core entry point
- `workload/Workload.h`: workload parsing and operation model (READ/WRITE/SCAN)

### Storage engines (`storage/`)

String key/value backends used by the runner:

- `MapStorageEngine`: in-memory `std::map`
- `TkrzwHashStorageEngine`: TKRZW HashDBM
- `TkrzwTreeStorageEngine`: TKRZW TreeDBM
- `LmdbStorageEngine`: LMDB
- `TbbStorageEngine`: in-memory `tbb::concurrent_hash_map`

### KeyStorage (`keystorage/`)

String keys mapping to integral or pointer values, with a CRTP iterator interface:

- `MapKeyStorage`, `TkrzwHashKeyStorage`, `TkrzwTreeKeyStorage`
- `LmdbKeyStorage`
- `UnorderedDenseKeyStorage`

### Partitioned / repartitioning storage (`kvstorage/`)

The `repart-kv` executable selects these via the `storage_type` CLI argument:

- `soft`: `SoftRepartitioningKeyValueStorage`
- `hard`: `HardRepartitioningKeyValueStorage`
- `threaded`: `SoftThreadedRepartitioningKeyValueStorage`
- `hard_threaded`: `HardThreadedRepartitioningKeyValueStorage`
- `engine`: direct `StorageEngine` usage (no repartitioning)

Supporting components:

- `graph/Graph.h`: weighted directed graph (uses `ankerl::unordered_dense`)
- `graph/MetisGraph.h`: METIS adapter (CSR conversion + partitioning)
- `kvstorage/Tracker.h`: tracking support used by repartitioning strategies

## How to validate locally

```bash
./build.sh
```

This configures CMake, builds targets (core library plus tests), and runs the test suite via `run_tests.sh`. Add `-t` to `./build.sh` when you also want to compile the optional `repart-kv-runner` CLI.

