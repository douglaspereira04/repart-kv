#pragma once

#include "RepartitioningKeyValueStorage.h"
#include "../keystorage/KeyStorage.h"
#include "../storage/StorageEngine.h"
#include "../graph/Graph.h"
#include "../graph/MetisGraph.h"
#include "Tracker.h"
#include "storage/StorageEngineIterator.h"
#include <bitset>
#include <map>
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
 * capabilities. Uses a key-to-index map (StorageMapType) whose values hold
 * the owning storage engine pointer and partition index for each key.
 *
 * This implementation creates separate storage engines for each partition and
 * migrates data by creating new storage engines during repartitioning.
 *
 * @tparam StorageEngineTemplate Storage engine class template (e.g.
 *        \c MapStorageEngine)
 * @tparam STORAGE_SYNC Engine sync flag (\c
 * StorageEngineTemplate<STORAGE_SYNC>)
 * @tparam StorageMapType Template for key storage type for key->index
 * mapping (e.g., MapKeyStorage). Will be instantiated with Index* as value
 * type, where Index holds the storage engine pointer and partition index.
 * @tparam HashFunc Hash function type for key hashing (defaults to
 * std::hash<std::string>)
 *
 * Usage example:
 *   HardRepartitioningKeyValueStorage<MapStorageEngine, false, MapKeyStorage>
 */
template <template <bool> class StorageEngineTemplate, bool STORAGE_SYNC,
          template <typename> typename StorageMapType,
          typename HashFunc = std::hash<std::string>>
