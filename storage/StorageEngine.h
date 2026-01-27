#pragma once

#include "StorageEngineConcepts.h"
#include <atomic>
#include <string>
#include <shared_mutex>
#include <vector>
#include "Status.h"
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
 * - std::vector<std::pair<std::string, std::string>> scan_impl(const
 * std::string& initial_key_prefix, size_t limit) const
 */
template <typename Derived> class StorageEngine {
protected:
    mutable std::shared_mutex _lock; // Mutex for thread-safe operations
    size_t level_;                   // Hierarchy level of this storage engine
    std::atomic_size_t operation_count_; // Number of operations performed on
                                         // this storage engine
    std::string path_; // Path for embedded database files (default: /tmp)
public:
    /**
     * @brief Constructor
     * @param level The hierarchy level for this storage engine
     * @param path Optional path for embedded database files (default: /tmp)
     */
    explicit StorageEngine(size_t level, const std::string &path = "/tmp") :
        level_(level), operation_count_(0), path_(path) {}

    /**
     * @brief Default destructor
     */
    ~StorageEngine() = default;

    /**
     * @brief Read a value by key
     * @param key The key to read
     * @param value Reference to store the value associated with the key
     * @return Status code indicating the result of the operation
     */
    Status read(const std::string &key, std::string &value) {
        Derived *derived = static_cast<Derived *>(this);
        derived->operation_count_.fetch_add(1, std::memory_order_relaxed);
        return derived->read_impl(key, value);
    }

    /**
     * @brief Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     * @return Status code indicating the result of the operation
     */
    Status write(const std::string &key, const std::string &value) {
        Derived *derived = static_cast<Derived *>(this);
        derived->operation_count_.fetch_add(1, std::memory_order_relaxed);
        return derived->write_impl(key, value);
    }

    /**
     * @brief Scan for key-value pairs from a starting point (lower_bound)
     * @param key_start The starting key (returns keys >= key_start)
     * @param limit Maximum number of key-value pairs to return
     * @param results Reference to store the results of the scan
     * @return Status code indicating the result of the operation
     */
    Status scan(const std::string &key_start, size_t limit,
                std::vector<std::pair<std::string, std::string>> &results) {
        Derived *derived = static_cast<Derived *>(this);
        derived->operation_count_.fetch_add(1, std::memory_order_relaxed);
        return derived->scan_impl(key_start, limit, results);
    }

    /**
     * @brief Acquire a shared lock (for read operations)
     * User must manually call this when thread-safety is needed
     */
    void lock_shared() const { _lock.lock_shared(); }

    /**
     * @brief Release a shared lock
     * User must manually call this after lock_shared()
     */
    void unlock_shared() const { _lock.unlock_shared(); }

    /**
     * @brief Acquire an exclusive lock (for write operations)
     * User must manually call this when thread-safety is needed
     */
    void lock() const { _lock.lock(); }

    /**
     * @brief Release an exclusive lock
     * User must manually call this after lock()
     */
    void unlock() const { _lock.unlock(); }

    /**
     * @brief Get the hierarchy level of this storage engine
     * @return The level value
     */
    size_t level() const { return level_; }

    /**
     * @brief Set the hierarchy level of this storage engine
     * @param level The new level value
     */
    void level(size_t level) { level_ = level; }

    size_t operation_count() const { return operation_count_; }

    /**
     * @brief Get the path for embedded database files
     * @return The path string
     */
    const std::string &path() const { return path_; }

    /**
     * @brief Set the path for embedded database files
     * @param path The new path value
     */
    void path(const std::string &path) { path_ = path; }
};
