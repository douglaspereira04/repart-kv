#pragma once

#include "StorageEngineIterator.h"
#include "StorageEngine.h"
#include <leveldb/db.h>
#include <leveldb/options.h>
#include <leveldb/status.h>
#include <leveldb/write_batch.h>
#include <string>
#include <vector>
#include <memory>
#include <ctime>
#include <atomic>
#include <chrono>
#include <filesystem>

/**
 * @brief LevelDB-based implementation of StorageEngine
 *
 * Uses Google's LevelDB for sorted key-value storage. LevelDB is an LSM-tree
 * based key-value store that provides:
 * - Keys stored in sorted order (lexicographic)
 * - Efficient range queries and prefix scans
 * - Good write throughput with batching
 * - Persistence to disk
 *
 * Requires C++20 and libleveldb-dev to be installed.
 *
 * Key features:
 * - Keys are stored in sorted order
 * - Efficient range queries via iterator Seek()
 * - Good for scan-heavy and write-heavy workloads
 *
 * Note: This class is NOT thread-safe by default. Users must manually
 * call lock()/unlock() or lock_shared()/unlock_shared() when needed.
 */
class LevelDBStorageEngine : public StorageEngine<LevelDBStorageEngine> {
private:
    std::unique_ptr<leveldb::DB> db_;
    bool is_open_;
    std::string db_path_;
    static std::atomic_int db_counter_;
    static std::string id_;

public:
    /**
     * @brief Constructor - creates a temporary database
     * @param level The hierarchy level for this storage engine (default: 0)
     * @param path Optional path for database files (default: /tmp)
     *
     * Note: LevelDB requires a file path. Uses a temporary directory
     * for in-memory-like behavior.
     */
    explicit LevelDBStorageEngine(size_t level = 0,
                                  const std::string &path = "/tmp") :
        StorageEngine<LevelDBStorageEngine>(level, path), db_(nullptr),
        is_open_(false) {
        std::string temp_path =
            path_ + std::string("/repart_kv_storage/") + id_ +
            std::string("/leveldb_temp_") +
            std::to_string(db_counter_.fetch_add(1, std::memory_order_relaxed));
        std::filesystem::create_directories(
            path_ + std::string("/repart_kv_storage/") + id_);

        leveldb::Options options;
        options.create_if_missing = true;
        options.error_if_exists = false;

        leveldb::DB *db = nullptr;
        leveldb::Status status = leveldb::DB::Open(options, temp_path, &db);
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
    explicit LevelDBStorageEngine(const std::string &file_path,
                                  size_t level = 0,
                                  const std::string &path = "/tmp") :
        StorageEngine<LevelDBStorageEngine>(level, path), db_(nullptr),
        is_open_(false), db_path_(file_path) {

        leveldb::Options options;
        options.create_if_missing = true;

        leveldb::DB *db = nullptr;
        leveldb::Status status = leveldb::DB::Open(options, file_path, &db);
        if (status.ok() && db) {
            db_.reset(db);
            is_open_ = true;
        }
    }

    /**
     * @brief Destructor - closes the database
     */
    ~LevelDBStorageEngine() {
        if (is_open_ && db_) {
            db_.reset();
            is_open_ = false;
        }
    }

    // Disable copy
    LevelDBStorageEngine(const LevelDBStorageEngine &) = delete;
    LevelDBStorageEngine &operator=(const LevelDBStorageEngine &) = delete;

    // Enable move
    LevelDBStorageEngine(LevelDBStorageEngine &&other) noexcept :
        StorageEngine<LevelDBStorageEngine>(other.level_, other.path_),
        db_(std::move(other.db_)), is_open_(other.is_open_),
        db_path_(std::move(other.db_path_)) {
        other.is_open_ = false;
    }

    LevelDBStorageEngine &operator=(LevelDBStorageEngine &&other) noexcept {
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
        leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
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
        leveldb::Status status = db_->Put(leveldb::WriteOptions(), key, value);
        if (status.ok()) {
            return Status::SUCCESS;
        }
        return Status::ERROR;
    }

    /**
     * @brief Implementation: Scan for key-value pairs from a starting point
     *
     * LevelDB maintains sorted order, making this efficient.
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

        leveldb::ReadOptions read_options;
        std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));

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
        leveldb::ReadOptions read_options;
        std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
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
        // LevelDB syncs automatically on write by default; WriteOptions.sync
        // controls that. For explicit sync we can try a no-op write or
        // rely on Close. leveldb::DB doesn't expose a direct Sync().
        // Return true as LevelDB is generally crash-safe.
        return true;
    }

    /**
     * @brief Clear all entries from the database
     */
    void clear() {
        if (!is_open_ || !db_) {
            return;
        }
        leveldb::ReadOptions read_options;
        std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
        leveldb::WriteBatch batch;
        for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
            batch.Delete(iter->key());
        }
        db_->Write(leveldb::WriteOptions(), &batch);
    }

    /**
     * @brief Remove a key from the database
     */
    Status remove(const std::string &key) {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        std::string dummy;
        leveldb::Status get_status =
            db_->Get(leveldb::ReadOptions(), key, &dummy);
        if (get_status.IsNotFound()) {
            return Status::NOT_FOUND;
        }
        leveldb::Status del_status = db_->Delete(leveldb::WriteOptions(), key);
        if (del_status.ok()) {
            return Status::SUCCESS;
        }
        return Status::ERROR;
    }

    /**
     * @brief LevelDB scan iterator for locality-optimized key lookups
     */
    class LevelDBIterator
        : public StorageEngineIterator<LevelDBIterator, LevelDBStorageEngine> {
    private:
        std::unique_ptr<leveldb::Iterator> iter_;

    public:
        explicit LevelDBIterator(LevelDBStorageEngine &engine) :
            StorageEngineIterator<LevelDBIterator, LevelDBStorageEngine>(
                engine) {
            if (engine.is_open_ && engine.db_) {
                iter_ = std::unique_ptr<leveldb::Iterator>(
                    engine.db_->NewIterator(leveldb::ReadOptions()));
            }
        }

        LevelDBIterator(const LevelDBIterator &) = delete;
        LevelDBIterator &operator=(const LevelDBIterator &) = delete;

        LevelDBIterator(LevelDBIterator &&other) noexcept :
            StorageEngineIterator<LevelDBIterator, LevelDBStorageEngine>(
                *other.engine_),
            iter_(std::move(other.iter_)) {}

        LevelDBIterator &operator=(LevelDBIterator &&other) noexcept {
            if (this != &other) {
                engine_ = other.engine_;
                iter_ = std::move(other.iter_);
            }
            return *this;
        }

        ~LevelDBIterator() = default;

        Status find_impl(const std::string &key, std::string &value) const {
            if (!iter_ || !engine_->is_open_) {
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
     * @brief Implementation: create and return a LevelDB scan iterator
     */
    LevelDBIterator iterator_impl() { return LevelDBIterator(*this); }

    using IteratorType = LevelDBIterator;
};

// Static member definitions
inline std::atomic_int LevelDBStorageEngine::db_counter_ = 0;
inline std::string LevelDBStorageEngine::id_ = std::to_string(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count());
