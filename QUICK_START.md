# Quick Start

This guide builds the project and runs a workload using `repart-kv-runner`.

## Install dependencies (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake build-essential pkg-config \
  libtkrzw-dev liblzma-dev liblz4-dev libzstd-dev \
  libabsl-dev \
  libmetis-dev \
  libboost-dev \
  libtbb-dev \
  liblmdb-dev \
  libleveldb-dev \
  librocksdb-dev \
  clang-format
```

Notes:

- `libabsl-dev` is required (CMake calls `find_package(absl REQUIRED)`).
- `libtbb-dev`, `liblmdb-dev`, `libleveldb-dev`, and `librocksdb-dev` are only needed for the corresponding engines (`tbb`, `lmdb`/`lmdb_nolock`, `leveldb`, `rocksdb`).
- LoadGen and `unordered_dense` are downloaded by CMake (FetchContent); they do not need a system package.
- `clang-format` is used by `build.sh`. If you build manually, it is optional.

For other platforms and minimal installs, see [INSTALL_DEPENDENCIES.md](INSTALL_DEPENDENCIES.md).

## Build and run tests

```bash
./build.sh           # builds repart-kv-core + tests
./build.sh -t        # also builds the optional repart-kv-runner CLI
```

This creates `build/`, builds all configured targets, and runs `run_tests.sh`. The runner binary is only produced when you pass `-t`.

## Run a workload

Only available when you built the runner (`-t` flag above). Then:

```bash
cd build
./repart-kv-runner ../ycsb_c.toml
```

The first argument is a LoadGen TOML file, not a CSV. Bundled configs (`ycsb_a.toml` … `ycsb_e.toml`) are large (1M records / 10M operations); the operation phase is capped at 20 seconds by default (`max_duration_s`).

## Common configurations

### Change partitions and workers

```bash
./repart-kv-runner ../ycsb_a.toml 8 4
```

### Select storage type and backend

```bash
# In-memory, no partitioning (direct engine usage)
./repart-kv-runner ../ycsb_a.toml 4 4 engine tbb

# Threaded repartitioning with in-memory backend
./repart-kv-runner ../ycsb_a.toml 8 4 threaded tbb

# Static hashed partitions (lock stripping), no repartitioning
./repart-kv-runner ../ycsb_a.toml 8 4 lock_stripping tbb

# Static range partitions from workload.n_records, no repartitioning
./repart-kv-runner ../ycsb_a.toml 8 4 range_lock_stripping tbb

# Persistent backend
./repart-kv-runner ../ycsb_a.toml 8 4 soft tkrzw_tree
```

### Use multiple storage paths

`thinking_time_ns` sits before `storage_paths`. Pass `0` when you do not want a delay:

```bash
./repart-kv-runner ../ycsb_a.toml 8 4 soft tkrzw_tree 0 /mnt/d1,/mnt/d2
```

## Inspect metrics

After a run, the executor prints the metrics filename. It is **not** `metrics.csv`; the name encodes the run parameters, for example `ycsb_c__1__soft__4__tkrzw_tree__1__5000__0__sync_off.csv`. A matching `__latency.csv` is written alongside it.

```bash
ls *.csv
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

## Next steps

- Read [ARCHITECTURE.md](ARCHITECTURE.md) for design and data flow.
- Use [INDEX.md](INDEX.md) to locate code by component.
