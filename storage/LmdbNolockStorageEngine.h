#pragma once

#include "LmdbStorageEngine.h"

/**
 * @brief LMDB StorageEngine opened with \c MDB_NOLOCK.
 *
 * Same B+tree as \c LmdbStorageEngine, but LMDB performs no locking. The
 * caller (typically kvstorage) must enforce single-writer semantics and that
 * no reader holds a transaction while a writer is active — a reader-writer
 * lock is sufficient.
 */
template <bool SYNC = false> using LmdbNolockStorageEngine =
    LmdbStorageEngine<SYNC, true>;
