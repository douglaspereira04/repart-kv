#pragma once

#include "PartitionedKeyValueStorage.h"
#include "../keystorage/KeyStorage.h"
#include "../storage/StorageEngine.h"
#include <string>
#include <vector>
#include <cstddef>
#include <shared_mutex>
#include <functional>
#include <set>
#include <algorithm>

/**
 * @brief Repartitioning key-value storage implementation
 * 
 * This class manages partitioned storage with dynamic repartitioning capabilities.
 * Uses two key-value maps:
 * - StorageMapType: Maps keys to storage engine instances
 * - PartitionMapType: Maps key ranges to partition IDs
 * 
 * @tparam StorageEngineType The storage engine type (must derive from StorageEngine)
 * @tparam StorageMapType Template for key storage type for partition->engine mapping
 *         (e.g., MapKeyStorage). Will be instantiated with StorageEngineType* as value type.
 * @tparam PartitionMapType Template for key storage type for key->partition mapping
 *         (e.g., MapKeyStorage). Will be instantiated with size_t as value type.
 * @tparam HashFunc Hash function type for key hashing (defaults to std::hash<std::string>)
 * 
 * Usage example:
 *   RepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage>
 * Instead of:
 *   RepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage<MapStorageEngine*>, MapKeyStorage<size_t>>
 */
template<
    typename StorageEngineType,
    template<typename> typename StorageMapType,
    template<typename> typename PartitionMapType,
    typename HashFunc = std::hash<std::string>
