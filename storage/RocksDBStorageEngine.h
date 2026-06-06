#pragma once

#include "StorageEngineIterator.h"
#include "StorageEngine.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/write_batch.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <filesystem>

/**
 * @brief RocksDB-based implementation of StorageEngine
 *
 * Uses Meta's RocksDB for sorted key-value storage. RocksDB is an LSM-tree
 * based key-value store (LevelDB fork) that provides:
 * - Keys stored in sorted order (lexicographic)
 * - Efficient range queries and prefix scans
 * - Good write throughput with batching
 * - Persistence to disk
 *
 * Requires C++20 and librocksdb-dev to be installed.
 *
 * Key features:
 * - Keys are stored in sorted order
 * - Efficient range queries via iterator Seek()
 * - Good for scan-heavy and write-heavy workloads
 *
 * Note: This class is NOT thread-safe by default. Users must manually
 * call lock()/unlock() or lock_shared()/unlock_shared() when needed.
 *
 * @tparam SYNC When true, all writes use WriteOptions.sync for durability.
 */
template <bool SYNC = false> class RocksDBStorageEngine
    : public StorageEngine<RocksDBStorageEngine<SYNC>, SYNC> {
private:
    std::unique_ptr<rocksdb::DB> db_;
    bool is_open_;
    std::string db_path_;
    static std::atomic_int db_counter_;
    static std::string id_;

    static rocksdb::WriteOptions durable_write_options() {
        rocksdb::WriteOptions o;
        o.sync = SYNC;
        return o;
    }

    static rocksdb::WriteOptions async_write_options() {
        rocksdb::WriteOptions o;
        o.sync = false;
        return o;
    }

public:
    /**
     * @brief Constructor - creates a temporary database
     * @param level The hierarchy level for this storage engine (default: 0)
     * @param path Optional path for database files (default: /tmp)
     *
     * Note: RocksDB requires a file path. Uses a temporary directory
     * for in-memory-like behavior.
     */
    explicit RocksDBStorageEngine(size_t level = 0,
                                  const std::string &path = "/tmp") :
        StorageEngine<RocksDBStorageEngine<SYNC>, SYNC>(level, path),
        db_(nullptr), is_open_(false) {
        std::string temp_path =
            this->path_ + std::string("/repart_kv_storage/") + id_ +
            std::string("/rocksdb_temp_") +
            std::to_string(db_counter_.fetch_add(1, std::memory_order_relaxed));
        std::filesystem::create_directories(
            this->path_ + std::string("/repart_kv_storage/") + id_);

        rocksdb::Options options;
        options.create_if_missing = true;
        options.error_if_exists = false;

        rocksdb::DB *db = nullptr;
        rocksdb::Status status = rocksdb::DB::Open(options, temp_path, &db);
        if (status.ok() && db) {
            db_.reset(db);
            db_path_ = temp_path;
            is_open_ = true;
        }
    }

    /**
     * @brief Constructor with file path - creates a persistent database
     * @param file_path Path to the database directory
     * @param level The hierarchy level for this storage engine (default: 0)
     * @param path Optional path for database files (default: /tmp)
     */
    explicit RocksDBStorageEngine(const std::string &file_path,
                                  size_t level = 0,
                                  const std::string &path = "/tmp") :
        StorageEngine<RocksDBStorageEngine<SYNC>, SYNC>(level, path),
        db_(nullptr), is_open_(false), db_path_(file_path) {

        rocksdb::Options options;
        options.create_if_missing = true;

        rocksdb::DB *db = nullptr;
        rocksdb::Status status = rocksdb::DB::Open(options, file_path, &db);
        if (status.ok() && db) {
            db_.reset(db);
            is_open_ = true;
        }
    }

    /**
     * @brief Destructor - closes the database
     */
    ~RocksDBStorageEngine() {
        if (is_open_ && db_) {
            db_.reset();
            is_open_ = false;
        }
    }

    // Disable copy
    RocksDBStorageEngine(const RocksDBStorageEngine &) = delete;
    RocksDBStorageEngine &operator=(const RocksDBStorageEngine &) = delete;

    // Enable move
    RocksDBStorageEngine(RocksDBStorageEngine &&other) noexcept :
        StorageEngine<RocksDBStorageEngine<SYNC>, SYNC>(other.level_,
                                                        other.path_),
        db_(std::move(other.db_)), is_open_(other.is_open_),
        db_path_(std::move(other.db_path_)) {
        other.is_open_ = false;
    }

    RocksDBStorageEngine &operator=(RocksDBStorageEngine &&other) noexcept {
        if (this != &other) {
            if (is_open_ && db_) {
                db_.reset();
            }
            db_ = std::move(other.db_);
            is_open_ = other.is_open_;
            db_path_ = std::move(other.db_path_);
            other.is_open_ = false;
        }
        return *this;
    }

    /**
     * @brief Implementation: Read a value by key
     */
    Status read_impl(const std::string &key, std::string &value) const {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), key, &value);
        if (status.ok()) {
            return Status::SUCCESS;
        }
        if (status.IsNotFound()) {
            return Status::NOT_FOUND;
        }
        return Status::ERROR;
    }

    /**
     * @brief Implementation: Write a key-value pair
     */
    Status write_impl(const std::string &key, const std::string &value) {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        rocksdb::Status status = db_->Put(durable_write_options(), key, value);
        if (status.ok()) {
            return Status::SUCCESS;
        }
        return Status::ERROR;
    }

    Status async_write_impl(const std::string &key, const std::string &value) {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        rocksdb::Status status = db_->Put(async_write_options(), key, value);
        if (status.ok()) {
            return Status::SUCCESS;
        }

        return Status::ERROR;
    }

    Status force_sync_impl() {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        rocksdb::WriteBatch batch;
        rocksdb::WriteOptions o;
        o.sync = true;
        rocksdb::Status status = db_->Write(o, &batch);
        return status.ok() ? Status::SUCCESS : Status::ERROR;
    }

    /**
     * @brief Implementation: Scan for key-value pairs from a starting point
     *
     * RocksDB maintains sorted order, making this efficient.
     * Uses Seek() to go to the first key >= initial_key_prefix.
     */
    Status
    scan_impl(const std::string &initial_key_prefix, size_t limit,
              std::vector<std::pair<std::string, std::string>> &results) const {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }

        results.clear();
        results.reserve(std::min(limit, static_cast<size_t>(1000)));

        rocksdb::ReadOptions read_options;
        std::unique_ptr<rocksdb::Iterator> iter(db_->NewIterator(read_options));

        if (initial_key_prefix.empty()) {
            iter->SeekToFirst();
        } else {
            iter->Seek(initial_key_prefix);
        }

        size_t count = 0;
        while (iter->Valid() && count < limit) {
            std::string key = iter->key().ToString();
            std::string value = iter->value().ToString();
            results.emplace_back(std::move(key), std::move(value));
            ++count;
            iter->Next();
        }

        if (count == 0) {
            return Status::NOT_FOUND;
        }
        return Status::SUCCESS;
    }

    /**
     * @brief Scan and append key-value pairs to existing results
     */
    Status
    scan_append(const std::string &key_start, size_t limit,
                std::vector<std::pair<std::string, std::string>> &results) {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }

        rocksdb::ReadOptions read_options;
        std::unique_ptr<rocksdb::Iterator> iter(db_->NewIterator(read_options));

        if (key_start.empty()) {
            iter->SeekToFirst();
        } else {
            iter->Seek(key_start);
        }

        size_t count = 0;
        while (iter->Valid() && count < limit) {
            std::string key = iter->key().ToString();
            std::string value = iter->value().ToString();
            results.emplace_back(std::move(key), std::move(value));
            ++count;
            iter->Next();
        }

        if (count == 0) {
            return Status::NOT_FOUND;
        }
        return Status::SUCCESS;
    }

    /**
     * @brief Check if the database is open
     */
    bool is_open() const { return is_open_; }

    /**
     * @brief Get the number of records in the database
     */
    int64_t count() const {
        if (!is_open_ || !db_) {
            return 0;
        }
        rocksdb::ReadOptions read_options;
        std::unique_ptr<rocksdb::Iterator> iter(db_->NewIterator(read_options));
        int64_t n = 0;
        for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
            ++n;
        }
        return n;
    }

    /**
     * @brief Synchronize the database to storage
     */
    bool sync() {
        if (!is_open_ || !db_) {
            return false;
        }
        if constexpr (SYNC) {
            rocksdb::WriteBatch batch;
            return db_->Write(durable_write_options(), &batch).ok();
        }
        return true;
    }

    /**
     * @brief Clear all entries from the database
     */
    void clear() {
        if (!is_open_ || !db_) {
            return;
        }
        rocksdb::ReadOptions read_options;
        std::unique_ptr<rocksdb::Iterator> iter(db_->NewIterator(read_options));
        rocksdb::WriteBatch batch;
        for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
            batch.Delete(iter->key());
        }
        db_->Write(durable_write_options(), &batch);
    }

    /**
     * @brief Implementation: Remove a key and return the stored value
     */
    Status remove_impl(const std::string &key, std::string &removed_value) {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        rocksdb::Status get_status =
            db_->Get(rocksdb::ReadOptions(), key, &removed_value);
        if (get_status.IsNotFound()) {
            return Status::NOT_FOUND;
        }
        if (!get_status.ok()) {
            return Status::ERROR;
        }
        rocksdb::Status del_status = db_->Delete(durable_write_options(), key);
        if (del_status.ok()) {
            return Status::SUCCESS;
        }
        return Status::ERROR;
    }

    /**
     * @brief RocksDB scan iterator for locality-optimized key lookups
     */
    class RocksDBIterator
        : public StorageEngineIterator<RocksDBIterator,
                                       RocksDBStorageEngine<SYNC>> {
    private:
        std::unique_ptr<rocksdb::Iterator> iter_;

    public:
        explicit RocksDBIterator(RocksDBStorageEngine &engine) :
            StorageEngineIterator<RocksDBIterator, RocksDBStorageEngine<SYNC>>(
                engine) {
            if (engine.is_open_ && engine.db_) {
                iter_ = std::unique_ptr<rocksdb::Iterator>(
                    engine.db_->NewIterator(rocksdb::ReadOptions()));
            }
        }

        RocksDBIterator(const RocksDBIterator &) = delete;
        RocksDBIterator &operator=(const RocksDBIterator &) = delete;

        RocksDBIterator(RocksDBIterator &&other) noexcept :
            StorageEngineIterator<RocksDBIterator, RocksDBStorageEngine<SYNC>>(
                *other.engine_),
            iter_(std::move(other.iter_)) {}

        RocksDBIterator &operator=(RocksDBIterator &&other) noexcept {
            if (this != &other) {
                this->engine_ = other.engine_;
                iter_ = std::move(other.iter_);
            }
            return *this;
        }

        ~RocksDBIterator() = default;

        Status find_impl(const std::string &key, std::string &value) const {
            if (!iter_ || !this->engine_->is_open_) {
                return Status::ERROR;
            }
            iter_->Seek(key);
            if (!iter_->Valid()) {
                return Status::NOT_FOUND;
            }
            if (iter_->key().ToString() != key) {
                return Status::NOT_FOUND;
            }
            value = iter_->value().ToString();
            return Status::SUCCESS;
        }
    };

    /**
     * @brief Implementation: create and return a RocksDB scan iterator
     */
    RocksDBIterator iterator_impl() { return RocksDBIterator(*this); }

    using IteratorType = RocksDBIterator;
};

// Static member definitions
template <bool SYNC> std::atomic_int RocksDBStorageEngine<SYNC>::db_counter_ =
    0;
template <bool SYNC> std::string RocksDBStorageEngine<SYNC>::id_ =
    std::to_string(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count());
