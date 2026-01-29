# Threaded storage (`kvstorage/threaded/`)

This directory contains the threaded variants of the repartitioning storage layer and the supporting worker/operation infrastructure they use.

## Key files

### Storage implementations

- `SoftThreadedRepartitioningKeyValueStorage.h`
- `HardThreadedRepartitioningKeyValueStorage.h`

These are selected by the `repart-kv` CLI via `storage_type`:

- `threaded` maps to `SoftThreadedRepartitioningKeyValueStorage`
- `hard_threaded` maps to `HardThreadedRepartitioningKeyValueStorage`

### Workers

- `SoftPartitionWorker.h`
- `HardPartitionWorker.h`

These workers coordinate partition-local work and inter-thread communication. Some paths use `boost::lockfree::spsc_queue` for SPSC queues.

### Operation model and futures

- `operation/`: operation types and tests (`test_operation`, `test_readoperation`, `test_writeoperation`, etc.)
- `future/`: `Future` abstraction and tests (`test_future`)

## Build and tests

```bash
./build.sh
```

Selected tests:

```bash
cd build
./test_soft_partition_worker
./test_operation
./test_readoperation
./test_writeoperation
./test_scanoperation
./test_doneoperation
./test_syncoperation
./test_future
```
