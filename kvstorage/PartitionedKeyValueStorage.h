#pragma once

#include "../storage/StorageEngine.h"
#include "../storage/Status.h"
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
 * - std::vector<std::pair<std::string, std::string>> scan_impl(const
 * std::string& initial_key_prefix, size_t limit)
 */
template <typename Derived, typename StorageEngineType>
class PartitionedKeyValueStorage {
public:
    /**
     * @brief Default constructor
     */
    PartitionedKeyValueStorage() = default;

    /**
     * @brief Read a value by key
     * @param key The key to read
     * @param value Reference to store the value associated with the key
     * @return Status code indicating the result of the operation
     */
    Status read(const std::string &key, std::string &value) {
        return static_cast<Derived *>(this)->read_impl(key, value);
    }

    /**
     * @brief Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     * @return Status code indicating the result of the operation
     */
    Status write(const std::string &key, const std::string &value) {
        return static_cast<Derived *>(this)->write_impl(key, value);
    }

    /**
     * @brief Scan for key-value pairs starting with a given prefix
     * @param key_prefix The initial key prefix to search for
     * @param limit Maximum number of key-value pairs to return
     * @param results Reference to store the results
     * @return Status code indicating the result of the operation
     */
    Status scan(const std::string &initial_key_prefix, size_t limit,
                std::vector<std::pair<std::string, std::string>> &results) {
        return static_cast<Derived *>(this)->scan_impl(initial_key_prefix,
                                                       limit, results);
    }

protected:
    /**
     * @brief Protected destructor (CRTP pattern)
     */
    ~PartitionedKeyValueStorage() = default;
};
