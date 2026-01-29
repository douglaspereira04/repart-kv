# KeyStorage test suite notes

This document describes what the KeyStorage test suite validates and how to run it. It avoids hard-coded counts so it stays accurate as tests evolve.

## What is tested

`build/test_keystorage` exercises KeyStorage implementations across multiple value types and validates:

- `put` / `get` behavior (including overwrite and missing keys)
- `lower_bound` correctness
- iterator semantics (`++`, `is_end`, `get_key`, `get_value`)
- scan-like iteration patterns implemented as `lower_bound` + iterator increment

## How to run

```bash
cd build
./test_keystorage
```

## Implementation notes (ordered vs unordered backends)

KeyStorage requires an ordered iteration API (`lower_bound`). Some backends are naturally ordered; others simulate it:

- **Naturally ordered**: `MapKeyStorage`, `TkrzwTreeKeyStorage`, `LmdbKeyStorage`
- **Simulated order**: `TkrzwHashKeyStorage`, `UnorderedDenseKeyStorage`

For simulated order, `lower_bound` typically:

1. Collects all keys
2. Sorts them
3. Creates an iterator over the sorted key list

This is correct but has a higher upfront cost (\(O(n \log n)\) for sorting).

