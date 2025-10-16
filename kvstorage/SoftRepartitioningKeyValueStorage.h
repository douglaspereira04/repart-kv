#pragma once

#include "RepartitioningKeyValueStorage.h"
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
#include <thread>
#include <chrono>
#include <optional>
#include <atomic>
#include <condition_variable>
#include <mutex>

/**
 * @brief Soft repartitioning key-value storage implementation
 * 
 * This class manages partitioned storage with dynamic repartitioning capabilities.
 * Uses a single storage engine with partition-level locking for soft repartitioning.
 * 
 * This implementation uses a single storage engine and partition-level locks
 * to provide non-disruptive repartitioning that preserves existing data access.
 * 
 * @tparam StorageEngineType The storage engine type (must derive from StorageEngine)
 * @tparam StorageMapType Template for key storage type for partition->engine mapping
 *         (e.g., MapKeyStorage). Will be instantiated with StorageEngineType* as value type.
 * @tparam PartitionMapType Template for key storage type for key->partition mapping
 *         (e.g., MapKeyStorage). Will be instantiated with size_t as value type.
 * @tparam HashFunc Hash function type for key hashing (defaults to std::hash<std::string>)
 * 
 * Usage example:
 *   SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage>
 * Instead of:
 *   SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage<MapStorageEngine*>, MapKeyStorage<size_t>>
 */
template<
    typename StorageEngineType,
    template<typename> typename StorageMapType,
    template<typename> typename PartitionMapType,
    typename HashFunc = std::hash<std::string>
