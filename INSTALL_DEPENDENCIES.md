# Dependencies

This project is built with C++20 and uses several optional libraries depending on the storage backends you enable.

## Required (build + default runtime)

- **CMake**: 3.20+
- **C++20 compiler**: GCC 10+ or Clang 10+
- **pkg-config**
- **TKRZW**: used by the default `tkrzw_tree` and `tkrzw_hash` engines
- **METIS**: used for graph partitioning in repartitioning storage types

On Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake build-essential pkg-config \
  libtkrzw-dev liblzma-dev liblz4-dev libzstd-dev \
  libmetis-dev
```

Notes:

- The compression development packages (`liblzma-dev`, `liblz4-dev`, `libzstd-dev`) are required by many packaged TKRZW builds.

## Optional (enabled by CLI selection)

- **Boost Lockfree** (`libboost-dev`): used by threaded worker implementations (`boost::lockfree::spsc_queue`)
- **Intel TBB** (`libtbb-dev`): required for `tbb` storage engine and some internal queues
- **LMDB** (`liblmdb-dev`): required for `lmdb` storage engine and LMDB-based KeyStorage

On Ubuntu/Debian:

```bash
sudo apt-get install -y libboost-dev libtbb-dev liblmdb-dev
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
  metis-devel \
  boost-devel tbb-devel lmdb-devel
```

### Arch Linux

```bash
sudo pacman -S --needed cmake base-devel pkgconf \
  tkrzw xz lz4 zstd metis \
  boost tbb lmdb \
  clang-format
```

### macOS (Homebrew)

```bash
brew install cmake pkg-config tkrzw xz lz4 zstd metis boost tbb lmdb llvm
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
