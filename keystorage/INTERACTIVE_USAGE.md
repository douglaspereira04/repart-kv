# Interactive KeyStorage Testing

The `interactive_keystorage_test` program provides an interactive command-line interface for testing and exploring KeyStorage implementations.

## Building

```bash
cd build
cmake ..
make interactive_keystorage_test
```

## Running

```bash
./build/interactive_keystorage_test
```

## Setup

When you run the program, you'll be prompted to:

1. **Select a value type:**
   - `1` - int
   - `2` - long
   - `3` - uint64_t

2. **Select a KeyStorage implementation:**
   - `1` - MapKeyStorage (in-memory, std::map)
   - `2` - TkrzwHashKeyStorage (hash-based, unordered)
   - `3` - TkrzwTreeKeyStorage (tree-based, sorted)

## Commands

### `get("key")`
Retrieve a value by its key.

**Example:**
```
> get("user:1001")
100
```

**Not found:**
```
> get("nonexistent")
(not found)
```

### `put("key", value)`
Store a key-value pair.

**Example:**
```
> put("user:1001", 100)
OK
> put("counter:visits", 1500)
OK
```

### `scan("start_key", limit)`
Scan entries starting from the first key >= start_key, returning up to `limit` results.
Uses iterator incrementation to traverse the storage.

**Example:**
```
> scan("user:", 10)
user:1001 -> 100
user:1002 -> 200
user:1003 -> 300
```

**With limit:**
```
> scan("user:", 2)
user:1001 -> 100
user:1002 -> 200
```

**Scan all entries:**
```
> scan("", 100)
counter:clicks -> 2500
counter:visits -> 1500
user:1001 -> 100
user:1002 -> 200
user:1003 -> 300
```

**No results:**
```
> scan("zzz", 10)
(no results)
```

### `help` or `?`
Display available commands.

### `exit` or `quit`
Exit the program.

## Example Session

```
=== Interactive KeyStorage Test ===

Select value type:
  1. int
  2. long
  3. uint64_t

Enter choice (1-3): 1

Select a key storage implementation:
  1. MapKeyStorage (in-memory, std::map)
  2. TkrzwHashKeyStorage (hash-based, unordered)
  3. TkrzwTreeKeyStorage (tree-based, sorted)

Enter choice (1-3): 1

Using: MapKeyStorage<int>

Available commands:
  get("key")                  - Get a value by key
  put("key", value)           - Put a key-value pair
  scan("start_key", limit)    - Scan entries >= start_key (up to limit)
  help                        - Show this help
  exit                        - Exit the program

> put("user:1001", 100)
OK
> put("user:1002", 200)
OK
> put("counter:visits", 1500)
OK
> get("user:1001")
100
> scan("user:", 10)
user:1001 -> 100
user:1002 -> 200
> exit

Goodbye!
```

## Automated Testing

You can pipe commands to test non-interactively:

```bash
./build/interactive_keystorage_test <<EOF
1
1
put("key1", 100)
put("key2", 200)
get("key1")
scan("key", 10)
exit
EOF
```

Or use a test script:

```bash
./keystorage/test_interactive_keystorage.sh
```

## Use Cases

- **Learning**: Explore how different KeyStorage implementations behave
- **Testing**: Quick manual testing of storage operations
- **Debugging**: Interactive exploration of edge cases
- **Comparison**: Compare behavior between MapKeyStorage, TkrzwHashKeyStorage, and TkrzwTreeKeyStorage
- **Demonstrations**: Show how the KeyStorage interface works

## Key Differences from StorageEngine Interactive Test

1. **Type Safety**: KeyStorage requires value type selection (int, long, uint64_t)
2. **API**: Uses `get`/`put` instead of `read`/`write`
3. **Iteration**: `scan` uses iterators with `lower_bound()` + `++` incrementation
4. **Return Values**: `get` returns boolean and uses output parameter

## Notes

- All KeyStorage implementations must store integral types or pointers (enforced by C++20 concepts)
- `scan` uses `lower_bound()` to find the starting position, then `++` to iterate
- TreeDBM-based storage maintains sorted key order (efficient scanning)
- HashDBM-based storage requires full iteration then sorting for lower_bound
- MapKeyStorage uses std::map which maintains sorted order naturally

