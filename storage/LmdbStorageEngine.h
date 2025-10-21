#pragma once

#include "StorageEngine.h"
#include <lmdb++.h>
#include <string>
#include <vector>
#include <memory>
#include <ctime>
#include <filesystem>

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
 * Requires C++20 and liblmdb++-dev to be installed.
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
    std::unique_ptr<lmdb::env> env_;
    lmdb::dbi dbi_;
    bool is_open_;
    std::string db_path_;

public:
    /**
     * @brief Constructor - creates an in-memory database
     * @param level The hierarchy level for this storage engine (default: 0)
     * 
     * Creates a temporary LMDB database in /tmp for in-memory-like behavior.
     */
    explicit LmdbStorageEngine(size_t level = 0) 
        : StorageEngine<LmdbStorageEngine>(level),
          env_(std::make_unique<lmdb::env>(lmdb::env::create())),
          dbi_(MDB_dbi{}),
          is_open_(false) {
        
        // Create a temporary directory for the LMDB database
        std::string temp_dir = "/tmp/lmdb_temp_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(temp_dir);
        db_path_ = temp_dir;
        
        try {
            // Set map size to 100MB
            env_->set_mapsize(100 * 1024 * 1024);
            env_->set_max_dbs(1);
            env_->open(db_path_.c_str(), MDB_CREATE | MDB_NOSYNC, 0644);
            
            // Open the database
            auto txn = lmdb::txn::begin(*env_);
            dbi_ = lmdb::dbi::open(txn, nullptr, MDB_CREATE);
            txn.commit();
            
            is_open_ = true;
        } catch (const lmdb::error& e) {
            is_open_ = false;
        }
    }

    /**
     * @brief Constructor with file path - creates a persistent database
     * @param file_path Path to the database directory
     * @param map_size Maximum size of the memory map in bytes (default: 100MB)
     * @param level The hierarchy level for this storage engine (default: 0)
     */
    explicit LmdbStorageEngine(const std::string& file_path, 
                               size_t map_size = 100 * 1024 * 1024,
                               size_t level = 0) 
        : StorageEngine<LmdbStorageEngine>(level),
          env_(std::make_unique<lmdb::env>(lmdb::env::create())),
          dbi_(MDB_dbi{}),
          is_open_(false),
          db_path_(file_path) {
        
        try {
            // Create directory if it doesn't exist
            std::filesystem::create_directories(file_path);
            
            // Set map size and open environment
            env_->set_mapsize(map_size);
            env_->set_max_dbs(1);
            env_->open(file_path.c_str(), MDB_CREATE, 0644);
            
            // Open the database
            auto txn = lmdb::txn::begin(*env_);
            dbi_ = lmdb::dbi::open(txn, nullptr, MDB_CREATE);
            txn.commit();
            
            is_open_ = true;
        } catch (const lmdb::error& e) {
            is_open_ = false;
        }
    }

    /**
     * @brief Destructor - closes the database and cleans up
     */
    ~LmdbStorageEngine() {
        if (is_open_) {
            try {
                env_->close();
                is_open_ = false;
                
                // Clean up temporary directory if it was created
                if (db_path_.find("/tmp/lmdb_temp_") == 0) {
                    std::filesystem::remove_all(db_path_);
                }
            } catch (const lmdb::error& e) {
                // Ignore errors during cleanup
            }
        }
    }

    // Disable copy
    LmdbStorageEngine(const LmdbStorageEngine&) = delete;
    LmdbStorageEngine& operator=(const LmdbStorageEngine&) = delete;

    // Enable move
    LmdbStorageEngine(LmdbStorageEngine&& other) noexcept 
        : StorageEngine<LmdbStorageEngine>(other.level_),
          env_(std::move(other.env_)),
          dbi_(std::move(other.dbi_)),
          is_open_(other.is_open_),
          db_path_(std::move(other.db_path_)) {
        other.is_open_ = false;
    }

    LmdbStorageEngine& operator=(LmdbStorageEngine&& other) noexcept {
        if (this != &other) {
            if (is_open_) {
                try {
                    env_->close();
                } catch (const lmdb::error& e) {
                    // Ignore errors during cleanup
                }
            }
            env_ = std::move(other.env_);
            dbi_ = std::move(other.dbi_);
            is_open_ = other.is_open_;
            db_path_ = std::move(other.db_path_);
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
    Status read_impl(const std::string& key, std::string& value) const {
        if (!is_open_) {
            return Status::ERROR;
        }
        
        try {
            auto txn = lmdb::txn::begin(*env_, nullptr, MDB_RDONLY);
            auto cursor = lmdb::cursor::open(txn, dbi_);
            
            lmdb::val key_val(key);
            lmdb::val data_val;
            
            if (cursor.get(key_val, data_val, MDB_SET_KEY)) {
                value = std::string(data_val.data(), data_val.size());
                cursor.close();
                txn.commit();
                return Status::SUCCESS;
            } else {
                cursor.close();
                txn.commit();
                return Status::NOT_FOUND;
            }
        } catch (const lmdb::error& e) {
            return Status::ERROR;
        }
    }

    /**
     * @brief Implementation: Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     * @return Status code indicating the result of the operation
     */
    Status write_impl(const std::string& key, const std::string& value) {        
        try {
            auto txn = lmdb::txn::begin(*env_);
            lmdb::val key_val(key);
            lmdb::val data_val(value);
            
            dbi_.put(txn, key_val, data_val);
            txn.commit();
            return Status::SUCCESS;
        } catch (const lmdb::error& e) {
            return Status::ERROR;
        }
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
    Status scan_impl(const std::string& initial_key_prefix, size_t limit, std::vector<std::pair<std::string, std::string>>& results) const {

        size_t count = 0;
        results.resize(limit);
        try {
            auto txn = lmdb::txn::begin(*env_, nullptr, MDB_RDONLY);
            auto cursor = lmdb::cursor::open(txn, dbi_);
            
            lmdb::val key_val;
            lmdb::val data_val;
    
            lmdb::val prefix_val(initial_key_prefix);
            if (cursor.get(prefix_val, data_val, MDB_SET_RANGE)) {
                // Add the first result
                std::string key(prefix_val.data(), prefix_val.size());
                std::string value(data_val.data(), data_val.size());
                results[count] = std::make_pair(key, value);
                ++count;
                // Continue iterating
                while (count < limit && cursor.get(key_val, data_val, MDB_NEXT)) {
                    key = std::string(key_val.data(), key_val.size());
                    value = std::string(data_val.data(), data_val.size());
                    results[count] = std::make_pair(key, value);
                    ++count;
                }
            }
            
            cursor.close();
            txn.commit();
            
            if (count < limit) {
                results.resize(count);
                if (count == 0) {
                    return Status::NOT_FOUND;
                }
            }
            
            return Status::SUCCESS;
        } catch (const lmdb::error& e) {
            return Status::NOT_FOUND;
        }
    }

    /**
     * @brief Check if the database is open
     * @return true if open, false otherwise
     */
    bool is_open() const {
        return is_open_;
    }

    /**
     * @brief Get the number of records in the database
     * @return Number of key-value pairs
     */
    int64_t count() const {
        if (!is_open_) {
            return 0;
        }
        
        try {
            auto txn = lmdb::txn::begin(*env_, nullptr, MDB_RDONLY);
            int64_t count = dbi_.size(txn);
            txn.commit();
            return count;
        } catch (const lmdb::error& e) {
            return 0;
        }
    }

    /**
     * @brief Synchronize the database to storage
     * @return true if successful, false otherwise
     */
    bool sync() {
        if (!is_open_) {
            return false;
        }
        
        try {
            env_->sync(true);
            return true;
        } catch (const lmdb::error& e) {
            return false;
        }
    }

    /**
     * @brief Clear all entries from the database
     */
    void clear() {
        if (!is_open_) {
            return;
        }
        
        try {
            auto txn = lmdb::txn::begin(*env_);
            dbi_.drop(txn, false);  // false = don't delete the database, just clear it
            txn.commit();
        } catch (const lmdb::error& e) {
            // Ignore errors during clear
        }
    }

    /**
     * @brief Remove a key from the database
     * @param key The key to remove
     * @return Status code indicating the result of the operation
     */
    Status remove(const std::string& key) {
        if (!is_open_) {
            return Status::ERROR;
        }
        
        try {
            auto txn = lmdb::txn::begin(*env_);
            lmdb::val key_val(key);
            
            if (dbi_.del(txn, key_val)) {
                txn.commit();
                return Status::SUCCESS;
            } else {
                txn.abort();
                return Status::NOT_FOUND;
            }
        } catch (const lmdb::error& e) {
            return Status::ERROR;
        }
    }

    /**
     * @brief Get the database path
     * @return The path to the database directory
     */
    const std::string& get_path() const {
        return db_path_;
    }
};
