# KeyStorage

The KeyStorage module provides a typed key/value mapping with:

- **Keys**: `std::string`
- **Values**: constrained by `KeyStorageValueType` (trivially copyable and default constructible)
- **Range iteration**: `lower_bound()` + an iterator interface

It is used by higher-level components (for example, to store mappings and indexes) where values are not plain strings.

## Public API

### Storage

`keystorage/KeyStorage.h` defines a CRTP base with:

- `get(const std::string& key, ValueType& out) -> bool`
- `put(const std::string& key, const ValueType& value)`
- `scan(const std::string& key_start, size_t limit, std::vector<std::pair<std::string, ValueType>>& results)`
- `get_or_insert(const std::string& key, const ValueType& value_to_insert, ValueType& found_value) -> bool`
- `lower_bound(const std::string& key) -> IteratorType`

### Iterator

`keystorage/KeyStorageIterator.h` defines the iterator API:

- `get_key()`
- `get_value()`
- `operator++()`
- `is_end()`

## Value type constraints

`keystorage/KeyStorageConcepts.h` defines:

- `KeyStorageValueType`: `std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>`

That covers integrals, pointers, and small POD structs. Persistent backends round-trip the object representation via `keystorage/KeyStorageValueBinary.h`. Pointer fields store address bits only, not the pointed-to object.

## Implementations

- `MapKeyStorage<T>`: `std::map` (ordered)
- `AbslBtreeKeyStorage<T>`: `absl::btree_map` (ordered)
- `TkrzwTreeKeyStorage<T>`: TKRZW TreeDBM (ordered)
- `TkrzwHashKeyStorage<T>`: TKRZW HashDBM (unordered; builds sorted iteration by collecting and sorting keys)
- `LmdbKeyStorage<T>`: LMDB (ordered)
- `LmdbNolockKeyStorage<T>`: LMDB with `MDB_NOLOCK` (caller-owned sync)
- `TokyoCabinetKeyStorage<T>`: Tokyo Cabinet TCBDB without `tcbdbsetmutex`
- `LevelDBKeyStorage<T>`: LevelDB (ordered)
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
- Notes: [TEST_RESULTS.md](TEST_RESULTS.md)
