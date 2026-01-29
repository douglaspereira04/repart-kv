# KeyStorage

The KeyStorage module provides a typed key/value mapping with:

- **Keys**: `std::string`
- **Values**: constrained by `KeyStorageValueType` (integral types or pointers)
- **Range iteration**: `lower_bound()` + an iterator interface

It is used by higher-level components (for example, to store mappings and indexes) where values are not plain strings.

## Public API

### Storage

`keystorage/KeyStorage.h` defines a CRTP base with:

- `get(const std::string& key, ValueType& out) -> bool`
- `put(const std::string& key, const ValueType& value)`
- `lower_bound(const std::string& key) -> IteratorType`

### Iterator

`keystorage/KeyStorageIterator.h` defines the iterator API:

- `get_key()`
- `get_value()`
- `operator++()`
- `is_end()`

## Value type constraints

`keystorage/KeyStorageConcepts.h` defines:

- `KeyStorageValueType`: integral types (`std::integral`) or pointer types (`std::is_pointer_v`)

## Implementations

- `MapKeyStorage<T>`: `std::map` (ordered)
- `TkrzwTreeKeyStorage<T>`: TKRZW TreeDBM (ordered)
- `TkrzwHashKeyStorage<T>`: TKRZW HashDBM (unordered; builds sorted iteration by collecting and sorting keys)
- `LmdbKeyStorage<T>`: LMDB (ordered)
- `UnorderedDenseKeyStorage<T>`: `ankerl::unordered_dense::map` (unordered; builds sorted iteration by collecting and sorting keys)

## Example

```cpp
#include "keystorage/MapKeyStorage.h"

MapKeyStorage<int> counters;
counters.put("visits", 100);

int visits = 0;
if (counters.get("visits", visits)) {
    // ...
}

auto it = counters.lower_bound("v");
while (!it.is_end()) {
    // it.get_key(), it.get_value()
    ++it;
}
```

## Tools and tests

- Interactive CLI: see [INTERACTIVE_USAGE.md](INTERACTIVE_USAGE.md)
- Test suite: `build/test_keystorage`

