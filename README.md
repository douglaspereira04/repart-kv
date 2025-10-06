# Repart-KV

A minimal C++20 project built with CMake.

## Prerequisites

- CMake 3.20 or higher
- A C++20 compatible compiler (GCC 10+, Clang 10+, or MSVC 2019+)
- Make or Ninja build system

## Building

### Option 1: Using the build script (Recommended)

```bash
./build.sh
```

This script will:
- Create a `build` directory
- Configure the project with CMake
- Build the project using all available CPU cores

### Option 2: Manual build

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)
```

### Build Types

By default, the project builds in Release mode. To build in Debug mode:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

## Running

After building, execute the program:

```bash
cd build
./repart-kv
```

Expected output:
```
Hello, World!
```

## Project Structure

```
repart-kv/
├── CMakeLists.txt    # CMake configuration
├── main.cpp         # Main source file
├── build.sh         # Build script
├── README.md        # This file
└── build/           # Build directory (created after building)
```

## Development

The project is configured with:
- C++20 standard
- Strict compiler warnings (`-Wall -Wextra -Wpedantic -Werror`)
- Optimized builds in Release mode (`-O3`)
- Debug symbols in Debug mode (`-g -O0`)

## Clean Build

To clean and rebuild from scratch:

```bash
rm -rf build
./build.sh
```
