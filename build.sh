#!/bin/bash

# Build script for repart-kv project

set -e  # Exit on any error

# Default build type
BUILD_TYPE="Release"
# Optionally build the runner/test utility
BUILD_RUNNER="OFF"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -d, --debug     Build in Debug mode (no optimizations, with debug symbols)"
            echo "  -r, --release   Build in Release mode (optimizations enabled) [default]"
            echo "  -t, --runner    Enable the Repart-KV runner/testing utility"
            echo "  -h, --help      Show this help message"
            exit 0
            ;;
        -t|--runner)
            BUILD_RUNNER="ON"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Create build directory if it doesn't exist
mkdir -p build

# Change to build directory
cd build

# Configure with CMake
echo "Configuring project with CMake (Build type: $BUILD_TYPE, runner: $BUILD_RUNNER)..."
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_REPART_KV_RUNNER=$BUILD_RUNNER ..

# Format code with clang-format
echo "Formatting code with clang-format..."
cd ..
find . \( -name "*.h" -o -name "*.cpp" \) ! -path "./build/*" ! -path "./.git/*" -type f -print0 | xargs -0 -r clang-format -i
cd build

# Build the project
echo "Building project in $BUILD_TYPE mode..."
make -j$(nproc)

echo "Build completed successfully! (Build type: $BUILD_TYPE)"

echo "Running tests..."
cd ..
./run_tests.sh

echo "Tests completed successfully!"