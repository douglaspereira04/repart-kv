#pragma once

#include "StorageEngineIterator.h"
#include "StorageEngine.h"
#include <tbb/concurrent_hash_map.h>
#include <string>
#include <vector>
#include <algorithm>

/**
 * @brief TBB concurrent_hash_map implementation of StorageEngine
 *
 * Provides a key-value storage using Intel TBB's concurrent_hash_map as the
 * underlying data structure. TBB's concurrent_hash_map provides fine-grained
 * locking and excellent concurrent performance.
 *
 * Key features:
 * - Thread-safe concurrent access without explicit locking
 * - Fine-grained locking (per bucket) for better concurrency
 * - Excellent performance for multi-threaded workloads
 * - In-memory storage (no persistence)
 *
 * Requires C++20 and Intel TBB library to be installed.
 *
 * Note: This class is thread-safe by design. The base class lock()/unlock()
 * methods are still available but not necessary for thread-safety.
 */
class TbbStorageEngine : public StorageEngine<TbbStorageEngine> {
private:
    // TBB concurrent_hash_map with string keys and values
    tbb::concurrent_hash_map<std::string, std::string> storage_;

public:
    /**
     * @brief Constructor
     * @param level The hierarchy level for this storage engine (default: 0)
     * @param path Optional path for embedded database files (default: /tmp)
     *              Note: This parameter is ignored for TbbStorageEngine as it
     *              doesn't use files, but is included for interface
     *              consistency.
     */
    explicit TbbStorageEngine(size_t level = 0,
                              const std::string &path = "/tmp") :
        StorageEngine<TbbStorageEngine>(level, path) {}

    /**
     * @brief Destructor
     */
    ~TbbStorageEngine() = default;

    /**
     * @brief Implementation: Read a value by key
     * @param key The key to read
     * @param value Reference to store the value associated with the key
     * @return Status code indicating the result of the operation
     */
    Status read_impl(const std::string &key, std::string &value) const {
        // TBB concurrent_hash_map uses accessor for thread-safe access
        tbb::concurrent_hash_map<std::string, std::string>::const_accessor
            accessor;
        if (storage_.find(accessor, key)) {
            value = accessor->second;
            return Status::SUCCESS;
        }
        return Status::NOT_FOUND;
    }

    /**
     * @brief Implementation: Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     * @return Status code indicating the result of the operation
     */
    Status write_impl(const std::string &key, const std::string &value) {
        // TBB concurrent_hash_map uses accessor for thread-safe access
        tbb::concurrent_hash_map<std::string, std::string>::accessor accessor;
        storage_.insert(accessor, key);
        accessor->second = value;
        return Status::SUCCESS;
    }

    /**
     * @brief Implementation: Scan for key-value pairs from a starting point
     * @param initial_key_prefix The starting key (lower_bound)
     * @param limit Maximum number of key-value pairs to return
     * @param results Reference to store the results of the scan
     * @return Status code indicating the result of the operation
     *
     * Note: TBB concurrent_hash_map is unordered, so we need to collect all
     * pairs, sort them, and filter by prefix.
     */
    Status
    scan_impl(const std::string &initial_key_prefix, size_t limit,
              std::vector<std::pair<std::string, std::string>> &results) const {
        // Collect all key-value pairs (concurrent_hash_map is unordered)
        std::vector<std::pair<std::string, std::string>> all_pairs;
        all_pairs.reserve(storage_.size());

        // Iterate through the concurrent_hash_map
        for (auto it = storage_.begin(); it != storage_.end(); ++it) {
            all_pairs.push_back({it->first, it->second});
        }

        // Sort all pairs by key
        std::sort(
            all_pairs.begin(), all_pairs.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

        // Find first key >= initial_key_prefix and collect up to limit
        results.clear();
        results.reserve(std::min(limit, all_pairs.size()));

        for (const auto &pair : all_pairs) {
            if (pair.first >= initial_key_prefix) {
                results.push_back(pair);
                if (results.size() >= limit) {
                    break;
                }
            }
        }

        if (results.empty()) {
            return Status::NOT_FOUND;
        }

        return Status::SUCCESS;
    }

    /**
     * @brief Get the number of records in the storage
     * @return Number of key-value pairs
     */
    size_t count() const { return storage_.size(); }

    /**
     * @brief Clear all entries from the storage
     */
    void clear() { storage_.clear(); }

    /**
     * @brief Remove a key from the storage
     * @param key The key to remove
     * @return Status code indicating the result of the operation
     */
    Status remove(const std::string &key) {
        if (storage_.erase(key)) {
            return Status::SUCCESS;
        }
        return Status::NOT_FOUND;
    }

    /**
     * @brief TBB concurrent_hash_map scan iterator for key lookups
     *
     * Provides the StorageEngineIterator interface. TBB concurrent_hash_map is
     * unordered (hash-based), so this iterator does not offer spatial locality
     * benefits; it uses find() per lookup. Thread-safe like the engine.
     */
    class TbbIterator
        : public StorageEngineIterator<TbbIterator, TbbStorageEngine> {
    public:
        explicit TbbIterator(TbbStorageEngine &engine) :
            StorageEngineIterator<TbbIterator, TbbStorageEngine>(engine) {}

        TbbIterator(const TbbIterator &) = delete;
        TbbIterator &operator=(const TbbIterator &) = delete;

        TbbIterator(TbbIterator &&other) noexcept :
            StorageEngineIterator<TbbIterator, TbbStorageEngine>(
                *other.engine_) {}

        TbbIterator &operator=(TbbIterator &&other) noexcept {
            if (this != &other) {
                engine_ = other.engine_;
            }
            return *this;
        }

        ~TbbIterator() = default;

        Status find_impl(const std::string &key, std::string &value) const {
            tbb::concurrent_hash_map<std::string, std::string>::const_accessor
                accessor;
            if (engine_->storage_.find(accessor, key)) {
                value = accessor->second;
                return Status::SUCCESS;
            }
            return Status::NOT_FOUND;
        }
    };

    TbbIterator iterator_impl() { return TbbIterator(*this); }
};
