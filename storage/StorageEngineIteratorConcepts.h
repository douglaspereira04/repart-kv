#pragma once

#include <concepts>
#include <string>

#include "Status.h"

/**
 * @brief Concept for types that implement the StorageEngineIterator interface
 *
 * StorageEngineIterator implementations leverage storage engine optimizations
 * for locality when scanning records. Best used when retrieving records that,
 * though not adjacent, are close in the storage layout (e.g., keys in the
 * same B+tree leaf or cache line).
 *
 * @tparam T The iterator implementation type
 * @tparam StorageEngineType The storage engine type this iterator operates on
 */
template <typename T, typename StorageEngineType>
concept StorageEngineIteratorImpl =
    requires(T iter, const std::string &key, std::string &value) {
        { iter.find_impl(key, value) } -> std::same_as<Status>;
    };
