# Installing Dependencies for Repart-KV

This document provides detailed instructions for installing all required and optional dependencies for the Repart-KV partitioned key-value storage system.

## Quick Install (Ubuntu/Debian)

```bash
# Install all required dependencies
sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    pkg-config \
    libtkrzw-dev \
    liblzma-dev \
    liblz4-dev \
    libzstd-dev \
    libmetis-dev
```

## Step-by-Step Installation

### 1. Basic Build Tools

```bash
sudo apt-get install cmake build-essential pkg-config
```

**Requirements:**
- CMake 3.20+
- GCC 10+ or Clang 10+
- pkg-config for library detection

### 2. TKRZW Database Library

```bash
sudo apt-get install libtkrzw-dev
```

**Purpose:** High-performance persistent key-value storage backend
- Used by TkrzwHashStorageEngine and TkrzwTreeStorageEngine
- Provides HashDBM and TreeDBM implementations

### 3. Compression Libraries (Required by TKRZW)

```bash
sudo apt-get install liblzma-dev liblz4-dev libzstd-dev
```

**Purpose:** TKRZW compression support
- `liblzma-dev` - LZMA compression
- `liblz4-dev` - LZ4 fast compression
- `libzstd-dev` - Zstandard compression

**Note:** These are **required**, not optional. TKRZW depends on them.

### 4. METIS Graph Partitioning Library (Required)

```bash
sudo apt-get install libmetis-dev
```

**Purpose:** Graph partitioning for dynamic repartitioning
- Used by MetisGraph class
- Powers the RepartitioningKeyValueStorage
- Optimally partitions access pattern graphs

**Key Features:**
- Multi-level graph partitioning
- Edge-cut minimization
- Load balancing
- Fast recursive bisection

## Verification

After installation, verify all libraries are found:

### Verify TKRZW
```bash
pkg-config --modversion tkrzw
pkg-config --libs tkrzw
```

Expected: Version 1.0.x and library paths

### Verify METIS
```bash
ls /usr/lib/x86_64-linux-gnu/libmetis.so
# or
ls /usr/lib/libmetis.so
```

Expected: Library file exists

### Verify Compression Libraries
```bash
dpkg -l | grep -E "liblzma-dev|liblz4-dev|libzstd-dev"
```

Expected: All three packages shown as installed

## Platform-Specific Instructions

### Fedora/RHEL

```bash
sudo dnf install cmake gcc-c++ pkgconfig
sudo dnf install tkrzw-devel
sudo dnf install xz-devel lz4-devel libzstd-devel
sudo dnf install metis-devel
```

### Arch Linux

```bash
sudo pacman -S cmake base-devel pkgconf
sudo pacman -S tkrzw
sudo pacman -S xz lz4 zstd
sudo pacman -S metis
```

### macOS (Homebrew)

```bash
brew install cmake pkgconfig
brew install tkrzw
brew install xz lz4 zstd
brew install metis
```

## Building TKRZW from Source

If your distribution doesn't have TKRZW packages:

```bash
# Install compression libraries first
sudo apt-get install liblzma-dev liblz4-dev libzstd-dev

# Download and build TKRZW
wget https://dbmx.net/tkrzw/pkg/tkrzw-1.0.29.tar.gz
tar xzf tkrzw-1.0.29.tar.gz
cd tkrzw-1.0.29
./configure
make -j$(nproc)
sudo make install
sudo ldconfig
```

## Building METIS from Source

If your distribution doesn't have METIS packages:

```bash
# Install GKlib (METIS dependency)
git clone https://github.com/KarypisLab/GKlib.git
cd GKlib
make config prefix=/usr/local
make -j$(nproc)
sudo make install
cd ..

# Install METIS
git clone https://github.com/KarypisLab/METIS.git
cd METIS
make config prefix=/usr/local
make -j$(nproc)
sudo make install
sudo ldconfig
```

## Troubleshooting

### "tkrzw not found" error

