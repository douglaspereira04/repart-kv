#pragma once

#include "../RepartitioningKeyValueStorage.h"
#include "../../keystorage/KeyStorage.h"
#include "../../storage/StorageEngine.h"
#include "../Tracker.h"
#include "HardPartitionWorker.h"
#include "operation/HardReadOperation.h"
#include "operation/HardWriteOperation.h"
#include "operation/HardScanOperation.h"
#include "operation/SyncOperation.h"
#include <string>
#include <vector>
#include <cstddef>
#include <shared_mutex>
#include <functional>
#include <set>
#include <map>
#include <algorithm>
#include <thread>
#include <chrono>
#include <optional>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <memory>

/**
 * @brief Hard threaded repartitioning key-value storage implementation
 *
 * This class manages partitioned storage with dynamic repartitioning
 * capabilities. Uses separate storage engines for each partition and
 * worker threads for asynchronous operation processing.
 *
 * This implementation combines features from HardRepartitioningKeyValueStorage
 * (separate storage engines per partition) with
 * SoftThreadedRepartitioningKeyValueStorage (worker threads for async
 * processing).
 *
 * @tparam StorageEngineType The storage engine type (must derive from
 * StorageEngine)
 * @tparam StorageMapType Template for key storage type for key->engine
 * mapping (e.g., MapKeyStorage). Will be instantiated with StorageEngineType*
 * as value type.
 * @tparam PartitionMapType Template for key storage type for key->partition
 * mapping (e.g., MapKeyStorage). Will be instantiated with size_t as value
 * type.
 * @tparam HashFunc Hash function type for key hashing (defaults to
 * std::hash<std::string>)
 * @tparam Q Maximum queue size for worker operations
 */
template <typename StorageEngineType,
          template <typename> typename StorageMapType,
          template <typename> typename PartitionMapType,
          typename HashFunc = std::hash<std::string>, size_t Q = 1024 * 1024>
