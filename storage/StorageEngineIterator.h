#pragma once

#include "StorageEngineIteratorConcepts.h"
#include "Status.h"
#include <string>

/**
 * @brief CRTP base class for storage engine iterators
 *
 * @tparam Derived The derived iterator type (e.g., LmdbIterator)
 * @tparam StorageEngineType The storage engine type this iterator operates on
 *
 * Uses Curiously Recurring Template Pattern for compile-time polymorphism
 * without virtual functions. Enables storage engines to leverage their
 * internal optimizations (e.g., B+tree iterator positioning, cache locality)
 * when retrieving records that are close but not adjacent.
 *
 * Best use case: retrieve records that, though not next to each other,
 * are close in the storage layout—e.g., keys in the same B+tree leaf
 * or within the same cache line.
 *
 * Derived classes must implement:
 * - find_impl(const std::string& key, std::string& value) -> Status
 *
 * The derived constructor must prepare the iterator for scanning (e.g., open
 * transaction, allocate cursor). The derived destructor must free resources
 * (e.g., close cursor, abort/commit transaction).
 */
template <typename Derived, typename StorageEngineType>
class StorageEngineIterator {
protected:
    StorageEngineType *engine_;

public:
    /**
     * @brief Constructor - stores reference to the storage engine
     * @param engine The storage engine to scan
     *
     * The derived constructor body is responsible for preparing the iterator
     * for scanning (e.g., opening a read transaction, allocating a cursor).
     */
    explicit StorageEngineIterator(StorageEngineType &engine) :
        engine_(&engine) {}

    /**
     * @brief Default destructor
     *
     * The derived destructor must free resources (close cursor, end
     * transaction).
     */
    ~StorageEngineIterator() = default;

    /**
     * @brief Retrieve the value for a given key
     * @param key The key to look up
     * @param value Reference to store the value associated with the key
     * @return Status code indicating the result of the operation
     *
     * Leverages the iterator's position for locality: keys close to the
     * current iterator position may be resolved more efficiently than
     * arbitrary point lookups.
     */
    Status find(const std::string &key, std::string &value) {
        Derived *derived = static_cast<Derived *>(this);
        return derived->find_impl(key, value);
    }
};
