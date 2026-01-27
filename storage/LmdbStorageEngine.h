#pragma once

#include "StorageEngine.h"
#include <lmdb.h>
#include <string>
#include <vector>
#include <ctime>
#include <filesystem>
#include <atomic>
#include <chrono>

/**
 * @brief LMDB-based implementation of StorageEngine
 *
 * Uses LMDB (Lightning Memory-Mapped Database) for high-performance
 * key-value storage. LMDB is a B+tree database that provides:
 * - Memory-mapped I/O for excellent performance
 * - ACID transactions
 * - MVCC (Multi-Version Concurrency Control)
 * - Crash recovery
 * - Zero-copy reads
 *
 * Requires C++20 and liblmdb-dev to be installed.
 *
 * Key features:
 * - Keys are stored in sorted order (lexicographic)
 * - Very fast reads due to memory mapping
 * - Excellent for read-heavy workloads
 * - Crash-safe with WAL (Write-Ahead Logging)
 *
 * Limitations:
 * - Empty keys may not be supported (LMDB restriction)
 *
 * Note: This class is NOT thread-safe by default. Users must manually
 * call lock()/unlock() or lock_shared()/unlock_shared() when needed.
 */
class LmdbStorageEngine : public StorageEngine<LmdbStorageEngine> {
private:
    MDB_env *env_;
    MDB_dbi dbi_;
    bool is_open_;
    std::string db_path_;

    static std::atomic_int db_counter_;
    static std::string id_;

public:
    /**
     * @brief Constructor - creates an in-memory database
     * @param level The hierarchy level for this storage engine (default: 0)
     * @param path Optional path for database files (default: /tmp)
     *
     * Creates a temporary LMDB database in the provided path for in-memory-like
     * behavior.
     */
    explicit LmdbStorageEngine(size_t level = 0,
                               const std::string &path = "/tmp") :
        StorageEngine<LmdbStorageEngine>(level, path), env_(nullptr),
        dbi_(MDB_dbi{}), is_open_(false) {

        init();
    }

    /**
     * @brief Constructor with file path - creates a persistent database
     * @param file_path Path to the database directory
     * @param map_size Maximum size of the memory map in bytes (default: 50GB)
     * @param level The hierarchy level for this storage engine (default: 0)
     * @param path Optional path for database files (default: /tmp)
     */
    explicit LmdbStorageEngine(const std::string &file_path,
                               size_t map_size = 50ULL * 1024 * 1024 * 1024,
                               size_t level = 0,
                               const std::string &path = "/tmp") :
        StorageEngine<LmdbStorageEngine>(level, path), env_(nullptr),
        dbi_(MDB_dbi{}), is_open_(false), db_path_(file_path) {

        init_with_path(file_path, map_size);
    }

    /**
     * @brief Destructor - closes the database and cleans up
     */
    ~LmdbStorageEngine() {
        if (is_open_ && env_) {
            mdb_dbi_close(env_, dbi_);
            mdb_env_close(env_);
            is_open_ = false;

            // Clean up temporary directory if it was created
            std::string temp_prefix = path_ + "/repart_kv_storage/";
            if (db_path_.find(temp_prefix) == 0) {
                std::filesystem::remove_all(db_path_);
            }
        }
    }

    // Disable copy
    LmdbStorageEngine(const LmdbStorageEngine &) = delete;
    LmdbStorageEngine &operator=(const LmdbStorageEngine &) = delete;

    // Enable move
    LmdbStorageEngine(LmdbStorageEngine &&other) noexcept :
        StorageEngine<LmdbStorageEngine>(other.level_, other.path_),
        env_(other.env_), dbi_(other.dbi_), is_open_(other.is_open_),
        db_path_(std::move(other.db_path_)) {
        other.env_ = nullptr;
        other.is_open_ = false;
    }

    LmdbStorageEngine &operator=(LmdbStorageEngine &&other) noexcept {
        if (this != &other) {
            if (is_open_ && env_) {
                mdb_dbi_close(env_, dbi_);
                mdb_env_close(env_);
            }
            env_ = other.env_;
            dbi_ = other.dbi_;
            is_open_ = other.is_open_;
            db_path_ = std::move(other.db_path_);
            other.env_ = nullptr;
            other.is_open_ = false;
        }
        return *this;
    }

