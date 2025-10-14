#pragma once

#include "PartitionedKeyValueStorage.h"
#include "../keystorage/KeyStorage.h"
#include "../storage/StorageEngine.h"
#include "../graph/Graph.h"
#include "../graph/MetisGraph.h"
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
    Graph graph_;                                      // Graph for tracking key access patterns and relationships
    MetisGraph metis_graph_;                           // METIS graph for partitioning

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
     * This method uses METIS graph partitioning to redistribute keys across
     * partitions based on their access patterns. Keys that are frequently
     * accessed together will be placed in the same partition to optimize
     * scan performance and minimize cross-partition operations.
     * 
     * Algorithm:
     * 1. Disable tracking
     * 2. Use METIS to partition the access pattern graph
     * 3. Clear the graph for fresh tracking
     * 4. Create new storage engines
     * 5. Update partition_map with new assignments
     * 
     * Note: This implementation does not migrate existing data. Data migration
     * will occur lazily as keys are accessed and reassigned to new partitions.
     */
    void repartition() {
        // Step 1: Disable tracking temporarily
        key_map_lock_.lock();
        enable_tracking_ = false;
        key_map_lock_.unlock();
        
        // Step 2: Partition the graph using METIS
        std::unordered_map<std::string, size_t> new_partition_map;
        
        if (graph_.get_vertex_count() > 0) {
            try {
                metis_graph_.prepare_from_graph(graph_);
                auto metis_partitions = metis_graph_.partition(partition_count_);
                
                // Convert partition IDs to size_t
                for (const auto& [key, partition_id] : metis_partitions) {
                    new_partition_map[key] = static_cast<size_t>(partition_id);
                }
            } catch (const std::exception& e) {
                // If METIS fails, keep the old partition map
                // This can happen if the graph is too small or has other issues
            }
        }
        
        // Step 3: Clear the graph for fresh tracking
        graph_.clear();
        
        // Step 4: Lock and update partition assignments
        key_map_lock_.lock();
        
        // Save old storages
        auto old_storages = storages_;
        
        // Lock all old storages in sorted order (by pointer address) to avoid deadlocks
        std::vector<StorageEngineType*> sorted_storages = old_storages;
        std::sort(sorted_storages.begin(), sorted_storages.end());
        
        for (auto* storage : sorted_storages) {
            storage->lock_shared();
        }
        
        // Update partition_map with new assignments
        if (!new_partition_map.empty()) {
            // Update partition_map_ for keys that were in the graph
            for (const auto& [key, new_partition_id] : new_partition_map) {
                partition_map_.put(key, new_partition_id);
            }
        }
        
        // Create new storage engines
        storages_.clear();
        storages_.reserve(partition_count_);
        // Increment level for new storage engines
        level_++;
        for (size_t i = 0; i < partition_count_; ++i) {
            storages_.push_back(new StorageEngineType(level_));
        }
        
        // Unlock all old storages
        for (auto* storage : sorted_storages) {
            storage->unlock_shared();
        }
        
        // Unlock key map
        key_map_lock_.unlock();
    }

    /**
     * @brief Enable or disable tracking of key access patterns
     * @param enable If true, enables tracking; if false, disables it
     */
    void enable_tracking(bool enable) {
        enable_tracking_ = enable;
    }

    /**
     * @brief Check if tracking is currently enabled
     * @return true if tracking is enabled, false otherwise
     */
    bool enable_tracking() const {
        return enable_tracking_;
    }

    /**
     * @brief Get a const reference to the access pattern graph
     * @return Const reference to the graph tracking key access patterns
     */
    const Graph& graph() const {
        return graph_;
    }

    /**
     * @brief Clear all tracking data from the graph
     */
    void clear_graph() {
        graph_.clear();
    }

private:
    /**
     * @brief Update graph structure for a single key
     * @param key The key whose graph relationships need to be updated
     * 
     * This method increments the vertex weight for the given key in the graph,
     * tracking its access frequency. If the vertex doesn't exist, it's created
     * with weight 1.
     */
    void single_key_graph_update(const std::string& key) {
        graph_.increment_vertex_weight(key);
    }

    /**
     * @brief Update graph structure for multiple keys
     * @param keys Vector of keys whose graph relationships need to be updated
     * 
     * This method updates the graph structure for multiple keys accessed together
     * (e.g., during a scan operation). It:
     * 1. Increments vertex weight for each key (tracking access frequency)
     * 2. Creates edges between all pairs of keys (tracking co-access patterns)
     * 
     * This helps identify keys that are frequently accessed together, which
     * should ideally be placed in the same partition for optimal performance.
     */
    void multi_key_graph_update(const std::vector<std::string>& keys) {
        // Add or increment vertex for each key
        for (const auto& key : keys) {
            graph_.increment_vertex_weight(key);
        }
        
        // Add or increment edges between all pairs of keys
        for (size_t i = 0; i < keys.size(); ++i) {
            for (size_t j = i + 1; j < keys.size(); ++j) {
                graph_.increment_edge_weight(keys[i], keys[j]);
            }
        }
    }
};

