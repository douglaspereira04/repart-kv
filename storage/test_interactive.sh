#!/bin/bash
# Demo script for interactive storage test

cd "$(dirname "$0")/../build"

if [ ! -f ./interactive_storage_test ]; then
    echo "Error: interactive_storage_test not built"
    exit 1
fi

echo "Running interactive storage test demo..."
echo ""

# Test with MapStorageEngine
./interactive_storage_test << 'EOF'
1
write("user:1001", "Alice")
write("user:1002", "Bob")
write("product:2001", "Laptop")
get("user:1001")
get("user:1002")
get("nonexistent")
scan("user:", 10)
scan("product:", 5)
exit
EOF

echo ""
echo "Demo completed!"

