#pragma once

#include "StorageEngineConcepts.h"
#include <string>
#include <shared_mutex>
#include <vector>

/**
 * @brief CRTP base class for storage engines
 * @tparam Derived The derived storage engine type
 * 
 * Uses Curiously Recurring Template Pattern for compile-time polymorphism
 * without virtual functions. Requires C++20 for compile-time polymorphism.
 * Provides locking primitives for manual thread-safety control.
 * 
 * Note: read(), write(), and scan() methods do NOT automatically lock.
 * Users must manually call lock()/unlock() or lock_shared()/unlock_shared()
 * when thread-safety is required.
 * 
 * Derived classes must implement:
 * - std::string read_impl(const std::string& key) const
 * - void write_impl(const std::string& key, const std::string& value)
 * - std::vector<std::string> scan_impl(const std::string& key_prefix, size_t limit) const
 */
template<typename Derived>
class StorageEngine {
protected:
    mutable std::shared_mutex _lock;  // Mutex for thread-safe operations

public:
    /**
     * @brief Default constructor
     */
    StorageEngine() = default;

    /**
     * @brief Default destructor
     */
    ~StorageEngine() = default;

    /**
     * @brief Read a value by key
     * @param key The key to read
     * @return The value associated with the key
     */
    std::string read(const std::string& key) const {
        return static_cast<const Derived*>(this)->read_impl(key);
    }

    /**
     * @brief Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     */
    void write(const std::string& key, const std::string& value) {
        static_cast<Derived*>(this)->write_impl(key, value);
    }

    /**
     * @brief Scan for keys starting with a given prefix
     * @param key_prefix The prefix to search for
     * @param limit Maximum number of keys to return
     * @return Vector of keys matching the prefix
     */
    std::vector<std::string> scan(const std::string& key_prefix, size_t limit) const {
        return static_cast<const Derived*>(this)->scan_impl(key_prefix, limit);
    }

    /**
     * @brief Acquire a shared lock (for read operations)
     * User must manually call this when thread-safety is needed
     */
    void lock_shared() const {
        _lock.lock_shared();
    }

    /**
     * @brief Release a shared lock
     * User must manually call this after lock_shared()
     */
    void unlock_shared() const {
        _lock.unlock_shared();
    }

    /**
     * @brief Acquire an exclusive lock (for write operations)
     * User must manually call this when thread-safety is needed
     */
    void lock() const {
        _lock.lock();
    }

    /**
     * @brief Release an exclusive lock
     * User must manually call this after lock()
     */
    void unlock() const {
        _lock.unlock();
    }
};
