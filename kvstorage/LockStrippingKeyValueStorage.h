#pragma once

#include "RepartitioningKeyValueStorage.h"
#include "../keystorage/KeyStorage.h"
#include "../storage/StorageEngine.h"
#include "../graph/Graph.h"
#include "../graph/MetisGraph.h"
#include "Tracker.h"
#include "storage/StorageEngineIterator.h"
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
 * @brief Lock stripping key-value storage implementation
 *
 * This class manages partitioned storage with static partitioning
 * capabilities. Uses a single storage engine for each partition.
 * Range scans require locking all partitions and scanning all of
 * them with the requested limit.
 *
 * @tparam StorageEngineType The storage engine type (must derive from
 * StorageEngine)
 * @tparam HashFunc Hash function type for key hashing (defaults to
 * std::hash<std::string>)
 */
template <typename StorageEngineType,
          typename HashFunc = std::hash<std::string>>
class LockStrippingKeyValueStorage
    : public PartitionedKeyValueStorage<
          LockStrippingKeyValueStorage<StorageEngineType, HashFunc>,
          StorageEngineType> {
private:
    size_t partition_count_; // Number of partitions
    std::vector<StorageEngineType *>
        storages_; // Vector of storage engine instances
    std::vector<std::unique_ptr<std::shared_mutex>>
        partition_locks_; // Vector of partition locks
    HashFunc hash_func_;  // Hash function for key hashing
    std::vector<std::string>
        paths_; // Paths for embedded database files (default: {/tmp})

    using IteratorType = typename StorageEngineType::IteratorType;

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
    LockStrippingKeyValueStorage(
        size_t partition_count, const HashFunc &hash_func = HashFunc(),
        const std::vector<std::string> &paths = {"/tmp"}) :
        partition_count_(partition_count), hash_func_(hash_func),
        paths_(paths.empty() ? std::vector<std::string>{"/tmp"} : paths) {

        // Create partition_count storage engine instances
        storages_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            storages_.push_back(new StorageEngineType(
                1,
                paths_[i % paths_.size()])); // Child storages at level + 1
        }

        // Create partition locks
        partition_locks_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            partition_locks_.emplace_back(
                std::make_unique<std::shared_mutex>());
        }
    }

    /**
     * @brief Destructor - cleans up dynamically allocated storage engines
     */
    ~LockStrippingKeyValueStorage() {
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
        size_t partition_idx = hash_func_(key) % partition_count_;
        partition_locks_[partition_idx]->lock_shared();
        Status status = storages_[partition_idx]->read(key, value);
        partition_locks_[partition_idx]->unlock_shared();
        return status;
    }

    /**
     * @brief Write a key-value pair (implementation for CRTP)
     * @param key The key to write
     * @param value The value to associate with the key
     * @return Status code indicating the result of the operation
     */
    Status write_impl(const std::string &key, const std::string &value) {
        size_t partition_idx = hash_func_(key) % partition_count_;
        partition_locks_[partition_idx]->lock();
        Status status = storages_[partition_idx]->write(key, value);
        partition_locks_[partition_idx]->unlock();
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

        Status status = Status::SUCCESS;
        // Read lock all storages
        for (size_t i = 0; i < partition_count_; ++i) {
            partition_locks_[i]->lock_shared();
        }

        results.clear();
        results.reserve(limit * partition_count_);

        for (size_t i = 0; i < partition_count_; ++i) {
            std::vector<std::pair<std::string, std::string>> partition_results;
            partition_results.reserve(limit);
            storages_[i]->scan(initial_key_prefix, limit, partition_results);
            results.insert(results.end(), partition_results.begin(),
                           partition_results.end());
        }

        if (results.empty()) {
            status = Status::NOT_FOUND;
        }

        std::sort(results.begin(), results.end());

        results.resize(std::min(results.size(), limit));

        // Unlock all storages
        for (size_t i = 0; i < partition_count_; ++i) {
            partition_locks_[i]->unlock_shared();
        }

        return status;
    }

    size_t operation_count_impl() const {
        size_t operation_count = 0;
        for (auto *storage : storages_) {
            operation_count += storage->operation_count();
        }
        return operation_count;
    }
};