    /**
     * @brief Implementation: Read a value by key
     * @param key The key to read
     * @param value Reference to store the value associated with the key
     * @return Status code indicating the result of the operation
     */
    Status read_impl(const std::string &key, std::string &value) const {
        if (!is_open_ || !env_) {
            return Status::ERROR;
        }

        MDB_txn *txn;
        MDB_val mdb_key;
        MDB_val mdb_value;

        int rc = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
        if (rc != 0) {
            return Status::ERROR;
        }

        mdb_key.mv_size = key.size();
        mdb_key.mv_data =
            const_cast<void *>(static_cast<const void *>(key.c_str()));

        rc = mdb_get(txn, dbi_, &mdb_key, &mdb_value);
        if (rc == MDB_SUCCESS) {
            value = std::string(reinterpret_cast<char *>(mdb_value.mv_data),
                                mdb_value.mv_size);
            mdb_txn_abort(txn);
            return Status::SUCCESS;
        } else {
            mdb_txn_abort(txn);
            return Status::NOT_FOUND;
        }
    }

    /**
     * @brief Implementation: Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     * @return Status code indicating the result of the operation
     */
    Status write_impl(const std::string &key, const std::string &value) {
        if (!is_open_ || !env_) {
            return Status::ERROR;
        }

        MDB_txn *txn;
        MDB_val mdb_key;
        MDB_val mdb_value;

        int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
        if (rc != 0) {
            return Status::ERROR;
        }

        mdb_key.mv_size = key.size();
        mdb_key.mv_data =
            const_cast<void *>(static_cast<const void *>(key.c_str()));
        mdb_value.mv_size = value.size();
        mdb_value.mv_data =
            const_cast<void *>(static_cast<const void *>(value.c_str()));

        rc = mdb_put(txn, dbi_, &mdb_key, &mdb_value, 0);
        if (rc != 0) {
            mdb_txn_abort(txn);
            return Status::ERROR;
        }

        rc = mdb_txn_commit(txn);
        if (rc != 0) {
            return Status::ERROR;
        }

        return Status::SUCCESS;
    }

    /**
     * @brief Implementation: Scan for key-value pairs from a starting point
     * @param initial_key_prefix The starting key (lower_bound)
     * @param limit Maximum number of key-value pairs to return
     * @param results Reference to store the results of the scan
     * @return Status code indicating the result of the operation
     *
     * Note: LMDB maintains keys in sorted order, making this efficient.
     * We use MDB_SET_RANGE to find the first key >= key_prefix.
     */
    Status
    scan_impl(const std::string &initial_key_prefix, size_t limit,
              std::vector<std::pair<std::string, std::string>> &results) const {
        if (!is_open_ || !env_) {
            return Status::ERROR;
        }

        results.clear();
        results.reserve(limit);

        MDB_txn *txn;
        MDB_cursor *cursor;
        MDB_val mdb_key;
        MDB_val mdb_value;

        int rc = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
        if (rc != 0) {
            return Status::ERROR;
        }

        rc = mdb_cursor_open(txn, dbi_, &cursor);
        if (rc != 0) {
            mdb_txn_abort(txn);
            return Status::ERROR;
        }

        // Set the cursor to the first key >= initial_key_prefix
        if (initial_key_prefix.empty()) {
            // If prefix is empty, start from the very beginning of the database
            rc = mdb_cursor_get(cursor, &mdb_key, &mdb_value, MDB_FIRST);
        } else {
            // Use MDB_SET_RANGE to find first key >= initial_key_prefix
            mdb_key.mv_size = initial_key_prefix.size();
            mdb_key.mv_data = const_cast<void *>(
                static_cast<const void *>(initial_key_prefix.c_str()));

            rc = mdb_cursor_get(cursor, &mdb_key, &mdb_value, MDB_SET_RANGE);
        }
        if (rc == MDB_SUCCESS) {
            // Add the first result
            std::string key(reinterpret_cast<char *>(mdb_key.mv_data),
                            mdb_key.mv_size);
            std::string value(reinterpret_cast<char *>(mdb_value.mv_data),
                              mdb_value.mv_size);
            results.emplace_back(key, value);

            // Continue iterating
            while (results.size() < limit &&
                   mdb_cursor_get(cursor, &mdb_key, &mdb_value, MDB_NEXT) ==
                       0) {
                key = std::string(reinterpret_cast<char *>(mdb_key.mv_data),
                                  mdb_key.mv_size);
                value = std::string(reinterpret_cast<char *>(mdb_value.mv_data),
                                    mdb_value.mv_size);
                results.emplace_back(key, value);
            }
        }

        mdb_cursor_close(cursor);
        mdb_txn_abort(txn);

        if (results.empty()) {
            return Status::NOT_FOUND;
        }

        return Status::SUCCESS;
    }

