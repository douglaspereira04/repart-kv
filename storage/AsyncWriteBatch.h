#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

/**
 * Pending relaxed writes for LSM engines. Callers must serialize writers on
 * a given engine instance (caller-owned MRSW). Data is copied so the caller's
 * key/value may die after async_write returns.
 */
struct AsyncWriteBatch {
    static constexpr size_t kMaxEntries = 512;
    static constexpr size_t kMaxBytes = 8ull * 1024 * 1024;

    std::vector<std::pair<std::string, std::string>> entries;
    size_t bytes = 0;

    void add(const std::string &key, const std::string &value) {
        bytes += key.size() + value.size();
        entries.emplace_back(key, value);
    }

    bool empty() const { return entries.empty(); }

    bool should_flush() const {
        return entries.size() >= kMaxEntries || bytes >= kMaxBytes;
    }

    void clear() {
        entries.clear();
        bytes = 0;
    }
};
