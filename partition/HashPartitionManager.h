#pragma once

#include "PartitionManager.h"
#include <functional>

/**
 * @brief Hash-based partition manager that distributes keys across storage engines
 * @tparam StorageEngineType The type of storage engine to manage
 */
template<typename StorageEngineType>
class HashPartitionManager : public PartitionManager<StorageEngineType> {
public:
    /**
     * @brief Constructor
     * @param size Number of storage engines to create
     */
    explicit HashPartitionManager(size_t size) : PartitionManager<StorageEngineType>(size) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~HashPartitionManager() = default;

    /**
     * @brief Get storage engine for a given key using hash modulo
     * @param key The key to determine storage for
     * @return Reference to the storage engine to use for this key
     */
    StorageEngineType& getStorage(const std::string& key) override {
        // Use fast hash function (not cryptographically secure)
        size_t hash_value = std::hash<std::string>{}(key);
        size_t index = hash_value % this->_size;
        
        // Add key to storage mapping
        this->__key_to_storage[key] = this->_storage_engines[index];
        
        return this->_storage_engines[index];
    }
};