class HardThreadedRepartitioningKeyValueStorage
    : public RepartitioningKeyValueStorage<
          HardThreadedRepartitioningKeyValueStorage<
              StorageEngineType, StorageMapType, PartitionMapType, HashFunc>,
          StorageEngineType> {
private:
    StorageMapType<StorageEngineType *>
        storage_map_; // Maps keys to storage engine instances
    PartitionMapType<size_t> partition_map_; // Maps keys to partition IDs
    std::shared_mutex
        key_map_lock_; // Mutex for thread-safe access to key mappers
    std::atomic_bool update_key_map_; // Flag indicating if the partition map
                                      // should be updated
    std::atomic_bool
        enable_tracking_; // Enable/disable tracking of key access patterns
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
    std::counting_semaphore<2> repartitioning_semaphore_;
    // Semaphore for repartitioning max of 2 because there might be already one
    // permit while calling the destructor
    std::optional<std::chrono::milliseconds>
        tracking_duration_; // Duration to track key accesses
    std::optional<std::chrono::milliseconds>
        repartition_interval_;   // Interval between repartitioning cycles
    std::atomic<bool> running_;  // Flag to control the repartitioning loop
    std::condition_variable cv_; // Condition variable to wake the thread
    std::mutex cv_mutex_;        // Mutex for condition variable
    std::vector<std::unique_ptr<HardPartitionWorker<StorageEngineType, Q>>>
        workers_; // Workers for each partition

    std::atomic<bool> auto_repartitioning_; // Flag indicating if auto
                                            // repartitioning is enabled
    std::vector<std::string>
        paths_; // Paths for embedded database files (default: {/tmp})

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
    HardThreadedRepartitioningKeyValueStorage(
        size_t partition_count, const HashFunc &hash_func = HashFunc(),
        std::optional<std::chrono::milliseconds> tracking_duration =
            std::nullopt,
        std::optional<std::chrono::milliseconds> repartition_interval =
            std::nullopt,
        const std::vector<std::string> &paths = {"/tmp"}) :
        update_key_map_(false), enable_tracking_(false),
        is_repartitioning_(false), partition_count_(partition_count), level_(0),
        hash_func_(hash_func), repartitioning_semaphore_(1),
        tracking_duration_(tracking_duration),
        repartition_interval_(repartition_interval), running_(true), workers_(),
        auto_repartitioning_(false),
        paths_(paths.empty() ? std::vector<std::string>{"/tmp"} : paths) {

        // Create partition_count storage engine instances
        storages_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            storages_.push_back(new StorageEngineType(
                level_ + 1,
                paths_[i % paths_.size()])); // Child storages at level + 1
        }

        // Create workers
        workers_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            workers_.emplace_back(
                std::make_unique<HardPartitionWorker<StorageEngineType, Q>>(i));
        }

        // Start repartitioning thread if both durations are set
        if (partition_count_ > 1 && tracking_duration_.has_value() &&
            repartition_interval_.has_value() &&
            tracking_duration_.value().count() > 0 &&
            repartition_interval_.value().count() > 0) {
            auto_repartitioning_ = true;
            repartitioning_thread_ = std::thread(
                &HardThreadedRepartitioningKeyValueStorage::repartition_loop,
                this);
        }
    }

    /**
     * @brief Destructor - cleans up dynamically allocated storage engines
     */
    ~HardThreadedRepartitioningKeyValueStorage() {
        // Stop the repartitioning thread if it's running
        running_ = false;

        // Wake up threads
        repartitioning_semaphore_.release();
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

        // Look up which partition owns this key
        size_t partition_idx;
        size_t next_partition_idx = hash_func_(key) % partition_count_;
        partition_map_.get_or_insert(key, next_partition_idx, partition_idx);

        HardReadOperation<StorageEngineType> read_operation(key, value,
                                                            storage);
        workers_[partition_idx]->enqueue(&read_operation);

        // Unlock key map (we have the storage lock now)
        key_map_lock_.unlock_shared();

        // Track key access if enabled
        if (enable_tracking_.load(std::memory_order_relaxed)) {
            tracker_.update(key);
        }

        // Read value from storage
        read_operation.wait();
        return read_operation.status();
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
        size_t partition_idx = 0;

        // Look up or assign storage for this key
        StorageEngineType *storage;

        size_t next_partition_idx = hash_func_(key) % partition_count_;
        StorageEngineType *next_storage = storages_[next_partition_idx];

        bool found_storage =
            storage_map_.get_or_insert(key, next_storage, storage);
        if (found_storage) {
            partition_map_.get(key, partition_idx);
        } else {
            partition_map_.get_or_insert(key, next_partition_idx,
                                         partition_idx);
        }

        if (storage->level() != level_) {
            // Storage is from a different level - reassign to current level
            storage = storages_[partition_idx];
            storage_map_.put(key, storage);
        }

        HardWriteOperation<StorageEngineType> *write_operation =
            new HardWriteOperation<StorageEngineType>(key, value, storage);
        workers_[partition_idx]->enqueue(write_operation);

        // Unlock key map (we have the partition lock now)
        key_map_lock_.unlock();

        // Track key access if enabled
        if (enable_tracking_.load(std::memory_order_relaxed)) {
            tracker_.update(key);
        }

        return Status::SUCCESS;
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

        std::set<size_t> partition_set;
        std::vector<size_t> partition_array;
        std::vector<std::string> key_array;
        std::vector<StorageEngineType *> storage_array;

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
            size_t partition_idx;
            size_t next_partition_idx =
                hash_func_(it.get_key()) % partition_count_;
            partition_map_.get_or_insert(it.get_key(), next_partition_idx,
                                         partition_idx);
            partition_set.insert(partition_idx);
            partition_array.push_back(partition_idx);
            storage_array.push_back(storage);
            key_array.push_back(it.get_key());

            ++it;
            ++count;
        }

        // Pre-populate results with pairs containing keys from key_array
        results.reserve(key_array.size());
        for (const auto &key : key_array) {
            results.push_back(
                {key, ""}); // Empty value, will be filled by scan operation
        }

        // Create a single scan operation with all storages and partition array
        // Each worker will scan the storage corresponding to its partition
        // index
        HardScanOperation<StorageEngineType> scan_operation(
            initial_key_prefix, results, partition_set.size(), storage_array,
            partition_array);
        for (size_t partition_idx : partition_set) {
            workers_[partition_idx]->enqueue(&scan_operation);
        }
        // Unlock key map
        key_map_lock_.unlock_shared();

        // Track key access patterns if enabled
        if (enable_tracking_.load(std::memory_order_relaxed)) {
            tracker_.multi_update(key_array);
        }

        scan_operation.sync();

        return scan_operation.status();
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

            // Update partition_map with new assignments
            tracker_.update_partition_map(partition_map_);

            // Create new storage engines
            storages_.clear();
            storages_.reserve(partition_count_);
            // Increment level for new storage engines
            level_++;
            for (size_t i = 0; i < partition_count_; ++i) {
                storages_.push_back(
                    new StorageEngineType(level_, paths_[i % paths_.size()]));
            }

            // Unlock key map
            key_map_lock_.unlock();
        }

        // Clear repartitioning flag
        is_repartitioning_ = false;
    }

    // Interface methods required by the abstract base class
    void enable_tracking_impl(bool enable) { enable_tracking_ = enable; }

    bool enable_tracking_impl() const { return enable_tracking_.load(); }

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
            // Wait for the semaphore to be released
            // The loop interval is repartition_interval_ plus the
            // time between the repartition thread signaling that
            // a new key map is available and one thread making an
            // operation detecting that the new key map is available,
            // effectively completing the repartitioning
            repartitioning_semaphore_.acquire();

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

            // Perform repartitioning
            repartition_impl();
        }
    }
};
