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

# Build the project
echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"

echo "Running tests..."
cd ..
./run_tests.sh

echo "Tests completed successfully!"