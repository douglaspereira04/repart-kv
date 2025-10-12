#pragma once

#include "StorageEngine.h"
#include <map>
#include <string>
#include <vector>
#include <algorithm>

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

public:
    /**
     * @brief Default constructor
     */
    MapStorageEngine() = default;

    /**
     * @brief Destructor
     */
    ~MapStorageEngine() = default;

    /**
     * @brief Implementation: Read a value by key
     * @param key The key to read
     * @return The value associated with the key, or empty string if not found
     */
    std::string read_impl(const std::string& key) const {
        auto it = storage_.find(key);
        if (it != storage_.end()) {
            return it->second;
        }
        return "";
    }

    /**
     * @brief Implementation: Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     */
    void write_impl(const std::string& key, const std::string& value) {
        storage_[key] = value;
    }

    /**
     * @brief Implementation: Scan for keys from a starting point
     * @param key_prefix The starting key (lower_bound)
     * @param limit Maximum number of keys to return
     * @return Vector of keys >= key_prefix
     */
    std::vector<std::string> scan_impl(const std::string& key_prefix, size_t limit) const {
        std::vector<std::string> results;
        results.reserve(std::min(limit, storage_.size()));

        // Use lower_bound to find the first key >= key_prefix
        auto it = storage_.lower_bound(key_prefix);
        
        // Iterate and collect keys
        while (it != storage_.end() && results.size() < limit) {
            results.push_back(it->first);
            ++it;
        }

        return results;
    }
};

