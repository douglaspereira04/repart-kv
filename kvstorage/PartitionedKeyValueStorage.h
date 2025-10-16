#pragma once

#include "../storage/StorageEngine.h"
#include <string>
#include <vector>

/**
 * @brief CRTP base class for managing partitioned storage
 * @tparam Derived The derived partitioned storage type
 * @tparam StorageEngineType Type that derives from StorageEngine
 * 
 * Uses Curiously Recurring Template Pattern for compile-time polymorphism
 * without virtual functions. Requires C++20 for compile-time polymorphism.
 * 
 * Derived classes must implement:
 * - std::string read_impl(const std::string& key)
 * - void write_impl(const std::string& key, const std::string& value)
 * - std::vector<std::pair<std::string, std::string>> scan_impl(const std::string& initial_key_prefix, size_t limit)
 */
template<typename Derived, typename StorageEngineType>
class PartitionedKeyValueStorage {
public:
    /**
     * @brief Default constructor
     */
    PartitionedKeyValueStorage() = default;

    /**
     * @brief Read a value by key
     * @param key The key to read
     * @return The value associated with the key
     */
    std::string read(const std::string& key) {
        return static_cast<Derived*>(this)->read_impl(key);
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
     * @brief Scan for key-value pairs starting with a given prefix
     * @param key_prefix The initial key prefix to search for
     * @param limit Maximum number of key-value pairs to return
     * @return Vector of key-value pairs
     */
    std::vector<std::pair<std::string, std::string>> scan(const std::string& initial_key_prefix, size_t limit) {
        return static_cast<Derived*>(this)->scan_impl(initial_key_prefix, limit);
    }

protected:
    /**
     * @brief Protected destructor (CRTP pattern)
     */
    ~PartitionedKeyValueStorage() = default;
};

