#pragma once

#include "RepartitioningKeyValueStorage.h"
#include "../keystorage/KeyStorage.h"
#include "../storage/StorageEngine.h"
#include "../graph/Graph.h"
#include "../graph/MetisGraph.h"
#include "Tracker.h"
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
 * @brief Hard repartitioning key-value storage implementation
 *
 * This class manages partitioned storage with dynamic repartitioning
 * capabilities. Uses two key-value maps:
 * - StorageMapType: Maps keys to storage engine instances
 * - PartitionMapType: Maps key ranges to partition IDs
 *
 * This implementation creates separate storage engines for each partition and
 * migrates data by creating new storage engines during repartitioning.
 *
 * @tparam StorageEngineType The storage engine type (must derive from
 * StorageEngine)
 * @tparam StorageMapType Template for key storage type for partition->engine
 * mapping (e.g., MapKeyStorage). Will be instantiated with StorageEngineType*
 * as value type.
 * @tparam PartitionMapType Template for key storage type for key->partition
 * mapping (e.g., MapKeyStorage). Will be instantiated with size_t as value
 * type.
 * @tparam HashFunc Hash function type for key hashing (defaults to
 * std::hash<std::string>)
 *
 * Usage example:
 *   HardRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage,
 * MapKeyStorage> Instead of:
 *   HardRepartitioningKeyValueStorage<MapStorageEngine,
 * MapKeyStorage<MapStorageEngine*>, MapKeyStorage<size_t>>
 */
template <typename StorageEngineType,
          template <typename> typename StorageMapType,
          template <typename> typename PartitionMapType,
          typename HashFunc = std::hash<std::string>>
