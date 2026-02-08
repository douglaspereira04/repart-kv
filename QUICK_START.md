# Quick Start

This guide builds the project and runs a workload using the current `repart-kv` CLI.

## Install dependencies (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake build-essential pkg-config \
  libtkrzw-dev liblzma-dev liblz4-dev libzstd-dev \
  libmetis-dev \
  libboost-dev \
  libtbb-dev \
  liblmdb-dev \
  clang-format
```

Notes:

- `libtbb-dev` and `liblmdb-dev` are only needed if you use the `tbb` or `lmdb` storage engines.
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
./repart-kv-runner ../sample_workload.txt
```

## Common configurations

### Change partitions and workers

```bash
./repart-kv-runner ../sample_workload.txt 8 4
```

### Select storage type and backend

```bash
# In-memory, no repartitioning (direct engine usage)
./repart-kv-runner ../sample_workload.txt 4 4 engine tbb

# Threaded repartitioning type with in-memory backend
./repart-kv-runner ../sample_workload.txt 8 4 threaded tbb

# Persistent backend
./repart-kv-runner ../sample_workload.txt 8 4 soft tkrzw_tree
```

### Use multiple storage paths

```bash
./repart-kv-runner ../sample_workload.txt 8 4 soft tkrzw_tree 0 /mnt/d1,/mnt/d2
```

## Workload file format

- **READ**: `0,<key>`
- **WRITE**: `1,<key>` (writes a default 1KB value)
- **SCAN**: `2,<start_key>,<limit>` (lower_bound-style scan)

Example:

```text
1,user:1001
0,user:1001
2,user:,10
```

## Inspect metrics

After a run, open `metrics.csv` in `build/`:

```bash
cat metrics.csv
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
