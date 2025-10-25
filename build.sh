#!/bin/bash

# Build script for repart-kv project

set -e  # Exit on any error

# Create build directory if it doesn't exist
mkdir -p build

# Change to build directory
cd build

# Configure with CMake
echo "Configuring project with CMake..."
cmake ..

# Format code with clang-format
echo "Formatting code with clang-format..."
cd ..
find . -name "*.h" -o -name "*.cpp" | grep -v build | xargs clang-format -i
cd build

# Build the project
echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"

echo "Running tests..."
cd ..
./run_tests.sh

echo "Tests completed successfully!"