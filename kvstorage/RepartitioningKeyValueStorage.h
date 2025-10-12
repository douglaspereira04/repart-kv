#pragma once

#include "PartitionedKeyValueStorage.h"
#include "../keystorage/KeyStorage.h"
#include "../storage/StorageEngine.h"
#include <string>
#include <vector>
#include <cstddef>
#include <shared_mutex>

/**
 * @brief Repartitioning key-value storage implementation
 * 
 * This class manages partitioned storage with dynamic repartitioning capabilities.
 * Uses two key-value maps:
 * - StorageMapType: Maps partition IDs to storage engine instances
 * - PartitionMapType: Maps key ranges to partition IDs
 * 
 * @tparam StorageEngineType The storage engine type (must derive from StorageEngine)
 * @tparam StorageMapType Template for key storage type for partition->engine mapping
 *         (e.g., MapKeyStorage). Will be instantiated with StorageEngineType* as value type.
 * @tparam PartitionMapType Template for key storage type for key->partition mapping
 *         (e.g., MapKeyStorage). Will be instantiated with size_t as value type.
 * 
 * Usage example:
 *   RepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage>
 * Instead of:
 *   RepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage<MapStorageEngine*>, MapKeyStorage<size_t>>
 */
template<
    typename StorageEngineType,
    template<typename> typename StorageMapType,
    template<typename> typename PartitionMapType
>
class RepartitioningKeyValueStorage 
    : public PartitionedKeyValueStorage<
        RepartitioningKeyValueStorage<StorageEngineType, StorageMapType, PartitionMapType>,
        StorageEngineType
      > {
private:
    StorageMapType<StorageEngineType*> storage_map_;   // Maps partition IDs to storage engines
    PartitionMapType<size_t> partition_map_;           // Maps key ranges to partition IDs
    std::shared_mutex storage_map_lock_;               // Mutex for thread-safe access to storage_map_
    bool enable_tracking_;                             // Enable/disable tracking of key access patterns
    size_t partition_count_;                           // Number of partitions
    std::vector<StorageEngineType*> storages_;         // Vector of storage engine instances
    size_t level_;                                     // Current level (tree depth or hierarchy level)

public:
    /**
     * @brief Constructor
     * @param partition_count Number of partitions to manage
     */
    RepartitioningKeyValueStorage(size_t partition_count)
        : enable_tracking_(false), partition_count_(partition_count), level_(0) {
        // Create partition_count storage engine instances
        storages_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            storages_.push_back(new StorageEngineType(level_ + 1));  // Child storages at level + 1
        }
    }

    /**
     * @brief Destructor - cleans up dynamically allocated storage engines
     */
    ~RepartitioningKeyValueStorage() {
        for (auto* storage : storages_) {
            delete storage;
        }
    }

    /**
     * @brief Read a value by key (implementation for CRTP)
     * @param key The key to read
     * @return The value associated with the key
     */
    std::string read_impl(const std::string& key) {
        
        return "";
    }

    /**
     * @brief Write a key-value pair (implementation for CRTP)
     * @param key The key to write
     * @param value The value to associate with the key
     */
    void write_impl(const std::string& key, const std::string& value) {
        // TODO: Implement write logic
        // 1. Use partition_map_ to find which partition should own this key
        // 2. Use storage_map_ to get the storage engine for that partition
        // 3. Call write() on that storage engine
    }

    /**
     * @brief Scan for key-value pairs starting with a given prefix (implementation for CRTP)
     * @param key_prefix The prefix to search for
     * @param limit Maximum number of key-value pairs to return
     * @return Vector of key-value pairs matching the prefix
     */
    std::vector<std::pair<std::string, std::string>> scan_impl(const std::string& key_prefix, size_t limit) {
        // TODO: Implement scan logic
        // 1. Use partition_map_ to find all partitions that might contain keys with this prefix
        // 2. Collect results from all relevant partitions using storage_map_
        // 3. Merge and sort results across partitions
        // 4. Return up to 'limit' results
        return {};
    }

    /**
     * @brief Repartition data across available partitions
     * 
     * This method redistributes data across partitions, potentially
     * moving keys from one storage engine to another for better load balancing.
     */
    void repartition() {
        // TODO: Implement repartitioning logic
        // 1. Analyze current key distribution
        // 2. Determine optimal partition boundaries
        // 3. Move keys between partitions as needed
        // 4. Update partition_map_ with new partition assignments
    }
};

