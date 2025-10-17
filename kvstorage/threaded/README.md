# Threaded Implementations

This directory contains threaded implementations of key-value storage systems.

## Files

- `SoftThreadedRepartitioningKeyValueStorage.h` - A threaded version of the soft repartitioning key-value storage implementation

## Purpose

This directory is dedicated to storing implementations that use threading for improved performance or concurrent access patterns. These implementations may offer different threading strategies compared to the main implementations in the parent directory.

## Usage

To use these threaded implementations, include them with the appropriate relative path:

```cpp
#include "kvstorage/threaded/SoftThreadedRepartitioningKeyValueStorage.h"
```
