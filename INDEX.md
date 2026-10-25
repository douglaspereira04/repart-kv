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
- [keystorage/TEST_RESULTS.md](keystorage/TEST_RESULTS.md)
- [graph/README.md](graph/README.md)
- [kvstorage/threaded/README.md](kvstorage/threaded/README.md)
- [utils/README.md](utils/README.md)

## Modules

### `storage/` (String to String)

- `storage/StorageEngine.h`: CRTP base interface (`SYNC` flag, `async_write` / `force_sync` / `remove`)
- `storage/MapStorageEngine.h`: in-memory `std::map`
- `storage/AbslBtreeStorageEngine.h`: in-memory `absl::btree_map` (tests; not a CLI engine)
- `storage/TkrzwHashStorageEngine.h`: TKRZW HashDBM
- `storage/TkrzwTreeStorageEngine.h`: TKRZW TreeDBM
- `storage/LmdbStorageEngine.h`: LMDB backend
- `storage/LmdbNolockStorageEngine.h`: LMDB with `MDB_NOLOCK`
- `storage/LevelDBStorageEngine.h`: LevelDB LSM-tree backend
- `storage/RocksDBStorageEngine.h`: RocksDB LSM-tree backend
- `storage/TbbStorageEngine.h`: in-memory `tbb::concurrent_hash_map`

### `keystorage/` (String to trivially copyable values)

- `keystorage/KeyStorage.h`: CRTP base interface
- `keystorage/KeyStorageIterator.h`: iterator base
- `keystorage/KeyStorageConcepts.h`: `KeyStorageValueType` constraints
- `keystorage/KeyStorageValueBinary.h`: binary round-trip helpers for persisted values
- Implementations:
  - `keystorage/MapKeyStorage.h`
  - `keystorage/AbslBtreeKeyStorage.h`
  - `keystorage/TkrzwHashKeyStorage.h`
  - `keystorage/TkrzwTreeKeyStorage.h`
  - `keystorage/LmdbKeyStorage.h`
  - `keystorage/LmdbNolockKeyStorage.h`
  - `keystorage/LevelDBKeyStorage.h`
  - `keystorage/UnorderedDenseKeyStorage.h`

### `graph/`

- `graph/Graph.h`: weighted **undirected** graph (uses `ankerl::unordered_dense`)
- `graph/MetisGraph.h`: METIS adapter (CSR conversion + k-way partitioning)

### `kvstorage/`

- `kvstorage/PartitionedKeyValueStorage.h`: CRTP base for partitioned storage
- `kvstorage/HardRepartitioningKeyValueStorage.h`
- `kvstorage/SoftRepartitioningKeyValueStorage.h`
- `kvstorage/RepartitioningKeyValueStorage.h`
- `kvstorage/LockStrippingKeyValueStorage.h`: static hashed partitions (`storage_type=lock_stripping`)
- `kvstorage/RangeLockStrippingKeyValueStorage.h`: static range partitions (`storage_type=range_lock_stripping`)
- `kvstorage/Tracker.h`: access tracking support
- `kvstorage/threaded/`: threaded variants and worker/operation primitives

### `workload/` and LoadGen

- `workload/Workload.h`: default 1KB write value; unused CSV parser
- LoadGen (FetchContent): `workload::RequestGenerator` used by `src/repart_kv.cpp`
- `ycsb_a.toml` … `ycsb_e.toml`: bundled LoadGen configs (~1 GiB payload)
- `ycsb_a_large.toml`, `ycsb_d_large.toml`, `ycsb_e_large.toml`: same mixes, 16 GiB of values (`experiment_set_large.sh`)
- `ycsb_a_mini.toml`, `ycsb_d_mini.toml`, `ycsb_e_mini.toml`: same mixes, 10240 records (`experiment_set_mini.sh`, `experiment_set_mini_full.sh`)

### `src/`

- `src/repart_kv_api.h`: `run_repart_kv(argc, argv)`
- `src/repart_kv.cpp`: CLI parsing, LoadGen orchestration, engine dispatch

## Build system and entry points

- `CMakeLists.txt`: defines `repart-kv-core` and the opt-in `repart-kv-runner`
- `build.sh`: configure/build/test helper (also runs `clang-format`; pass `-t` to build runner)
- `run_tests.sh`: runs `build/test_*` executables
- `experiment_set_lib.sh`: shared runners sourced by `experiment_set*.sh` (`--dry-run` / `-n` lists runner commands)
- `experiment_set.sh`: experiment grid for the 1 GiB YCSB configs
- `experiment_set_large.sh`: same grid with `*_large.toml` (16 GiB payloads)
- `experiment_set_mini.sh`: small smoke grid with `*_mini.toml`
- `experiment_set_mini_full.sh`: same grid as `experiment_set.sh` with `*_mini.toml`
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

`keystorage/test/test_concepts.cpp` and `keystorage/test/test_invalid_type.cpp` exist in the tree but are **not** CMake targets.

Examples:

- `example_graph`
- `example_metis_graph` (only when METIS is available)