>
class RepartitioningKeyValueStorage 
    : public PartitionedKeyValueStorage<
        RepartitioningKeyValueStorage<StorageEngineType, StorageMapType, PartitionMapType, HashFunc>,
        StorageEngineType
      > {
private:
    StorageMapType<StorageEngineType*> storage_map_;   // Maps partition IDs to storage engines
    PartitionMapType<size_t> partition_map_;           // Maps key ranges to partition IDs
    std::shared_mutex key_map_lock_;                   // Mutex for thread-safe access to key mappers
    bool enable_tracking_;                             // Enable/disable tracking of key access patterns
    size_t partition_count_;                           // Number of partitions
    std::vector<StorageEngineType*> storages_;         // Vector of storage engine instances
    size_t level_;                                     // Current level (tree depth or hierarchy level)
    HashFunc hash_func_;                               // Hash function for key hashing

public:
    /**
     * @brief Constructor
     * @param partition_count Number of partitions to manage
     * @param hash_func Hash function instance (defaults to default-constructed HashFunc)
     */
    RepartitioningKeyValueStorage(size_t partition_count, const HashFunc& hash_func = HashFunc())
        : enable_tracking_(false), partition_count_(partition_count), level_(0), hash_func_(hash_func) {
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
        // Lock key map for reading
        key_map_lock_.lock_shared();

        // Track key access if enabled
        if (enable_tracking_) {
            single_key_graph_update(key);
        }

        // Look up which storage owns this key
        StorageEngineType* storage;
        bool found = storage_map_.get(key, storage);
        if (!found) {
            // Key not found in any partition
            key_map_lock_.unlock_shared();
            return "";
        }
        
        // Lock the storage for reading
        storage->lock_shared();

        // Unlock key map (we have the storage lock now)
        key_map_lock_.unlock_shared();

        // Read value from storage
        std::string value = storage->read(key);
        
        // Unlock storage
        storage->unlock_shared();
        return value;
    }

    /**
     * @brief Write a key-value pair (implementation for CRTP)
     * @param key The key to write
     * @param value The value to associate with the key
     */
    void write_impl(const std::string& key, const std::string& value) {
        // Lock key map for writing
        key_map_lock_.lock();

        // Track key access if enabled
        if (enable_tracking_) {
            single_key_graph_update(key);
        }

        // Look up or assign storage for this key
        StorageEngineType* storage;
        bool found = storage_map_.get(key, storage);
        if (!found) {
            // Key not mapped yet - assign to a partition using hash function
            size_t partition_idx = hash_func_(key) % partition_count_;
            storage = storages_[partition_idx];
            storage_map_.put(key, storage);
            partition_map_.put(key, partition_idx);
        } else if(storage->level() != level_) {
            // Storage is from a different level - reassign to current level
            size_t partition_idx;
            bool found = partition_map_.get(key, partition_idx);
            if (!found) {
                // No partition mapping - assign using hash function
                partition_idx = hash_func_(key) % partition_count_;
                partition_map_.put(key, partition_idx);
                storage = storages_[partition_idx];
                storage_map_.put(key, storage);
            } else {
                // Use existing partition mapping
                storage = storages_[partition_idx];
            }
        }

        // Lock the storage for writing
        storage->lock();

        // Unlock key map (we have the storage lock now)
        key_map_lock_.unlock();

        // Write value to storage
        storage->write(key, value);
        
        // Unlock storage
        storage->unlock();
    }

    /**
     * @brief Scan for key-value pairs starting with a given prefix (implementation for CRTP)
     * @param key_prefix The prefix to search for
     * @param limit Maximum number of key-value pairs to return
     * @return Vector of key-value pairs matching the prefix
     */
    std::vector<std::pair<std::string, std::string>> scan_impl(const std::string& key_prefix, size_t limit) {
        std::set<StorageEngineType*> storage_set;
        std::vector<StorageEngineType*> storage_array;
        std::vector<std::string> key_array;

        // Lock key map for reading
        key_map_lock_.lock_shared();

        // Get iterator starting from initial_key
        auto it = storage_map_.lower_bound(key_prefix);
        
        // Collect storage pointers and keys up to limit
        while (limit > 0) {
            if (it.is_end()) {
                break;
            }
            
            StorageEngineType* storage = it.get_value();
            storage_set.insert(storage);
            storage_array.push_back(storage);
            key_array.push_back(it.get_key());
            
            ++it;
            limit--;
        }

        // Track key access patterns if enabled
        if (enable_tracking_) {
            multi_key_graph_update(key_array);
        }

        // Lock all unique storages in sorted order (by pointer address)
        std::vector<StorageEngineType*> sorted_storages(storage_set.begin(), storage_set.end());
        std::sort(sorted_storages.begin(), sorted_storages.end());
        
        for (auto* storage : sorted_storages) {
            storage->lock_shared();
        }

        // Unlock key map
        key_map_lock_.unlock_shared();

        // Read values from storages
        std::vector<std::pair<std::string, std::string>> value_array;
        value_array.reserve(key_array.size());
        
        for (size_t i = 0; i < storage_array.size(); ++i) {
            std::string value = storage_array[i]->read(key_array[i]);
            value_array.push_back({key_array[i], value});
        }

        // Unlock all storages
        for (auto* storage : sorted_storages) {
            storage->unlock_shared();
        }

        return value_array;
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

    /**
     * @brief Update graph structure for a single key
     * @param key The key whose graph relationships need to be updated
     * 
     * This method updates the graph structure (edges, relationships) associated
     * with a single key, potentially affecting partition assignments.
     */
    void single_key_graph_update(const std::string& key) {
        // TODO: Implement single key graph update logic
        // 1. Analyze key relationships and access patterns
        // 2. Update graph structure for this key
        // 3. Potentially trigger repartitioning if needed
    }

    /**
     * @brief Update graph structure for multiple keys
     * @param keys Vector of keys whose graph relationships need to be updated
     * 
     * This method updates the graph structure (edges, relationships) for multiple
     * keys in batch, which can be more efficient than individual updates.
     */
    void multi_key_graph_update(const std::vector<std::string>& keys) {
        // TODO: Implement multi-key graph update logic
        // 1. Analyze relationships between the provided keys
        // 2. Update graph structure for all keys
        // 3. Optimize partition assignments based on key relationships
        // 4. Potentially trigger batch repartitioning
    }
};

