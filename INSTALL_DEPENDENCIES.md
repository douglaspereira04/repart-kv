# Dependencies

This project is built with C++20. CMake fetches LoadGen and `unordered_dense` automatically. Several optional libraries are needed only for specific storage backends.

## Required (configure + default runtime)

- **CMake**: 3.20+
- **C++20 compiler**: GCC 10+ or Clang 10+
- **pkg-config**
- **TKRZW**: used by the default `tkrzw_tree` and `tkrzw_hash` engines (`pkg_check_modules(TKRZW REQUIRED …)`)
- **Abseil**: `find_package(absl REQUIRED)` (btree containers used by KeyStorage and tests)
- **Network access at configure time**: FetchContent clones [LoadGen](https://github.com/douglaspereira04/loadgen) and [unordered_dense](https://github.com/martinus/unordered_dense)

On Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake build-essential pkg-config \
  libtkrzw-dev liblzma-dev liblz4-dev libzstd-dev \
  libabsl-dev
```

Notes:

- The compression development packages (`liblzma-dev`, `liblz4-dev`, `libzstd-dev`) are required by many packaged TKRZW builds.

## Needed for repartitioning

- **METIS** (`libmetis-dev`): graph partitioning for `soft` / `hard` / threaded storage types. CMake warns and skips METIS-specific test/example targets if it is missing.

```bash
sudo apt-get install -y libmetis-dev
```

## Optional (enabled by CLI selection)

- **Boost Lockfree** (`libboost-dev`): threaded workers (`boost::lockfree::spsc_queue`)
- **Intel TBB** (`libtbb-dev`): `tbb` storage engine and some internal queues
- **LMDB** (`liblmdb-dev`): `lmdb` and `lmdb_nolock` (`MDB_NOLOCK`) engines and KeyStorage
- **Tokyo Cabinet** (`libtokyocabinet-dev`): `tokyo` engine (`TCBDB`, no `tcbdbsetmutex`)
- **LevelDB** (`libleveldb-dev`): `leveldb` storage engine and `LevelDBKeyStorage`
- **RocksDB** (`librocksdb-dev`): `rocksdb` storage engine

On Ubuntu/Debian:

```bash
sudo apt-get install -y libboost-dev libtbb-dev liblmdb-dev libtokyocabinet-dev libleveldb-dev librocksdb-dev
```

## Optional (developer convenience)

- **clang-format**: used by `build.sh` to format `*.h` / `*.cpp`

```bash
sudo apt-get install -y clang-format
```

## Platform notes

### Fedora/RHEL

```bash
sudo dnf install -y cmake gcc-c++ pkgconfig \
  tkrzw-devel xz-devel lz4-devel libzstd-devel \
  abseil-cpp-devel \
  metis-devel \
  boost-devel tbb-devel lmdb-devel tokyocabinet-devel \
  leveldb-devel rocksdb-devel
```

### Arch Linux

```bash
sudo pacman -S --needed cmake base-devel pkgconf \
  tkrzw xz lz4 zstd abseil-cpp metis \
  boost tbb lmdb tokyocabinet leveldb rocksdb \
  clang-format
```

### macOS (Homebrew)

```bash
brew install cmake pkg-config tkrzw xz lz4 zstd abseil metis boost tbb lmdb tokyo-cabinet leveldb rocksdb llvm
```

## Verify installation

```bash
cmake --version
pkg-config --modversion tkrzw
```

If CMake cannot find a library you installed from source, re-run the dynamic linker cache and ensure `pkg-config` can see it:

```bash
sudo ldconfig
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH}"
```

## Next step

Build and run tests:

```bash
./build.sh
```
