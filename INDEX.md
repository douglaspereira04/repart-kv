# Project index

This index points to the main documents, modules, and build targets that are part of the current repository.

## Documentation

- [README.md](README.md): overview, build/run instructions
- [QUICK_START.md](QUICK_START.md): minimal end-to-end workflow
- [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md): required/optional dependencies
- [ARCHITECTURE.md](ARCHITECTURE.md): design and data flow
- [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md): component and target summary

Module docs:

- [storage/INTERACTIVE_USAGE.md](storage/INTERACTIVE_USAGE.md)
- [keystorage/README.md](keystorage/README.md)
- [keystorage/INTERACTIVE_USAGE.md](keystorage/INTERACTIVE_USAGE.md)
- [graph/README.md](graph/README.md)
- [kvstorage/threaded/README.md](kvstorage/threaded/README.md)
- [utils/README.md](utils/README.md)

## Modules

### `storage/` (String to String)

- `storage/StorageEngine.h`: CRTP base interface
- `storage/MapStorageEngine.h`: in-memory `std::map`
- `storage/TkrzwHashStorageEngine.h`: TKRZW HashDBM
- `storage/TkrzwTreeStorageEngine.h`: TKRZW TreeDBM
- `storage/LmdbStorageEngine.h`: LMDB backend
- `storage/TbbStorageEngine.h`: in-memory `tbb::concurrent_hash_map`

### `keystorage/` (String to integral/pointer)

- `keystorage/KeyStorage.h`: CRTP base interface
- `keystorage/KeyStorageIterator.h`: iterator base
- `keystorage/KeyStorageConcepts.h`: `KeyStorageValueType` constraints
- Implementations:
  - `keystorage/MapKeyStorage.h`
  - `keystorage/TkrzwHashKeyStorage.h`
  - `keystorage/TkrzwTreeKeyStorage.h`
  - `keystorage/LmdbKeyStorage.h`
  - `keystorage/UnorderedDenseKeyStorage.h`

### `graph/`

- `graph/Graph.h`: weighted directed graph (uses `ankerl::unordered_dense`)
- `graph/MetisGraph.h`: METIS adapter (CSR conversion + partitioning)

### `kvstorage/`

- `kvstorage/PartitionedKeyValueStorage.h`: CRTP base for partitioned storage
- `kvstorage/HardRepartitioningKeyValueStorage.h`
- `kvstorage/SoftRepartitioningKeyValueStorage.h`
- `kvstorage/RepartitioningKeyValueStorage.h`
- `kvstorage/Tracker.h`: access tracking support
- `kvstorage/threaded/`: threaded variants and worker/operation primitives

### `workload/`

- `workload/Workload.h`: workload parsing and operation types

## Build system and entry points

- `CMakeLists.txt`: defines `repart-kv-core` and the opt-in `repart-kv-runner`
- `build.sh`: configure/build/test helper (also runs `clang-format`; pass `-t` to build runner)
- `run_tests.sh`: runs `build/test_*` executables
- `main.cpp`: CLI trampoline that forwards into `run_repart_kv`

## CMake targets

Core:

- `repart-kv-core` (static library)
- `repart-kv-runner` (optional executable; enabled via `BUILD_REPART_KV_RUNNER=ON`)
- `interactive_storage_test`
- `interactive_keystorage_test`

Tests:

- `test_storage_engine`
- `test_keystorage`
- `test_partitioned_kv_storage`
- `test_graph_tracking`
- `test_repartitioning_storage` (only when METIS is available)
- `test_graph`
- `test_metis_graph` (only when METIS is available)
- `test_future`
- `test_operation`
- `test_readoperation`
- `test_storage_operation`
- `test_writeoperation`
- `test_scanoperation`
- `test_doneoperation`
- `test_syncoperation`
- `test_soft_partition_worker`

Examples:

- `example_graph`
- `example_metis_graph` (only when METIS is available)
