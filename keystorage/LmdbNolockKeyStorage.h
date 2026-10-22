#pragma once

#include "LmdbKeyStorage.h"

/**
 * @brief LMDB KeyStorage opened with \c MDB_NOLOCK.
 *
 * Caller-owned synchronization: no LMDB lock table. Pair with a
 * reader-writer lock in kvstorage (e.g. \c key_map_lock_).
 */
template <KeyStorageValueType ValueType> using LmdbNolockKeyStorage =
    LmdbKeyStorage<ValueType, true>;