class HardRepartitioningKeyValueStorage
    : public RepartitioningKeyValueStorage<
          HardRepartitioningKeyValueStorage<StorageEngineType, StorageMapType,
                                            PartitionMapType, HashFunc>,
          StorageEngineType> {
private:
    StorageMapType<StorageEngineType *>
        storage_map_; // Maps partition IDs to storage engines
    PartitionMapType<size_t> partition_map_; // Maps key ranges to partition IDs
    std::shared_mutex
        key_map_lock_;     // Mutex for thread-safe access to key mappers
    bool enable_tracking_; // Enable/disable tracking of key access patterns
    std::atomic<bool>
        is_repartitioning_;  // Flag indicating if repartitioning is in progress
    size_t partition_count_; // Number of partitions
    std::vector<StorageEngineType *>
        storages_;       // Vector of storage engine instances
    size_t level_;       // Current level (tree depth or hierarchy level)
    HashFunc hash_func_; // Hash function for key hashing
    Tracker<PartitionMapType<size_t>>
        tracker_; // Tracker for tracking key access patterns

    // Threading attributes for automatic repartitioning
    std::thread repartitioning_thread_; // Background thread for automatic
                                        // repartitioning
    std::optional<std::chrono::milliseconds>
        tracking_duration_; // Duration to track key accesses
    std::optional<std::chrono::milliseconds>
        repartition_interval_;   // Interval between repartitioning cycles
    std::atomic<bool> running_;  // Flag to control the repartitioning loop
    std::condition_variable cv_; // Condition variable to wake the thread
    std::mutex cv_mutex_;        // Mutex for condition variable

public:
    /**
     * @brief Constructor
     * @param partition_count Number of partitions to manage
     * @param hash_func Hash function instance (defaults to default-constructed
     * HashFunc)
     * @param tracking_duration Optional duration for tracking key accesses
     * before repartitioning
     * @param repartition_interval Optional interval between repartitioning
     * cycles
     */
    HardRepartitioningKeyValueStorage(
        size_t partition_count, const HashFunc &hash_func = HashFunc(),
        std::optional<std::chrono::milliseconds> tracking_duration =
            std::nullopt,
        std::optional<std::chrono::milliseconds> repartition_interval =
            std::nullopt) :
        enable_tracking_(false), is_repartitioning_(false),
        partition_count_(partition_count), level_(0), hash_func_(hash_func),
        tracking_duration_(tracking_duration),
        repartition_interval_(repartition_interval), running_(true) {
        // Create partition_count storage engine instances
        storages_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            storages_.push_back(new StorageEngineType(
                level_ + 1)); // Child storages at level + 1
        }

        // Start repartitioning thread if both durations are set
        if (partition_count_ > 1 && tracking_duration_.has_value() &&
            repartition_interval_.has_value() &&
            tracking_duration_.value().count() > 0 &&
            repartition_interval_.value().count() > 0) {
            repartitioning_thread_ = std::thread(
                &HardRepartitioningKeyValueStorage::repartition_loop, this);
        }
    }

    /**
     * @brief Destructor - cleans up dynamically allocated storage engines
     */
    ~HardRepartitioningKeyValueStorage() {
        // Stop the repartitioning thread if it's running
        running_ = false;

        // Wake up the thread if it's sleeping
        cv_.notify_all();

        // Join the thread if it was started
        if (repartitioning_thread_.joinable()) {
            repartitioning_thread_.join();
        }

        // Clean up storage engines
        for (auto *storage : storages_) {
            delete storage;
        }
    }

    /**
     * @brief Read a value by key (implementation for CRTP)
     * @param key The key to read
     * @param value Reference to store the value associated with the key
     * @return Status code indicating the result of the operation
     */
    Status read_impl(const std::string &key, std::string &value) {
        // Lock key map for reading
        key_map_lock_.lock_shared();

        // Look up which storage owns this key
        StorageEngineType *storage;
        bool found = storage_map_.get(key, storage);
        if (!found) {
            // Key not found in any storage
            key_map_lock_.unlock_shared();
            return Status::NOT_FOUND;
        }

        // Lock the storage for reading
        storage->lock_shared();

        // Unlock key map (we have the storage lock now)
        key_map_lock_.unlock_shared();

        // Track key access if enabled
        if (enable_tracking_) {
            tracker_.update(key);
        }

        // Read value from storage
        Status status = storage->read(key, value);

        // Unlock storage
        storage->unlock_shared();
        return status;
    }

    /**
     * @brief Write a key-value pair (implementation for CRTP)
     * @param key The key to write
     * @param value The value to associate with the key
     * @return Status code indicating the result of the operation
     */
    Status write_impl(const std::string &key, const std::string &value) {
        // Lock key map for writing
        key_map_lock_.lock();

        // Look up or assign storage for this key
        StorageEngineType *storage;
        bool found = storage_map_.get(key, storage);
        if (!found) {
            // Key not mapped yet - assign to a partition using hash function
            size_t partition_idx = hash_func_(key) % partition_count_;
            storage = storages_[partition_idx];
            storage_map_.put(key, storage);
        } else if (storage->level() != level_) {
            // Storage is from a different level - reassign to current level
            size_t partition_idx;
            bool found = partition_map_.get(key, partition_idx);
            if (!found) {
                // No partition mapping - assign using hash function
                partition_idx = hash_func_(key) % partition_count_;
                partition_map_.put(key, partition_idx);
            }
            storage = storages_[partition_idx];
            storage_map_.put(key, storage);
        }

        // Lock the storage for writing
        storage->lock();

        // Unlock key map (we have the storage lock now)
        key_map_lock_.unlock();

        // Track key access if enabled
        if (enable_tracking_) {
            tracker_.update(key);
        }

        // Write value to storage
        Status status = storage->write(key, value);

        // Unlock storage
        storage->unlock();
        return status;
    }

    /**
     * @brief Scan for key-value pairs starting with a given prefix
     * @param initial_key_prefix The initial key prefix to search for
     * @param limit Maximum number of key-value pairs to return
     * @param results Reference to store the results
     * @return Status code indicating the result of the operation
     */
    Status
    scan_impl(const std::string &initial_key_prefix, size_t limit,
              std::vector<std::pair<std::string, std::string>> &results) {
        std::set<StorageEngineType *> storage_set;
        std::vector<StorageEngineType *> storage_array;
        std::vector<std::string> key_array;

        // Lock key map for reading
        key_map_lock_.lock_shared();

        // Get iterator starting from initial_key
        auto it = storage_map_.lower_bound(initial_key_prefix);

        size_t count = 0;
        // Collect storage pointers and keys up to limit
        while (count < limit) {
            if (it.is_end()) {
                if (count == 0) {
                    key_map_lock_.unlock_shared();
                    return Status::NOT_FOUND;
                }
                break;
            }

            StorageEngineType *storage = it.get_value();
            storage_set.insert(storage);
            storage_array.push_back(storage);
            key_array.push_back(it.get_key());

            ++it;
            ++count;
        }

        // Lock all unique storages in sorted order (by pointer address)
        std::vector<StorageEngineType *> sorted_storages(storage_set.begin(),
                                                         storage_set.end());
        std::sort(sorted_storages.begin(), sorted_storages.end());

        for (auto *storage : sorted_storages) {
            storage->lock_shared();
        }

        // Unlock key map
        key_map_lock_.unlock_shared();

        // Read values from storages
        results.reserve(key_array.size());
        Status status;
        for (size_t i = 0; i < storage_array.size(); ++i) {
            std::string value;
            status = storage_array[i]->read(key_array[i], value);
            if (status != Status::SUCCESS) {
                break;
            }
            results.push_back({key_array[i], value});
        }

        // Unlock all storages
        for (auto *storage : sorted_storages) {
            storage->unlock_shared();
        }

        // Track key access patterns if enabled
        if (enable_tracking_) {
            tracker_.multi_update(key_array);
        }

        return status;
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
     * 4. Create new storage engines
     * 5. Update partition_map with new assignments
     *
     * Note: This implementation does not migrate existing data. Data migration
     * will occur lazily as keys are accessed and reassigned to new partitions.
     */
    void repartition_impl() {
        // Set repartitioning flag and disable tracking temporarily
        is_repartitioning_ = true;
        enable_tracking_ = false;

        bool success =
            tracker_.prepare_for_partition_map_update(partition_count_);

        if (success) {
            // Step 3: Lock and update partition assignments
            key_map_lock_.lock();

            // Save old storages
            auto old_storages = storages_;

            // Lock all old storages in sorted order (by pointer address) to
            // avoid deadlocks
            std::vector<StorageEngineType *> sorted_storages = old_storages;
            std::sort(sorted_storages.begin(), sorted_storages.end());

            for (auto *storage : sorted_storages) {
                storage->lock();
            }

            // Update partition_map with new assignments
            tracker_.update_partition_map(partition_map_);

            // Create new storage engines
            storages_.clear();
            storages_.reserve(partition_count_);
            // Increment level for new storage engines
            level_++;
            for (size_t i = 0; i < partition_count_; ++i) {
                storages_.push_back(new StorageEngineType(level_));
            }

            // Unlock all old storages
            for (auto *storage : sorted_storages) {
                storage->unlock();
            }

            // Unlock key map
            key_map_lock_.unlock();
        }

        // Clear repartitioning flag
        is_repartitioning_ = false;
    }

    // Interface methods required by the abstract base class
    void enable_tracking_impl(bool enable) { enable_tracking_ = enable; }

    bool enable_tracking_impl() const { return enable_tracking_; }

    bool is_repartitioning_impl() const { return is_repartitioning_.load(); }

    const Graph &graph_impl() const { return tracker_.graph(); }

    size_t operation_count_impl() const {
        size_t operation_count = 0;
        for (auto *storage : storages_) {
            operation_count += storage->operation_count();
        }
        return operation_count;
    }

private:
    /**
     * @brief Background thread loop for automatic repartitioning
     *
     * This method runs in a separate thread and performs periodic
     * repartitioning:
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
            if (cv_.wait_for(lock, repartition_interval_.value(),
                             [this]() { return !running_; })) {
                // If running_ became false, exit the loop
                break;
            }
            lock.unlock();

            // Enable tracking
            this->enable_tracking(true);

            // Sleep for the tracking duration
            lock.lock();
            if (cv_.wait_for(lock, tracking_duration_.value(),
                             [this]() { return !running_; })) {
                // If running_ became false, exit the loop
                break;
            }
            lock.unlock();

            // Perform repartitioning (this also disables tracking)
            repartition_impl();
        }
    }
};
