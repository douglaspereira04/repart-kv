#pragma once

#include "StorageEngineIterator.h"
#include "StorageEngine.h"
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <shared_mutex>

/**
 * @brief Simple std::map implementation of StorageEngine
 *
 * Provides a key-value storage using std::map as the underlying
 * data structure. Uses CRTP for zero-overhead abstraction.
 * Requires C++20 for concepts and CRTP pattern.
 *
 * Note: This class is NOT thread-safe by default. Users must manually
 * call lock()/unlock() or lock_shared()/unlock_shared() when needed.
 */
class MapStorageEngine : public StorageEngine<MapStorageEngine> {
private:
    std::map<std::string, std::string> storage_;
    mutable std::shared_mutex lock_;

public:
    /**
     * @brief Constructor
     * @param level The hierarchy level for this storage engine (default: 0)
     * @param path Optional path for embedded database files (default: /tmp)
     *              Note: This parameter is ignored for MapStorageEngine as it
     *              doesn't use files, but is included for interface
     * consistency.
     */
    explicit MapStorageEngine(size_t level = 0,
                              const std::string &path = "/tmp") :
        StorageEngine<MapStorageEngine>(level, path) {}

    /**
     * @brief Destructor
     */
    ~MapStorageEngine() = default;

    /**
     * @brief Implementation: Read a value by key
     * @param key The key to read
     * @return The value associated with the key, or empty string if not found
     */
    Status read_impl(const std::string &key, std::string &value) const {
        lock_.lock_shared();
        auto it = storage_.find(key);
        if (it != storage_.end()) {
            value = it->second;
            lock_.unlock_shared();
            return Status::SUCCESS;
        }
        lock_.unlock_shared();
        return Status::NOT_FOUND;
    }

    /**
     * @brief Implementation: Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     */
    Status write_impl(const std::string &key, const std::string &value) {
        lock_.lock();
        storage_[key] = value;
        lock_.unlock();
        return Status::SUCCESS;
    }

    /**
     * @brief Implementation: Scan for key-value pairs from a starting point
     * @param initial_key_prefix The starting key (lower_bound)
     * @param limit Maximum number of key-value pairs to return
     * @return Vector of key-value pairs where keys >= initial_key_prefix
     */
    Status
    scan_impl(const std::string &initial_key_prefix, size_t limit,
              std::vector<std::pair<std::string, std::string>> &results) const {
        lock_.lock_shared();
        results.reserve(std::min(limit, storage_.size()));

        // Use lower_bound to find the first key >= key_prefix
        auto it = storage_.lower_bound(initial_key_prefix);

        // Iterate and collect key-value pairs
        results.resize(limit);
        size_t i = 0;
        while (it != storage_.end() && i < limit) {
            results[i] = {it->first, it->second};
            ++i;
            ++it;
        }

        if (i < limit) {
            results.resize(i);
            if (i == 0) {
                lock_.unlock_shared();
                return Status::NOT_FOUND;
            }
        }

        lock_.unlock_shared();
        return Status::SUCCESS;
    }

    /**
     * @brief std::map scan iterator for key lookups
     *
     * Provides the StorageEngineIterator interface with shared locking for
     * thread-safety. std::map is ordered; the iterator uses find() for lookups.
     */
    class MapIterator
        : public StorageEngineIterator<MapIterator, MapStorageEngine> {
    public:
        explicit MapIterator(MapStorageEngine &engine) :
            StorageEngineIterator<MapIterator, MapStorageEngine>(engine) {}

        MapIterator(const MapIterator &) = delete;
        MapIterator &operator=(const MapIterator &) = delete;

        MapIterator(MapIterator &&other) noexcept :
            StorageEngineIterator<MapIterator, MapStorageEngine>(
                *other.engine_) {}

        MapIterator &operator=(MapIterator &&other) noexcept {
            if (this != &other) {
                engine_ = other.engine_;
            }
            return *this;
        }

        ~MapIterator() = default;

        Status find_impl(const std::string &key, std::string &value) const {
            engine_->lock_.lock_shared();
            auto it = engine_->storage_.find(key);
            if (it != engine_->storage_.end()) {
                value = it->second;
                engine_->lock_.unlock_shared();
                return Status::SUCCESS;
            }
            engine_->lock_.unlock_shared();
            return Status::NOT_FOUND;
        }
    };

    MapIterator iterator_impl() { return MapIterator(*this); }
};