class HardRepartitioningKeyValueStorage
    : public RepartitioningKeyValueStorage<
          HardRepartitioningKeyValueStorage<StorageEngineTemplate, STORAGE_SYNC,
                                            StorageMapType, HashFunc>,
          StorageEngineTemplate, STORAGE_SYNC> {
private:
    using StorageEngineType = StorageEngineTemplate<STORAGE_SYNC>;

    StorageMapType<size_t>
        storage_map_; // Maps partition IDs to storage engines
    std::shared_mutex
        key_map_lock_;     // Mutex for thread-safe access to key mappers
    bool enable_tracking_; // Enable/disable tracking of key access patterns
    std::atomic<bool>
        is_repartitioning_;  // Flag indicating if repartitioning is in progress
    size_t partition_count_; // Number of partitions
    std::vector<StorageEngineType *>
        storages_; // Vector of storage engine instances
    size_t level_; // Current level (tree depth or hierarchy level)
    std::vector<std::unique_ptr<std::shared_mutex>>
        partition_locks_; // Vector of partition locks
    HashFunc hash_func_;  // Hash function for key hashing
    Tracker<> tracker_;   // Tracker for tracking key access patterns

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
    std::vector<std::string>
        paths_; // Paths for embedded database files (default: {/tmp})

    using IteratorType = typename StorageEngineType::IteratorType;

    static constexpr size_t MAX_PARTITION_COUNT =
        32; // Maximum number of partitions

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
     * @param paths Optional vector of paths for embedded database files
     * (default: {/tmp}) Each partition uses paths[i % paths.size()] to
     * distribute across paths
     */
    HardRepartitioningKeyValueStorage(
        size_t partition_count, const HashFunc &hash_func = HashFunc(),
        std::optional<std::chrono::milliseconds> tracking_duration =
            std::nullopt,
        std::optional<std::chrono::milliseconds> repartition_interval =
            std::nullopt,
        const std::vector<std::string> &paths = {"/tmp"}) :
        storage_map_(StorageMapType<size_t>(paths.empty() ? std::string("/tmp")
                                                          : paths[0])),
        enable_tracking_(false), is_repartitioning_(false),
        partition_count_(partition_count), level_(0), hash_func_(hash_func),
        tracking_duration_(tracking_duration),
        repartition_interval_(repartition_interval), running_(true),
        paths_(paths.empty() ? std::vector<std::string>{"/tmp"} : paths) {
        // Create partition_count storage engine instances
        storages_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            storages_.push_back(new StorageEngineType(
                level_,
                paths_[i % paths_.size()])); // Child storages at level + 1
        }

        // Create partition locks
        partition_locks_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            partition_locks_.emplace_back(
                std::make_unique<std::shared_mutex>());
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

        // Look up which storage owns this key
        size_t partition_idx;
        key_map_lock_.lock_shared();
        bool found = storage_map_.get(key, partition_idx);
        if (!found) {
            // Key not found in any storage
            key_map_lock_.unlock_shared();
            return Status::NOT_FOUND;
        }

        // Lock the partition for reading
        partition_locks_[partition_idx]->lock_shared();

        // Unlock key map (we have the storage lock now)
        key_map_lock_.unlock_shared();

        // Read value from storage
        Status status = storages_[partition_idx]->read(key, value);

        // Unlock storage
        partition_locks_[partition_idx]->unlock_shared();

        // Track key access if enabled
        if (enable_tracking_) {
            if (tracker_.update(key)) {
                enable_tracking_ = false;
            }
        }
        return status;
    }

    /**
     * @brief Write a key-value pair (implementation for CRTP)
     * @param key The key to write
     * @param value The value to associate with the key
     * @return Status code indicating the result of the operation
     */
    Status write_impl(const std::string &key, const std::string &value) {
        // Look up or assign storage for this key

        size_t partition_idx;

        key_map_lock_.lock();

        bool found_storage = storage_map_.get(key, partition_idx);

        if (!found_storage) {
            partition_idx = hash_func_(key) % partition_count_;
            storage_map_.put(key, partition_idx);
        }

        // Lock the partition for writing
        partition_locks_[partition_idx]->lock();

        // Unlock key map (we have the storage lock now)
        key_map_lock_.unlock();

        // Write value to storage
        Status status = storages_[partition_idx]->write(key, value);

        // Unlock storage
        partition_locks_[partition_idx]->unlock();

        // Track key access if enabled
        if (enable_tracking_) {
            if (tracker_.update(key)) {
                enable_tracking_ = false;
            }
        }
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
        std::bitset<MAX_PARTITION_COUNT> partition_bitset;

        // Get iterator starting from initial_key
        std::vector<std::pair<std::string, size_t>> key_index_pairs;
        key_map_lock_.lock_shared();
        storage_map_.scan(initial_key_prefix, limit, key_index_pairs);

        for (const auto &[key, partition_idx] : key_index_pairs) {
            partition_bitset.set(partition_idx);
        }

        for (size_t i = 0; i < partition_count_; ++i) {
            if (partition_bitset.test(i)) {
                partition_locks_[i]->lock_shared();
            }
        }

        key_map_lock_.unlock_shared();

        std::map<size_t, IteratorType> iterators;
        for (const auto &[key, partition_idx] : key_index_pairs) {
            iterators.try_emplace(partition_idx,
                                  storages_[partition_idx]->iterator());
        }

        // Read values from storages
        results.reserve(key_index_pairs.size());
        Status status = Status::NOT_FOUND;
        for (const auto &[key, partition_idx] : key_index_pairs) {
            std::string value;
            IteratorType &iterator = iterators.at(partition_idx);
            status = iterator.find(key, value);
            if (status != Status::SUCCESS) {
                break;
            }
            results.push_back({key, value});
        }

        iterators.clear();

        for (size_t i = 0; i < partition_count_; ++i) {
            if (partition_bitset.test(i)) {
                partition_locks_[i]->unlock_shared();
            }
        }

        // Track key access patterns if enabled
        if (enable_tracking_) {
            std::vector<std::string> keys;
            for (auto it = std::make_move_iterator(key_index_pairs.begin()),
                      end = std::make_move_iterator(key_index_pairs.end());
                 it != end; ++it) {
                keys.push_back(std::move(it->first));
            }
            if (tracker_.multi_update(keys)) {
                enable_tracking_ = false;
            }
        }

        return status;
    }

    void update_storage_map() {
        std::vector<idx_t> metis_partitions = tracker_.get_metis_partitions();
        const auto &idx_to_vertex = tracker_.get_idx_to_vertex();
        for (size_t i = 0; i < metis_partitions.size(); ++i) {
            size_t next_partition_idx =
                static_cast<size_t>(metis_partitions[i]);
            size_t partition_idx;
            key_map_lock_.lock();
            bool found = storage_map_.get(idx_to_vertex[i], partition_idx);
            if (found && next_partition_idx != partition_idx) {
                size_t curr_partition_idx = partition_idx;
                if (partition_idx <= next_partition_idx) {
                    partition_locks_[curr_partition_idx]->lock();
                    partition_locks_[next_partition_idx]->lock();
                } else {
                    partition_locks_[next_partition_idx]->lock();
                    partition_locks_[curr_partition_idx]->lock();
                }
                std::string value;
                Status status =
                    storages_[partition_idx]->remove(idx_to_vertex[i], value);
                if (status != Status::SUCCESS) {
                    partition_locks_[curr_partition_idx]->unlock();
                    partition_locks_[next_partition_idx]->unlock();
                    key_map_lock_.unlock();
                    throw std::runtime_error(
                        "Failed to read key from storage on repartitioning");
                }

                storage_map_.put(idx_to_vertex[i], next_partition_idx);
                key_map_lock_.unlock();

                storages_[next_partition_idx]->write(idx_to_vertex[i], value);

                partition_locks_[curr_partition_idx]->unlock();
                partition_locks_[next_partition_idx]->unlock();
            } else {
                key_map_lock_.unlock();
            }
        }
        // Lock the graph to clear it
        tracker_.lock_and_clear_graph();

        // Note that the queue was not cleared, thus, next repartitioning might
        // consider some realy old tracked keys
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

        bool success =
            tracker_.prepare_for_partition_map_update(partition_count_);

        if (success) {
            // Step 3: Lock and update partition assignments
            // Save old storages

            // Update partition_map with new assignments
            this->update_storage_map();

            // Create new storage engines
            // Increment level for new storage e
        }

        // Clear repartitioning flag
        is_repartitioning_ = false;
    }

    // Interface methods required by the abstract base class
    void enable_tracking_impl(bool enable) {
        enable_tracking_ = enable;
        if (!enable) {
            std::lock_guard<std::mutex> lock(cv_mutex_);
            cv_.notify_all();
        }
    }

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
     * 2. Enables tracking for tracking_duration (or until tracking is disabled
     *    externally via enable_tracking(false))
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

            // Sleep for the tracking duration, or until tracking is turned off
            lock.lock();
            (void)cv_.wait_for(lock, tracking_duration_.value(), [this]() {
                return !running_ || !enable_tracking_;
            });
            if (!running_) {
                lock.unlock();
                break;
            }
            lock.unlock();
            this->enable_tracking(false);

            // Perform repartitioning (this also disables tracking)
            this->repartition_impl();
        }
    }
};
