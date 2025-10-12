# Interactive Storage Engine Tester

An interactive command-line interface for testing and exploring storage engine implementations.

## Usage

```bash
cd build
./interactive_storage_test
```

## Selecting a Storage Engine

When you start the program, choose one of three storage engines:

1. **MapStorageEngine** - In-memory storage using `std::map`
2. **TkrzwHashStorageEngine** - Hash-based storage (unordered, fast random access)
3. **TkrzwTreeStorageEngine** - Tree-based storage (sorted keys, efficient scans)

## Command Semantics

**Important**: `scan(key_start, limit)` returns all keys >= `key_start`, NOT just keys starting with that prefix. Think of it as a **lower_bound** operation.

## Available Commands

### Write a Key-Value Pair

```
> write("key", "value")
OK
```

**Examples:**
```
> write("user:1001", "Alice")
OK
> write("config:timeout", "30")
OK
> write("product:2001", "Laptop")
OK
```

### Read a Value

```
> get("key")
value
```

**Examples:**
```
> get("user:1001")
Alice
> get("nonexistent")
(empty)
```

### Scan from Starting Key

Scan returns all keys >= the given starting key (like lower_bound).

```
> scan("start_key", limit)
key1
key2
key3
```

**Examples:**
```
> scan("user:", 10)
user:1001
user:1002
user:1003
(returns all keys >= "user:")

> scan("", 5)
(returns first 5 keys in sorted order)

> scan("m", 3)
product:2001
user:1001
user:1002
(returns 3 keys >= "m")
```

**Note**: Scan does NOT filter by prefix. It returns all keys >= the starting key.
If you have keys `["apple", "banana", "cherry"]` and scan `("b", 10)`, you get `["banana", "cherry"]`.

### Other Commands

- `help` or `?` - Display help message
- `exit` or `quit` - Exit the program
- `Ctrl+D` - Exit the program (EOF)

## Command Format

Commands must follow this exact format:
- Strings must be enclosed in double quotes: `"string"`
- No spaces inside the parentheses (except in strings)
- Numbers don't need quotes

**Valid:**
```
get("mykey")
write("key", "value")
scan("prefix:", 100)
```

**Invalid:**
```
get( "mykey" )      # Extra spaces
write('key', 'val') # Single quotes not supported
scan(prefix, 10)    # Missing quotes on string
```

## Example Session

```bash
$ ./interactive_storage_test

=== Interactive Storage Engine Test ===

Select a storage engine:
  1. MapStorageEngine (in-memory, std::map)
  2. TkrzwHashStorageEngine (hash-based, unordered)
  3. TkrzwTreeStorageEngine (tree-based, sorted)

Enter choice (1-3): 3

Using: TkrzwTreeStorageEngine

Available commands:
  get("key")                  - Read a value by key
  write("key", "value")      - Write a key-value pair
  scan("prefix", limit)       - Scan keys with prefix
  help                        - Show this help
  exit                        - Exit the program

> write("user:1001", "Alice")
OK
> write("user:1002", "Bob")
OK
> write("user:1003", "Charlie")
OK
> write("product:2001", "Laptop")
OK
> write("product:2002", "Mouse")
OK
> get("user:1001")
Alice
> get("product:2002")
Mouse
> scan("user:", 10)
user:1001
user:1002
user:1003
> scan("product:", 2)
product:2001
product:2002
> scan("", 3)
product:2001
product:2002
user:1001
> exit

Goodbye!
```

## Comparing Storage Engines

You can test the same commands with different storage engines to observe differences:

### MapStorageEngine
- In-memory only (no persistence)
- Fast and predictable
- Keys returned in sorted order (std::map maintains order)

### TkrzwHashStorageEngine
- Can be persisted to disk
- Hash-based (unordered)
- Very fast random access
- Scan requires iterating all keys

### TkrzwTreeStorageEngine
- Can be persisted to disk
- Tree-based (sorted order)
- Keys always returned in lexicographic order
- Very efficient prefix scans (uses Jump() to skip directly to matching keys)

## Tips

1. Start with simple commands to get familiar with the syntax
2. Use `help` anytime to see available commands
3. Try scanning with empty prefix `scan("", 10)` to see all keys
4. Compare the same operations across different engines to see performance differences
5. Keys are case-sensitive
6. Empty string values are valid

## Automated Testing

You can pipe commands to test programmatically:

```bash
./interactive_storage_test << EOF
1
write("key1", "value1")
write("key2", "value2")
get("key1")
scan("", 10)
exit
EOF
```

