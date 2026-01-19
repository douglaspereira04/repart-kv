#pragma once

#include "../RepartitioningKeyValueStorage.h"
#include "../../keystorage/KeyStorage.h"
#include "../../storage/StorageEngine.h"
#include "../../graph/Graph.h"
#include "../../graph/MetisGraph.h"
#include "SoftPartitionWorker.h"
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
 * This class manages partitioned storage with dynamic repartitioning
 * capabilities. Uses a single storage engine with partition-level locking for
 * soft repartitioning.
 *
 * This implementation uses a single storage engine and partition-level locks
 * to provide non-disruptive repartitioning that preserves existing data access.
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
 *   SoftThreadedRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage,
 * MapKeyStorage> Instead of:
 *   SoftThreadedRepartitioningKeyValueStorage<MapStorageEngine,
 * MapKeyStorage<MapStorageEngine*>, MapKeyStorage<size_t>>
 */
template <typename StorageEngineType,
          template <typename> typename PartitionMapType,
          typename HashFunc = std::hash<std::string>, size_t Q = 1024 * 1024>
class SoftThreadedRepartitioningKeyValueStorage
    : public RepartitioningKeyValueStorage<
          SoftThreadedRepartitioningKeyValueStorage<StorageEngineType,
                                                    PartitionMapType, HashFunc>,
          StorageEngineType> {
private:
    PartitionMapType<size_t> key_map_; // Maps key ranges to partition IDs
    std::atomic_bool update_key_map_;  // Flag indicating if the partition map
                                       // should be updated
    std::shared_mutex
        key_map_lock_; // Mutex for thread-safe access to key mappers
    std::atomic_bool
        enable_tracking_; // Enable/disable tracking of key access patterns

    size_t partition_count_;    // Number of partitions
    StorageEngineType storage_; // Storage engine instance for the storage
    HashFunc hash_func_;        // Hash function for key hashing
    Graph graph_; // Graph for tracking key access patterns and relationships
    MetisGraph metis_graph_; // METIS graph for partitioning

    std::mutex graph_lock_; // Mutex for thread-safe access to graph

    // Threading attributes for automatic repartitioning
    std::thread repartitioning_thread_; // Background thread for automatic
                                        // repartitioning
    std::atomic<bool>
        is_repartitioning_; // Flag indicating if repartitioning is in progress
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
    std::vector<std::unique_ptr<SoftPartitionWorker<StorageEngineType, Q>>>
        workers_; // Workers for each partition

    std::atomic<bool> auto_repartitioning_; // Flag indicating if auto
                                            // repartitioning is enabled

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
    SoftThreadedRepartitioningKeyValueStorage(
        size_t partition_count, const HashFunc &hash_func = HashFunc(),
        std::optional<std::chrono::milliseconds> tracking_duration =
            std::nullopt,
        std::optional<std::chrono::milliseconds> repartition_interval =
            std::nullopt) :
        key_map_(PartitionMapType<size_t>()), update_key_map_(false),
        enable_tracking_(false), partition_count_(partition_count),
        storage_(StorageEngineType(0)), hash_func_(hash_func),
        is_repartitioning_(false), repartitioning_semaphore_(1),
        tracking_duration_(tracking_duration),
        repartition_interval_(repartition_interval), running_(true), workers_(),
        auto_repartitioning_(false) {

        // Create workers
        workers_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            workers_.emplace_back(
                std::make_unique<SoftPartitionWorker<StorageEngineType, Q>>(
                    storage_));
        }

        // Start repartitioning thread if both durations are set
        if (partition_count_ > 1 && tracking_duration_.has_value() &&
            repartition_interval_.has_value() &&
            tracking_duration_.value().count() > 0 &&
            repartition_interval_.value().count() > 0) {
            auto_repartitioning_ = true;
            repartitioning_thread_ = std::thread(
                &SoftThreadedRepartitioningKeyValueStorage::repartition_loop,
                this);
        }
    }

    /**
     * @brief Destructor - cleans up the storage engine
     */
    ~SoftThreadedRepartitioningKeyValueStorage() {
        // Stop the repartitioning thread if it's running
        running_ = false;

        // Wake up threads
        repartitioning_semaphore_.release();
        cv_.notify_all();

        // Join the thread if it was started
        if (repartitioning_thread_.joinable()) {
            repartitioning_thread_.join();
        }
    }

    /**
     * @brief Read a value by key (implementation for CRTP)
     * @param key The key to read
     * @param value Reference to store the value associated with the key
     * @return Status code indicating the result of the operation
     */
    Status read_impl(const std::string &key, std::string &value) {
        verify_and_update_key_map();
        // Lock key map for reading
        key_map_lock_.lock_shared();

        // Track key access if enabled
        if (enable_tracking_.load(std::memory_order_relaxed)) {
            single_key_graph_update(key);
        }

        // Look up which partition owns this key
        size_t partition_idx;
        bool found = key_map_.get(key, partition_idx);
        if (!found) {
            // If the key is not mapped it is not stored
            key_map_lock_.unlock_shared();
            return Status::NOT_FOUND;
        }

        ReadOperation read_operation(key, value);
        workers_[partition_idx]->read(&read_operation);

        // Unlock key map (we have the storage lock now)
        key_map_lock_.unlock_shared();

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
        verify_and_update_key_map();
        // Lock key map for writing
        key_map_lock_.lock();

        // Track key access if enabled
        if (enable_tracking_.load(std::memory_order_relaxed)) {
            single_key_graph_update(key);
        }

        // Look up or assign partition for this key
        size_t partition_idx;
        bool found = key_map_.get(key, partition_idx);
        if (!found) {
            // Key not mapped yet - fall back to hash partitioning
            partition_idx = hash_func_(key) % partition_count_;
            key_map_.put(key, partition_idx);
        }

        WriteOperation *write_operation = new WriteOperation(key, value);
        workers_[partition_idx]->write(write_operation);

        // Unlock key map (we have the partition lock now)
        key_map_lock_.unlock();

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

        verify_and_update_key_map();

        std::set<size_t> partition_set;
        std::vector<std::string> key_array;

        // Lock key map for reading
        key_map_lock_.lock_shared();

        // Get iterator starting from initial_key
        auto it = key_map_.lower_bound(initial_key_prefix);

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

            size_t partition_idx = it.get_value();
            partition_set.insert(partition_idx);
            key_array.push_back(it.get_key());

            ++it;
            ++count;
        }

        // Track key access patterns if enabled
        if (enable_tracking_.load(std::memory_order_relaxed)) {
            multi_key_graph_update(key_array);
        }

        results.resize(limit);
        ScanOperation scan_operation(initial_key_prefix, results,
                                     partition_set.size());
        for (size_t partition_idx : partition_set) {
            workers_[partition_idx]->enqueue(&scan_operation);
        }
        // Unlock key map
        key_map_lock_.unlock_shared();
        scan_operation.sync();

        return scan_operation.status();
    }

    /**
     * @brief Verify and update the key map
     *
     * This method verifies if the key map should be updated and updates it if
     * needed. It also releases the semaphore to allow the repartitioning thread
     * to proceed to the next repartitioning cycle.
     */
    void verify_and_update_key_map() {
        // We take a look at the update_key_map_ flag in a relaxed manner
        // if seems enabled, we verify with stronger memory order
        if (update_key_map_.load(std::memory_order_relaxed)) {
            if (update_key_map_) {

                // Lock key map so it can be changed safely
                key_map_lock_.lock();
                if (update_key_map_) {
                    update_key_map();
                    update_key_map_ = false;

                    // We release the semaphore to allow the repartitioning
                    // thread to proceed to the next repartitioning cycle
                    if (auto_repartitioning_) {
                        repartitioning_semaphore_.release();
                    }
                }

                // Submit Sync operation to all workers
                // This way, future enqueued operations will
                // be processed only after every previously
                // enqueued operations, to any worker, are processed
                // This voids multiple workers acting in the same partition
                SyncOperation *sync_operation =
                    new SyncOperation(partition_count_);
                for (size_t i = 0; i < partition_count_; ++i) {
                    workers_[i]->enqueue(sync_operation);
                }

                // Unlock key map
                key_map_lock_.unlock();
            }
        }
    }

    /**
     * @brief Update the key map with the new partition assignments
     */
    void update_key_map() {
        const auto &idx_to_vertex = metis_graph_.get_idx_to_vertex();
        std::vector<idx_t> metis_partitions =
            metis_graph_.get_partition_result();
        for (size_t i = 0; i < metis_partitions.size(); ++i) {
            key_map_.put(idx_to_vertex[i],
                         static_cast<size_t>(metis_partitions[i]));
        }
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
        // Set repartitioning flag to true (this flag is for testing purposes)
        is_repartitioning_ = true;

        // Tracking is disabled during repartitioning
        // and during the tracking_duration_ interval
        // Will be re-enabled before waiting for the next
        // repartitioning interval
        this->enable_tracking(false);

        // Step 1: Partition the graph using METIS
        std::unordered_map<std::string, size_t> new_partition_map;
        std::vector<idx_t> metis_partitions;

        // Lock graph for reading and processing
        graph_lock_.lock();

        if (graph_.get_vertex_count() > 0) {
            try {
                metis_graph_.prepare_from_graph(graph_);
                metis_graph_.partition(partition_count_);
                update_key_map_ = true;
            } catch (const std::exception &e) {
                // If METIS fails, keep the old partition map
                // This can happen if the graph is too small or has other issues
            }

            // Now metis_graph is available with improved key to partition
            // assignments. The next operation call will update the key map,
            // with the new assignments and re-start the repartitioning cycle.
        }

        // Step 2: Clear the graph for fresh tracking
        graph_.clear();

        // Unlock graph after processing
        graph_lock_.unlock();

        // Clear repartitioning flag (this flag is for testing purposes)
        is_repartitioning_ = false;
    }

    // Interface methods required by the abstract base class
    void enable_tracking_impl(bool enable) { enable_tracking_ = enable; }

    bool enable_tracking_impl() const { return enable_tracking_; }

    bool is_repartitioning_impl() const { return is_repartitioning_.load(); }

    const Graph &graph_impl() const { return graph_; }

    size_t operation_count_impl() const { return storage_.operation_count(); }

private:
    /**
     * @brief Update graph structure for a single key
     * @param key The key whose graph relationships need to be updated
     *
     * This method increments the vertex weight for the given key in the graph,
     * tracking its access frequency. If the vertex doesn't exist, it's created
     * with weight 1.
     */
    void single_key_graph_update(const std::string &key) {
        graph_lock_.lock();
        graph_.increment_vertex_weight(key);
        graph_lock_.unlock();
    }

    /**
     * @brief Update graph structure for multiple keys
     * @param keys Vector of keys whose graph relationships need to be updated
     *
     * This method updates the graph structure for multiple keys accessed
     * together (e.g., during a scan operation). It:
     * 1. Increments vertex weight for each key (tracking access frequency)
     * 2. Creates edges between all pairs of keys (tracking co-access patterns)
     *
     * This helps identify keys that are frequently accessed together, which
     * should ideally be placed in the same partition for optimal performance.
     */
    void multi_key_graph_update(const std::vector<std::string> &keys) {
        graph_lock_.lock();

        // Add or increment vertex for each key
        for (const auto &key : keys) {
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
