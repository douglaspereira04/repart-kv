#pragma once

#include "PartitionedKeyValueStorage.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief Inclusive start keys for uniform integer ranges over
 *        \c [0, n_records). Partition \c i owns
 *        \c [i * n_records / partition_count, (i+1) * n_records /
 * partition_count). The last partition also receives keys \c >= n_records
 * (inserts).
 *        \c n_records == 0 is treated as 1 so every start is \c "0".
 */
inline std::vector<std::string>
make_uniform_numeric_range_starts(size_t n_records, size_t partition_count) {
    if (partition_count == 0) {
        throw std::invalid_argument("partition_count must be greater than 0");
    }
    const size_t n = n_records == 0 ? 1 : n_records;
    std::vector<std::string> starts(partition_count);
    for (size_t i = 0; i < partition_count; ++i) {
        starts[i] = std::to_string((i * n) / partition_count);
    }
    return starts;
}

/**
 * @brief True when \a a should sort before \a b for range assignment.
 *
 * Integer keys (LoadGen uses \c std::to_string on record ids) compare
 * numerically so \c "9" < \c "10". Other keys fall back to lexicographic
 * order so mixed test strings still have a total order.
 */
inline bool range_key_less(const std::string &a, const std::string &b) {
    auto parse_uint = [](const std::string &s, uint64_t &out) -> bool {
        if (s.empty()) {
            return false;
        }
        for (unsigned char ch : s) {
            if (!std::isdigit(ch)) {
                return false;
            }
        }
        char *end = nullptr;
        const unsigned long long v = std::strtoull(s.c_str(), &end, 10);
        if (end != s.c_str() + s.size()) {
            return false;
        }
        out = static_cast<uint64_t>(v);
        return true;
    };

    uint64_t na = 0;
    uint64_t nb = 0;
    if (parse_uint(a, na) && parse_uint(b, nb)) {
        return na < nb;
    }
    return a < b;
}

/**
 * @brief Lock stripping with static range partitioning
 *
 * Analog of \c LockStrippingKeyValueStorage: one engine and one lock per
 * partition, no repartitioning. Keys are assigned by caller-provided range
 * starts rather than by hash. Point operations lock a single partition.
 * Scans lock and read from the start key's partition through the last
 * partition, then sort/truncate like hash lock stripping.
 *
 * @tparam StorageEngineTemplate Storage engine class template
 * @tparam STORAGE_SYNC Engine sync flag
 */
template <template <bool> class StorageEngineTemplate, bool STORAGE_SYNC>
class RangeLockStrippingKeyValueStorage
    : public PartitionedKeyValueStorage<
          RangeLockStrippingKeyValueStorage<StorageEngineTemplate,
                                            STORAGE_SYNC>,
          StorageEngineTemplate, STORAGE_SYNC> {
private:
    using StorageEngineType = StorageEngineTemplate<STORAGE_SYNC>;

    size_t partition_count_;
    std::vector<std::string> range_starts_;
    std::vector<StorageEngineType *> storages_;
    std::vector<std::unique_ptr<std::shared_mutex>> partition_locks_;
    std::vector<std::string> paths_;

public:
    /**
     * @brief Constructor
     * @param partition_count Number of partitions (must equal
     *        \p range_starts.size())
     * @param range_starts Inclusive start key of each partition, in any
     *        order; they are sorted with \c range_key_less. Partition \c i
     *        owns keys in \c [range_starts[i], range_starts[i+1]); the last
     *        partition owns \c [range_starts.back(), +inf). Keys before the
     *        first start are clamped to partition 0.
     * @param paths Directories for embedded database files (default: {/tmp})
     */
    RangeLockStrippingKeyValueStorage(
        size_t partition_count, std::vector<std::string> range_starts,
        const std::vector<std::string> &paths = {"/tmp"}) :
        partition_count_(partition_count),
        range_starts_(std::move(range_starts)),
        paths_(paths.empty() ? std::vector<std::string>{"/tmp"} : paths) {

        if (partition_count_ == 0) {
            throw std::invalid_argument(
                "partition_count must be greater than 0");
        }
        if (range_starts_.size() != partition_count_) {
            throw std::invalid_argument(
                "range_starts size must equal partition_count");
        }

        std::sort(range_starts_.begin(), range_starts_.end(), range_key_less);

        storages_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            storages_.push_back(
                new StorageEngineType(1, paths_[i % paths_.size()]));
        }

        partition_locks_.reserve(partition_count_);
        for (size_t i = 0; i < partition_count_; ++i) {
            partition_locks_.emplace_back(
                std::make_unique<std::shared_mutex>());
        }
    }

    ~RangeLockStrippingKeyValueStorage() {
        for (auto *storage : storages_) {
            delete storage;
        }
    }

    /**
     * @brief Partition that owns \p key (0 .. partition_count-1)
     */
    size_t partition_index(const std::string &key) const {
        auto it = std::upper_bound(range_starts_.begin(), range_starts_.end(),
                                   key, range_key_less);
        if (it == range_starts_.begin()) {
            return 0;
        }
        return static_cast<size_t>(std::distance(range_starts_.begin(), it)) -
               1;
    }

    const std::vector<std::string> &range_starts() const {
        return range_starts_;
    }

    Status read_impl(const std::string &key, std::string &value) {
        const size_t partition_idx = partition_index(key);
        partition_locks_[partition_idx]->lock_shared();
        Status status = storages_[partition_idx]->read(key, value);
        partition_locks_[partition_idx]->unlock_shared();
        return status;
    }

    Status write_impl(const std::string &key, const std::string &value) {
        const size_t partition_idx = partition_index(key);
        partition_locks_[partition_idx]->lock();
        Status status = storages_[partition_idx]->write(key, value);
        partition_locks_[partition_idx]->unlock();
        return status;
    }

    Status async_write_impl(const std::string &key, const std::string &value) {
        const size_t partition_idx = partition_index(key);
        partition_locks_[partition_idx]->lock();
        Status status = storages_[partition_idx]->async_write(key, value);
        partition_locks_[partition_idx]->unlock();
        return status;
    }

    Status force_sync_impl() {
        for (StorageEngineType *s : storages_) {
            if (s == nullptr) {
                return Status::ERROR;
            }
            Status st = s->force_sync();
            if (st != Status::SUCCESS) {
                return st;
            }
        }
        return Status::SUCCESS;
    }

    Status
    scan_impl(const std::string &initial_key_prefix, size_t limit,
              std::vector<std::pair<std::string, std::string>> &results) {

        Status status = Status::SUCCESS;
        const size_t first = partition_index(initial_key_prefix);

        for (size_t i = first; i < partition_count_; ++i) {
            partition_locks_[i]->lock_shared();
        }

        results.clear();
        results.reserve(limit * (partition_count_ - first));

        for (size_t i = first; i < partition_count_; ++i) {
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

        for (size_t i = first; i < partition_count_; ++i) {
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
