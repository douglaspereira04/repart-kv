# Installing Dependencies for Repart-KV

This document provides detailed instructions for installing all required and optional dependencies.

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
    libzstd-dev
```

## Step-by-Step Installation

### 1. Basic Build Tools

```bash
sudo apt-get install cmake build-essential pkg-config
```

### 2. TKRZW Database Library

```bash
sudo apt-get install libtkrzw-dev
```

### 3. Compression Libraries (Required by TKRZW)

```bash
sudo apt-get install liblzma-dev liblz4-dev libzstd-dev
```

## Verification

After installation, verify the libraries are found:

```bash
pkg-config --modversion tkrzw
pkg-config --libs tkrzw
```

Expected output should show version 1.0.x and library paths.

## Platform-Specific Instructions

### Fedora/RHEL

```bash
sudo dnf install cmake gcc-c++ pkgconfig
sudo dnf install tkrzw-devel
sudo dnf install xz-devel lz4-devel libzstd-devel
```

### Arch Linux

```bash
sudo pacman -S cmake base-devel pkgconf
sudo pacman -S tkrzw
sudo pacman -S xz lz4 zstd
```

### macOS (Homebrew)

```bash
brew install cmake pkgconfig
brew install tkrzw
brew install xz lz4 zstd
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

### Check what's installed

```bash
# Check runtime libraries
dpkg -l | grep -E "lzma|lz4|zstd"

# Check development libraries  
dpkg -l | grep -E "liblzma-dev|liblz4-dev|libzstd-dev"
```

## Minimum Requirements

- **CMake**: 3.20 or higher
- **Compiler**: GCC 10+, Clang 10+, or MSVC 2019+
- **TKRZW**: 1.0.27 or higher
- **Compression libs**: lzma, lz4, zstd (development packages)