>
class SoftRepartitioningKeyValueStorage 
    : public RepartitioningKeyValueStorage<
        SoftRepartitioningKeyValueStorage<StorageEngineType, StorageMapType, PartitionMapType, HashFunc>,
        StorageEngineType
      > {
private:
    PartitionMapType<size_t> partition_map_;           // Maps key ranges to partition IDs
    std::shared_mutex key_map_lock_;                   // Mutex for thread-safe access to key mappers
    bool enable_tracking_;                             // Enable/disable tracking of key access patterns
    std::atomic<bool> is_repartitioning_;              // Flag indicating if repartitioning is in progress
    size_t partition_count_;                           // Number of partitions
    StorageEngineType storage_;                 // Storage engine instance for the storage
    std::vector<std::unique_ptr<std::shared_mutex>> partition_locks_;   // Vector of partition locks
    HashFunc hash_func_;                               // Hash function for key hashing
    Graph graph_;                                      // Graph for tracking key access patterns and relationships
    MetisGraph metis_graph_;                           // METIS graph for partitioning

    std::mutex graph_lock_;                            // Mutex for thread-safe access to graph
    
    // Threading attributes for automatic repartitioning
    std::thread repartitioning_thread_;                // Background thread for automatic repartitioning
    std::optional<std::chrono::milliseconds> tracking_duration_;     // Duration to track key accesses
    std::optional<std::chrono::milliseconds> repartition_interval_;  // Interval between repartitioning cycles
    std::atomic<bool> running_;                        // Flag to control the repartitioning loop
    std::condition_variable cv_;                       // Condition variable to wake the thread
    std::mutex cv_mutex_;                              // Mutex for condition variable

public:
    /**
     * @brief Constructor
     * @param partition_count Number of partitions to manage
     * @param hash_func Hash function instance (defaults to default-constructed HashFunc)
     * @param tracking_duration Optional duration for tracking key accesses before repartitioning
     * @param repartition_interval Optional interval between repartitioning cycles
     */
    SoftRepartitioningKeyValueStorage(
        size_t partition_count, 
        const HashFunc& hash_func = HashFunc(),
        std::optional<std::chrono::milliseconds> tracking_duration = std::nullopt,
        std::optional<std::chrono::milliseconds> repartition_interval = std::nullopt)
        : enable_tracking_(false), 
          is_repartitioning_(false),
          partition_count_(partition_count), 
          storage_(StorageEngineType(0)),
          hash_func_(hash_func),
          tracking_duration_(tracking_duration),
          repartition_interval_(repartition_interval),
          running_(true) {
        // Create partition locks
        partition_locks_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            partition_locks_.emplace_back(std::make_unique<std::shared_mutex>());
        }
        
        // Start repartitioning thread if both durations are set
        if (tracking_duration_.has_value() && repartition_interval_.has_value()) {
            repartitioning_thread_ = std::thread(&SoftRepartitioningKeyValueStorage::repartition_loop, this);
        }
    }

    /**
     * @brief Destructor - cleans up the storage engine
     */
    ~SoftRepartitioningKeyValueStorage() {
        // Stop the repartitioning thread if it's running
        running_ = false;
        
        // Wake up the thread if it's sleeping
        cv_.notify_all();
        
        // Join the thread if it was started
        if (repartitioning_thread_.joinable()) {
            repartitioning_thread_.join();
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

        // Look up which partition owns this key
        size_t partition_idx;
        bool found = partition_map_.get(key, partition_idx);
        if (!found) {
            // Key not mapped yet - fall back to hash partitioning
            partition_idx = hash_func_(key) % partition_count_;
        }

        // Lock the partition for reading
        partition_locks_[partition_idx]->lock_shared();

        // Unlock key map (we have the storage lock now)
        key_map_lock_.unlock_shared();

        // Read value from storage
        std::string value = storage_.read(key);
        
        // Unlock partition
        partition_locks_[partition_idx]->unlock_shared();
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

        // Look up or assign partition for this key
        size_t partition_idx;
        bool found = partition_map_.get(key, partition_idx);
        if (!found) {
            // Key not mapped yet - fall back to hash partitioning
            partition_idx = hash_func_(key) % partition_count_;
            partition_map_.put(key, partition_idx);
        }
        // Lock the partition for writing
        partition_locks_[partition_idx]->lock();

        // Unlock key map (we have the partition lock now)
        key_map_lock_.unlock();

        // Write value to storage
        storage_.write(key, value);
        
        // Unlock partition
        partition_locks_[partition_idx]->unlock();
    }

    /**
     * @brief Scan for key-value pairs starting with a given prefix
     * @param initial_key_prefix The initial key prefix to search for
     * @param limit Maximum number of key-value pairs to return
     * @return Vector of key-value pairs matching the prefix
     */
    std::vector<std::pair<std::string, std::string>> scan_impl(const std::string& initial_key_prefix, size_t limit) {
        std::set<size_t> partition_set;
        std::vector<size_t> partition_array;
        std::vector<std::string> key_array;

        // Lock key map for reading
        key_map_lock_.lock_shared();

        // Get iterator starting from initial_key
        auto it = partition_map_.lower_bound(initial_key_prefix);
        
        // Collect storage pointers and keys up to limit
        while (limit > 0) {
            if (it.is_end()) {
                break;
            }
            
            size_t partition_idx = it.get_value();
            partition_set.insert(partition_idx);
            partition_array.push_back(partition_idx);
            key_array.push_back(it.get_key());
            
            ++it;
            limit--;
        }

        // Track key access patterns if enabled
        if (enable_tracking_) {
            multi_key_graph_update(key_array);
        }

        // Lock all unique partitions in sorted order
        std::vector<size_t> sorted_partitions(partition_set.begin(), partition_set.end());
        std::sort(sorted_partitions.begin(), sorted_partitions.end());
        
        for (size_t partition_idx : sorted_partitions) {
            partition_locks_[partition_idx]->lock_shared();
        }

        // Unlock key map
        key_map_lock_.unlock_shared();

        // Read values from storage
        std::vector<std::pair<std::string, std::string>> value_array;
        value_array.reserve(key_array.size());
        
        for (size_t i = 0; i < partition_array.size(); ++i) {
            std::string value = storage_.read(key_array[i]);
            value_array.push_back({key_array[i], value});
        }

        // Unlock all partitions
        for (size_t partition_idx : sorted_partitions) {
            partition_locks_[partition_idx]->unlock_shared();
        }

        return value_array;
    }

    /**
     * @brief Repartition data across available partitions (implementation)
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
     * 4. Update partition_map with new assignments
     * 
     * Note: This implementation does not migrate existing data. Data migration
     * will occur lazily as keys are accessed and reassigned to new partitions.
     */
    void repartition_impl() {
        // Step 1: Set repartitioning flag and disable tracking temporarily
        is_repartitioning_ = true;
        key_map_lock_.lock();
        enable_tracking_ = false;
        key_map_lock_.unlock();
        
        // Step 2: Partition the graph using METIS
        std::unordered_map<std::string, size_t> new_partition_map;
        std::vector<idx_t> metis_partitions;
        
        // Lock graph for reading and processing
        graph_lock_.lock();
        
        if (graph_.get_vertex_count() > 0) {
            try {
                metis_graph_.prepare_from_graph(graph_);
                metis_partitions = metis_graph_.partition(partition_count_);\
            } catch (const std::exception& e) {
                // If METIS fails, keep the old partition map
                // This can happen if the graph is too small or has other issues
            }
        }
        
        // Step 3: Lock and update partition assignments
        key_map_lock_.lock();
        
        for (size_t partition_idx = 0; partition_idx < partition_count_; ++partition_idx) {
            partition_locks_[partition_idx]->lock();
        }
        
        // Update partition_map with new assignments
        const auto& idx_to_vertex = metis_graph_.get_idx_to_vertex();
        for (size_t i = 0; i < metis_partitions.size(); ++i) {
            partition_map_.put(idx_to_vertex[i], static_cast<size_t>(metis_partitions[i]));
        }
        
        // Unlock all partitions
        for (size_t partition_idx = 0; partition_idx < partition_count_; ++partition_idx) {
            partition_locks_[partition_idx]->unlock();
        }

        // Unlock key map
        key_map_lock_.unlock();
        
        // Step 4: Clear the graph for fresh tracking
        graph_.clear();
        
        // Unlock graph after processing
        graph_lock_.unlock();
        
        // Step 5: Clear repartitioning flag
        is_repartitioning_ = false;
    }

    // Interface methods required by the abstract base class
    void enable_tracking_impl(bool enable) {
        enable_tracking_ = enable;
    }

    bool enable_tracking_impl() const {
        return enable_tracking_;
    }

    bool is_repartitioning_impl() const {
        return is_repartitioning_.load();
    }

    const Graph& graph_impl() const {
        return graph_;
    }

    void clear_graph_impl() {
        graph_lock_.lock();
        graph_.clear();
        graph_lock_.unlock();
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
        graph_lock_.lock();
        graph_.increment_vertex_weight(key);
        graph_lock_.unlock();
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
        graph_lock_.lock();
        
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
        
        graph_lock_.unlock();
    }

    /**
     * @brief Background thread loop for automatic repartitioning
     * 
     * This method runs in a separate thread and performs periodic repartitioning:
     * 1. Sleeps for repartition_interval
     * 2. Enables tracking for tracking_duration
     * 3. Calls repartition to redistribute keys based on access patterns
     * 
     * The loop continues until running_ is set to false (during destruction).
     */
    void repartition_loop() {
        while (running_) {
            // Sleep for the repartition interval
            std::unique_lock<std::mutex> lock(cv_mutex_);
            if (cv_.wait_for(lock, repartition_interval_.value(), [this]() { return !running_; })) {
                // If running_ became false, exit the loop
                break;
            }
            lock.unlock();
            
            // Enable tracking
            this->enable_tracking(true);
            
            // Sleep for the tracking duration
            lock.lock();
            if (cv_.wait_for(lock, tracking_duration_.value(), [this]() { return !running_; })) {
                // If running_ became false, exit the loop
                break;
            }
            lock.unlock();
            
            // Perform repartitioning (this also disables tracking)
            repartition_impl();
        }
    }
};

