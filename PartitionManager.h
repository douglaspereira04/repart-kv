#pragma once

#include <string>
#include <vector>

/**
 * @brief Abstract base class for partition management
 * @tparam StorageEngineType The type of storage engine to manage
 */
template<typename StorageEngineType>
class PartitionManager {
protected:
    size_t _size;
    std::vector<StorageEngineType> _storage_engines;

public:
    /**
     * @brief Constructor
     * @param size Number of storage engines to create
     */
    explicit PartitionManager(size_t size) : _size(size), _storage_engines(size) {}

    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~PartitionManager() = default;

    /**
     * @brief Get storage engine for a given key
     * @param key The key to determine storage for
     * @return Reference to the storage engine to use for this key
     */
    virtual StorageEngineType& getStorage(const std::string& key) = 0;
};
