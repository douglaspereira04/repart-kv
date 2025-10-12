#!/bin/bash
# Script to compare the three storage engines

cd "$(dirname "$0")/../build"

if [ ! -f ./interactive_storage_test ]; then
    echo "Error: interactive_storage_test not built"
    exit 1
fi

echo "=========================================="
echo "Storage Engine Comparison Demo"
echo "=========================================="
echo ""

# Test commands
TEST_COMMANDS='write("user:1001", "Alice")
write("user:1002", "Bob")
write("product:2001", "Laptop")
write("config:debug", "true")
get("user:1001")
scan("user:", 10)
exit'

for engine in 1 2 3; do
    case $engine in
        1) name="MapStorageEngine (std::map)" ;;
        2) name="TkrzwHashStorageEngine (hash)" ;;
        3) name="TkrzwTreeStorageEngine (tree)" ;;
    esac
    
    echo "=========================================="
    echo "Testing: $name"
    echo "=========================================="
    
    echo "$engine" | cat - <(echo "$TEST_COMMANDS") | ./interactive_storage_test | grep -A 100 "Using:"
    
    echo ""
done

echo "Comparison complete!"
echo ""
echo "Key differences:"
echo "  - MapStorageEngine: In-memory, sorted output"
echo "  - TkrzwHashStorageEngine: Persistent, unordered scan"
echo "  - TkrzwTreeStorageEngine: Persistent, sorted scan"

