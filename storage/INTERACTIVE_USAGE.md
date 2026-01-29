# Interactive storage engine tester

`interactive_storage_test` is an interactive CLI for exploring the `StorageEngine` implementations under `storage/`.

## Run

```bash
cd build
./interactive_storage_test
```

## Choose an engine

The program prompts you to select one backend:

1. `MapStorageEngine` (in-memory, `std::map`)
2. `TkrzwHashStorageEngine` (persistent, hash-based)
3. `TkrzwTreeStorageEngine` (persistent, ordered keys)
4. `TbbStorageEngine` (in-memory, `tbb::concurrent_hash_map`)

## Commands

### `write("key", "value")`

```text
> write("user:1001", "Alice")
OK
```

### `get("key")`

```text
> get("user:1001")
Alice
> get("missing")
(empty)
```

### `scan("start_key", limit)`

`scan` is **lower_bound-style**: it returns up to `limit` items with keys >= `start_key`.

```text
> scan("user:", 3)
user:1001 -> Alice
user:1002 -> Bob
user:1003 -> Charlie
```

If nothing matches:

```text
> scan("zzz", 10)
(no results)
```

### Other commands

- `help` or `?`
- `exit` or `quit`
- `Ctrl+D` (EOF)

## Input rules

- Use double quotes for strings.
- Avoid extra spaces inside parentheses (the parser is strict).

Valid:

```text
get("k")
write("k", "v")
scan("k", 10)
```

Invalid:

```text
get( "k" )
write('k', 'v')
scan(k, 10)
```

## Non-interactive usage

```bash
./interactive_storage_test <<'EOF'
1
write("k1", "v1")
get("k1")
scan("", 10)
exit
EOF
```

