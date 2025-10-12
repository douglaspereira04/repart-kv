#!/bin/bash

# Script to test interactive_keystorage_test with MapKeyStorage<int>

echo "Testing interactive_keystorage_test with MapKeyStorage<int>"
echo ""

# Use a heredoc to feed commands to the interactive program
../build/interactive_keystorage_test <<EOF
1
1
put("user:1001", 100)
put("user:1002", 200)
put("user:1003", 300)
put("counter:visits", 1500)
put("counter:clicks", 2500)
get("user:1001")
get("user:1002")
get("counter:visits")
get("nonexistent")
scan("user:", 10)
scan("counter:", 5)
scan("", 100)
scan("user:1002", 2)
exit
EOF

echo ""
echo "Test completed!"

