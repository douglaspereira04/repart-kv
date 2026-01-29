# Interactive KeyStorage tester

`interactive_keystorage_test` is an interactive CLI for exploring KeyStorage implementations.

## Run

```bash
cd build
./interactive_keystorage_test
```

## Setup prompts

1. Select a value type (for example: `int`, `long`, `uint64_t`).
2. Select an implementation (for example: Map, TKRZW Hash/Tree).

The set of available implementations depends on what was built and which optional dependencies are present.

## Commands

### `put("key", value)`

```text
> put("user:1001", 100)
OK
```

### `get("key")`

```text
> get("user:1001")
100
> get("missing")
(not found)
```

### `scan("start_key", limit)`

`scan` is **lower_bound-style**: it returns up to `limit` items with keys >= `start_key`.

```text
> scan("user:", 2)
user:1001 -> 100
user:1002 -> 200
```

### Other commands

- `help` or `?`
- `exit` or `quit`
- `Ctrl+D` (EOF)

## Non-interactive usage

```bash
./interactive_keystorage_test <<'EOF'
1
1
put("k1", 10)
get("k1")
scan("", 10)
exit
EOF
```

## Related

- `keystorage/test_interactive_keystorage.sh`
- `build/test_keystorage`