    /**
     * @brief Check if the database is open
     * @return true if open, false otherwise
     */
    bool is_open() const { return is_open_; }

    /**
     * @brief Get the number of records in the database
     * @return Number of key-value pairs
     */
    int64_t count() const {
        if (!is_open_ || !env_) {
            return 0;
        }

        MDB_txn *txn;
        MDB_stat stat;

        int rc = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
        if (rc != 0) {
            return 0;
        }

        rc = mdb_stat(txn, dbi_, &stat);
        mdb_txn_abort(txn);

        if (rc != 0) {
            return 0;
        }

        return stat.ms_entries;
    }

    /**
     * @brief Synchronize the database to storage
     * @return true if successful, false otherwise
     */
    bool sync() {
        if (!is_open_ || !env_) {
            return false;
        }

        int rc = mdb_env_sync(env_, 1);
        return rc == 0;
    }

    /**
     * @brief Clear all entries from the database
     */
    void clear() {
        if (!is_open_ || !env_) {
            return;
        }

        MDB_txn *txn;
        int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
        if (rc != 0) {
            return;
        }

        rc = mdb_drop(txn, dbi_,
                      0); // 0 = don't delete the database, just clear it
        if (rc == 0) {
            mdb_txn_commit(txn);
        } else {
            mdb_txn_abort(txn);
        }
    }

    /**
     * @brief Remove a key from the database
     * @param key The key to remove
     * @return Status code indicating the result of the operation
     */
    Status remove(const std::string &key) {
        if (!is_open_ || !env_) {
            return Status::ERROR;
        }

        MDB_txn *txn;
        MDB_val mdb_key;

        int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
        if (rc != 0) {
            return Status::ERROR;
        }

        mdb_key.mv_size = key.size();
        mdb_key.mv_data =
            const_cast<void *>(static_cast<const void *>(key.c_str()));

        rc = mdb_del(txn, dbi_, &mdb_key, nullptr);
        if (rc == 0) {
            mdb_txn_commit(txn);
            return Status::SUCCESS;
        } else {
            mdb_txn_abort(txn);
            return Status::NOT_FOUND;
        }
    }

    /**
     * @brief Get the database path
     * @return The path to the database directory
     */
    const std::string &get_path() const { return db_path_; }

private:
    /**
     * @brief Initialize the database with a temporary path
     */
    void init() {
        db_path_ =
            path_ + std::string("/repart_kv_storage/") + id_ +
            std::string("/") +
            std::to_string(db_counter_.fetch_add(1, std::memory_order_relaxed));
        std::filesystem::create_directories(db_path_);

        init_with_path(db_path_, 50ULL * 1024 * 1024 * 1024);
    }

    /**
     * @brief Initialize the database with a specific path
     * @param path The database path
     * @param map_size The map size
     */
    void init_with_path(const std::string &path, size_t map_size) {
        int rc = mdb_env_create(&env_);
        if (rc != 0) {
            is_open_ = false;
            return;
        }

        mdb_env_set_maxdbs(env_, 1);
        mdb_env_set_mapsize(env_, map_size);

        rc =
            mdb_env_open(env_, path.c_str(), MDB_NOSYNC | MDB_NOMETASYNC, 0664);
        if (rc != 0) {
            mdb_env_close(env_);
            env_ = nullptr;
            is_open_ = false;
            return;
        }

        MDB_txn *txn;
        rc = mdb_txn_begin(env_, nullptr, 0, &txn);
        if (rc != 0) {
            mdb_env_close(env_);
            env_ = nullptr;
            is_open_ = false;
            return;
        }

        rc = mdb_dbi_open(txn, nullptr, 0, &dbi_);
        if (rc != 0) {
            mdb_txn_abort(txn);
            mdb_env_close(env_);
            env_ = nullptr;
            is_open_ = false;
            return;
        }

        mdb_txn_commit(txn);
        is_open_ = true;
    }
};

// Static member definitions
inline std::atomic_int LmdbStorageEngine::db_counter_ = 0;
inline std::string LmdbStorageEngine::id_ = std::to_string(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count());