```bash
# Update pkg-config cache
sudo ldconfig
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

### "cannot find -llzma" errors

This means the compression development libraries aren't installed:

```bash
sudo apt-get install liblzma-dev liblz4-dev libzstd-dev
```

### "undefined reference to METIS_*" errors

METIS library not found or not linked:

```bash
# Install METIS development package
sudo apt-get install libmetis-dev

# If building from source, ensure it's in the library path
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
sudo ldconfig
```

### Check what's installed

```bash
# Check runtime libraries
dpkg -l | grep -E "lzma|lz4|zstd|metis"

# Check development libraries  
dpkg -l | grep -E "liblzma-dev|liblz4-dev|libzstd-dev|libmetis-dev"

# Check METIS library
ls -la /usr/lib/x86_64-linux-gnu/libmetis.so*
```

### CMake can't find METIS

If CMake reports "METIS library not found":

```bash
# Check if library exists
find /usr -name "libmetis.so*" 2>/dev/null

# If found but CMake doesn't detect it, add to CMAKE_PREFIX_PATH
cmake -DCMAKE_PREFIX_PATH=/usr/local ..
```

## Minimum Requirements

### Compiler
- **GCC**: 10.0 or higher
- **Clang**: 10.0 or higher
- **MSVC**: 2019 or higher
- **C++ Standard**: C++20

### Libraries
- **CMake**: 3.20 or higher
- **TKRZW**: 1.0.27 or higher
- **METIS**: 5.1.0 or higher
- **Compression libs**: lzma, lz4, zstd (development packages)

## What Each Dependency Does

| Dependency | Purpose | Used By |
|------------|---------|---------|
| CMake | Build system | Entire project |
| GCC/Clang | C++20 compiler | Entire project |
| pkg-config | Library detection | Build system |
| TKRZW | Persistent storage | TkrzwHashStorageEngine, TkrzwTreeStorageEngine |
| lzma/lz4/zstd | Compression | TKRZW (required) |
| METIS | Graph partitioning | MetisGraph, RepartitioningKeyValueStorage |

## Optional Tools

For development and testing:

```bash
# Valgrind for memory checking
sudo apt-get install valgrind

# GDB debugger
sudo apt-get install gdb

# Performance profiling
sudo apt-get install linux-tools-common linux-tools-generic
```

## Verification Script

Save this as `check_deps.sh`:

```bash
#!/bin/bash

echo "=== Checking Repart-KV Dependencies ==="
echo

# Check CMake
cmake --version && echo "✓ CMake installed" || echo "✗ CMake missing"

# Check compiler
g++ --version | head -1 && echo "✓ GCC installed" || echo "✗ GCC missing"

# Check pkg-config
pkg-config --version && echo "✓ pkg-config installed" || echo "✗ pkg-config missing"

# Check TKRZW
pkg-config --exists tkrzw && echo "✓ TKRZW found" || echo "✗ TKRZW missing"

# Check METIS
ls /usr/lib/x86_64-linux-gnu/libmetis.so* >/dev/null 2>&1 && echo "✓ METIS found" || echo "✗ METIS missing"

# Check compression libs
dpkg -l | grep -q liblzma-dev && echo "✓ liblzma-dev installed" || echo "✗ liblzma-dev missing"
dpkg -l | grep -q liblz4-dev && echo "✓ liblz4-dev installed" || echo "✗ liblz4-dev missing"
dpkg -l | grep -q libzstd-dev && echo "✓ libzstd-dev installed" || echo "✗ libzstd-dev missing"

echo
echo "=== Summary ==="
echo "If all checks pass (✓), you're ready to build!"
```

Run with: `chmod +x check_deps.sh && ./check_deps.sh`

## Post-Installation

After all dependencies are installed:

1. **Clone/Download the repository**
2. **Build the project**: `./build.sh`
3. **Run tests**: `cd build && ./test_repartitioning`
4. **Execute workloads**: `./repart-kv workload.txt 8 4`

See [README.md](README.md) for detailed usage instructions.
