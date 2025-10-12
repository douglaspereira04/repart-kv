#pragma once

#include "../storage/StorageEngine.h"
#include <string>
#include <vector>

/**
 * @brief Template class for managing partitioned storage
 * @tparam StorageEngineType Type that derives from StorageEngine
 */
template<typename StorageEngineType>
class PartitionedKeyValueStorage {
public:
    /**
     * @brief Default constructor
     */
    PartitionedKeyValueStorage() = default;

    /**
     * @brief Default destructor
     */
    ~PartitionedKeyValueStorage() = default;

    /**
     * @brief Read a value by key
     * @param key The key to read
     * @return The value associated with the key
     */
    std::string read(const std::string& key) {
        // TODO: Implement read logic
        return "";
    }

    /**
     * @brief Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     */
    void write(const std::string& key, const std::string& value) {
        // TODO: Implement write logic
    }

    /**
     * @brief Scan for keys starting with a given prefix
     * @param key_prefix The prefix to search for
     * @param limit Maximum number of keys to return
     * @return Vector of keys matching the prefix
     */
    std::vector<std::string> scan(const std::string& key_prefix, size_t limit) {
        // TODO: Implement scan logic
        return {};
    }
};

