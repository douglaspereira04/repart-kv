#pragma once

#include "KeyStorage.h"
#include "KeyStorageIterator.h"
#include <lmdb.h>
#include <string>
#include <filesystem>
#include <atomic>
#include <chrono>

// Forward declaration
template<KeyStorageValueType ValueType>
class LmdbKeyStorageIterator;

/**
 * @brief LMDB-based implementation of KeyStorage
 * @tparam ValueType The type of values stored (integral types or pointers)
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
 * - ValueType must be serializable to/from string representation
 * 
 * Note: This class is NOT thread-safe by default. Users must manually
 * call lock()/unlock() or lock_shared()/unlock_shared() when needed.
 */
template<KeyStorageValueType ValueType>
class LmdbKeyStorage : public KeyStorage<LmdbKeyStorage<ValueType>, LmdbKeyStorageIterator<ValueType>, ValueType> {
private:
    MDB_env* env_;
    MDB_dbi dbi_;
    bool is_open_;
    std::string db_path_;
    
    static std::atomic_int db_counter;
    static std::string id;

public:
    /**
     * @brief Constructor - creates an in-memory database
     * 
     * Creates a temporary LMDB database in /tmp for in-memory-like behavior.
     */
    LmdbKeyStorage() 
        : env_(nullptr),
          dbi_(MDB_dbi{}),
          is_open_(false) {
        
        init();
    }

    /**
     * @brief Constructor with file path - creates a persistent database
     * @param file_path Path to the database directory
     * @param map_size Maximum size of the memory map in bytes (default: 50GB)
     */
    explicit LmdbKeyStorage(const std::string& file_path, 
                           size_t map_size = 50ULL * 1024 * 1024 * 1024) 
        : env_(nullptr),
          dbi_(MDB_dbi{}),
          is_open_(false),
          db_path_(file_path) {
        
        init_with_path(file_path, map_size);
    }

    /**
     * @brief Destructor - closes the database and cleans up
     */
    ~LmdbKeyStorage() {
        if (is_open_ && env_) {
            mdb_dbi_close(env_, dbi_);
            mdb_env_close(env_);
            is_open_ = false;
            
            // Clean up temporary directory if it was created
            if (db_path_.find("/tmp/repart_kv_keystorage/") == 0) {
                std::filesystem::remove_all(db_path_);
            }
        }
    }

    // Disable copy
    LmdbKeyStorage(const LmdbKeyStorage&) = delete;
    LmdbKeyStorage& operator=(const LmdbKeyStorage&) = delete;

    // Enable move
    LmdbKeyStorage(LmdbKeyStorage&& other) noexcept 
        : env_(other.env_),
          dbi_(other.dbi_),
          is_open_(other.is_open_),
          db_path_(std::move(other.db_path_)) {
        other.env_ = nullptr;
        other.is_open_ = false;
    }

    LmdbKeyStorage& operator=(LmdbKeyStorage&& other) noexcept {
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
     * @brief Implementation: Get a value by key
     * @param key The key to look up
     * @param value Output parameter for the retrieved value
     * @return true if the key exists, false otherwise
     */
    bool get_impl(const std::string& key, ValueType& value) const {
        if (!is_open_ || !env_) {
            return false;
        }
        
        MDB_txn* txn;
        MDB_val mdb_key;
        MDB_val mdb_value;
        
        int rc = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
        if (rc != 0) {
            return false;
        }
        
        mdb_key.mv_size = key.size();
        mdb_key.mv_data = const_cast<void*>(static_cast<const void*>(key.c_str()));
        
        rc = mdb_get(txn, dbi_, &mdb_key, &mdb_value);
        if (rc == MDB_SUCCESS) {
            // Convert the stored string back to ValueType
            std::string value_str(reinterpret_cast<char*>(mdb_value.mv_data), mdb_value.mv_size);
            value = deserialize_value(value_str);
            mdb_txn_abort(txn);
            return true;
        } else {
            mdb_txn_abort(txn);
            return false;
        }
    }

    /**
     * @brief Implementation: Put a key-value pair into storage
     * @param key The key to store
     * @param value The value to associate with the key
     */
    void put_impl(const std::string& key, const ValueType& value) {
        if (!is_open_ || !env_) {
            return;
        }
        
        MDB_txn* txn;
        MDB_val mdb_key;
        MDB_val mdb_value;
        
        int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
        if (rc != 0) {
            return;
        }
        
        // Serialize the value to string
        std::string value_str = serialize_value(value);
        
        mdb_key.mv_size = key.size();
        mdb_key.mv_data = const_cast<void*>(static_cast<const void*>(key.c_str()));
        mdb_value.mv_size = value_str.size();
        mdb_value.mv_data = const_cast<void*>(static_cast<const void*>(value_str.c_str()));
        
        rc = mdb_put(txn, dbi_, &mdb_key, &mdb_value, 0);
        if (rc != 0) {
            mdb_txn_abort(txn);
            return;
        }
        
        rc = mdb_txn_commit(txn);
        if (rc != 0) {
            return;
        }
    }

    /**
     * @brief Implementation: Find the first element with key not less than the given key
     * @param key The key to search for
     * @return Iterator pointing to the found element or end
     */
    LmdbKeyStorageIterator<ValueType> lower_bound_impl(const std::string& key);

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
        if (!is_open_ || !env_) {
            return 0;
        }
        
        MDB_txn* txn;
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
        
        MDB_txn* txn;
        int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
        if (rc != 0) {
            return;
        }
        
        rc = mdb_drop(txn, dbi_, 0);  // 0 = don't delete the database, just clear it
        if (rc == 0) {
            mdb_txn_commit(txn);
        } else {
            mdb_txn_abort(txn);
        }
    }

    /**
     * @brief Remove a key from the database
     * @param key The key to remove
     * @return true if the key was removed, false if not found
     */
    bool remove(const std::string& key) {
        if (!is_open_ || !env_) {
            return false;
        }
        
        MDB_txn* txn;
        MDB_val mdb_key;
        
        int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
        if (rc != 0) {
            return false;
        }
        
        mdb_key.mv_size = key.size();
        mdb_key.mv_data = const_cast<void*>(static_cast<const void*>(key.c_str()));
        
        rc = mdb_del(txn, dbi_, &mdb_key, nullptr);
        if (rc == 0) {
            mdb_txn_commit(txn);
            return true;
        } else {
            mdb_txn_abort(txn);
            return false;
        }
    }

    /**
     * @brief Get the database path
     * @return The path to the database directory
     */
    const std::string& get_path() const {
        return db_path_;
    }

private:
    /**
     * @brief Initialize the database with a temporary path
     */
    void init() {
        db_path_ = std::string("/tmp/repart_kv_keystorage/") +
                   id +
                   std::string("/") +
                   std::to_string(db_counter.fetch_add(1, std::memory_order_relaxed));
        std::filesystem::create_directories(db_path_);
        
        init_with_path(db_path_, 50ULL * 1024 * 1024 * 1024);
    }
    
    /**
     * @brief Initialize the database with a specific path
     * @param path The database path
     * @param map_size The map size
     */
    void init_with_path(const std::string& path, size_t map_size) {
        int rc = mdb_env_create(&env_);
        if (rc != 0) {
            is_open_ = false;
            return;
        }
        
        mdb_env_set_maxdbs(env_, 1);
        mdb_env_set_mapsize(env_, map_size);
        
        rc = mdb_env_open(env_, path.c_str(), MDB_NOSYNC | MDB_NOMETASYNC, 0664);
        if (rc != 0) {
            mdb_env_close(env_);
            env_ = nullptr;
            is_open_ = false;
            return;
        }
        
        MDB_txn* txn;
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

    /**
     * @brief Serialize a value to string for storage
     * @param value The value to serialize
     * @return String representation of the value
     */
    std::string serialize_value(const ValueType& value) const {
        if constexpr (std::is_pointer_v<ValueType>) {
            // For pointers, store as string representation of the address
            return std::to_string(reinterpret_cast<uintptr_t>(value));
        } else {
            // For integral types, convert to string
            return std::to_string(value);
        }
    }

    /**
     * @brief Deserialize a string back to ValueType
     * @param value_str The string representation
     * @return The deserialized value
     */
    ValueType deserialize_value(const std::string& value_str) const {
        if constexpr (std::is_pointer_v<ValueType>) {
            // For pointers, reconstruct from address
            return reinterpret_cast<ValueType>(std::stoull(value_str));
        } else {
            // For integral types, parse from string
            if constexpr (std::is_same_v<ValueType, int>) {
                return std::stoi(value_str);
            } else if constexpr (std::is_same_v<ValueType, long>) {
                return std::stol(value_str);
            } else if constexpr (std::is_same_v<ValueType, long long>) {
                return std::stoll(value_str);
            } else if constexpr (std::is_same_v<ValueType, unsigned int>) {
                return static_cast<unsigned int>(std::stoul(value_str));
            } else if constexpr (std::is_same_v<ValueType, unsigned long>) {
                return std::stoul(value_str);
            } else if constexpr (std::is_same_v<ValueType, unsigned long long>) {
                return std::stoull(value_str);
            } else if constexpr (std::is_same_v<ValueType, size_t>) {
                return static_cast<size_t>(std::stoull(value_str));
            } else if constexpr (std::is_same_v<ValueType, int64_t>) {
                return std::stoll(value_str);
            } else if constexpr (std::is_same_v<ValueType, uint64_t>) {
                return std::stoull(value_str);
            } else {
                // For other integral types, use generic conversion
                return static_cast<ValueType>(std::stoll(value_str));
            }
        }
    }
};

/**
 * @brief Iterator implementation for LmdbKeyStorage using LMDB cursors
 * @tparam ValueType The type of values stored
 * 
 * Requires C++20 for concepts and CRTP pattern.
 */
template<KeyStorageValueType ValueType>
class LmdbKeyStorageIterator : public KeyStorageIterator<LmdbKeyStorageIterator<ValueType>, ValueType> {
private:
    MDB_env* env_;
    MDB_dbi dbi_;
    MDB_txn* txn_;
    MDB_cursor* cursor_;
    MDB_val current_key_;
    MDB_val current_value_;
    bool is_valid_;
    bool is_at_end_;

public:
    /**
     * @brief Constructor for begin iterator
     * @param env LMDB environment
     * @param dbi LMDB database handle
     */
    LmdbKeyStorageIterator(MDB_env* env, MDB_dbi dbi)
        : env_(env), dbi_(dbi), txn_(nullptr), cursor_(nullptr), 
          is_valid_(false), is_at_end_(false) {
        
        if (!env_ || !dbi_) {
            is_at_end_ = true;
            return;
        }
        
        int rc = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn_);
        if (rc != 0) {
            is_at_end_ = true;
            return;
        }
        
        rc = mdb_cursor_open(txn_, dbi_, &cursor_);
        if (rc != 0) {
            mdb_txn_abort(txn_);
            txn_ = nullptr;
            is_at_end_ = true;
            return;
        }
        
        // Position at first element
        rc = mdb_cursor_get(cursor_, &current_key_, &current_value_, MDB_FIRST);
        if (rc == MDB_SUCCESS) {
            is_valid_ = true;
        } else {
            is_at_end_ = true;
        }
    }

    /**
     * @brief Constructor for lower_bound iterator
     * @param env LMDB environment
     * @param dbi LMDB database handle
     * @param key The key to search for
     */
    LmdbKeyStorageIterator(MDB_env* env, MDB_dbi dbi, const std::string& key)
        : env_(env), dbi_(dbi), txn_(nullptr), cursor_(nullptr), 
          is_valid_(false), is_at_end_(false) {
        
        if (!env_ || !dbi_) {
            is_at_end_ = true;
            return;
        }
        
        int rc = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn_);
        if (rc != 0) {
            is_at_end_ = true;
            return;
        }
        
        rc = mdb_cursor_open(txn_, dbi_, &cursor_);
        if (rc != 0) {
            mdb_txn_abort(txn_);
            txn_ = nullptr;
            is_at_end_ = true;
            return;
        }
        
        // Set the cursor to the first key >= key
        if (key.empty()) {
            // If key is empty, start from the very beginning of the database
            rc = mdb_cursor_get(cursor_, &current_key_, &current_value_, MDB_FIRST);
        } else {
            // Use MDB_SET_RANGE to find first key >= key
            MDB_val mdb_key;
            mdb_key.mv_size = key.size();
            mdb_key.mv_data = const_cast<void*>(static_cast<const void*>(key.c_str()));
            
            rc = mdb_cursor_get(cursor_, &mdb_key, &current_value_, MDB_SET_RANGE);
            if (rc == MDB_SUCCESS) {
                current_key_ = mdb_key;
            }
        }
        if (rc == MDB_SUCCESS) {
            is_valid_ = true;
        } else {
            is_at_end_ = true;
        }
    }

    /**
     * @brief Destructor
     */
    ~LmdbKeyStorageIterator() {
        if (cursor_) {
            mdb_cursor_close(cursor_);
        }
        if (txn_) {
            mdb_txn_abort(txn_);
        }
    }

    // Disable copy
    LmdbKeyStorageIterator(const LmdbKeyStorageIterator&) = delete;
    LmdbKeyStorageIterator& operator=(const LmdbKeyStorageIterator&) = delete;

    // Enable move
    LmdbKeyStorageIterator(LmdbKeyStorageIterator&& other) noexcept
        : env_(other.env_), dbi_(other.dbi_), txn_(other.txn_), cursor_(other.cursor_),
          current_key_(other.current_key_), current_value_(other.current_value_),
          is_valid_(other.is_valid_), is_at_end_(other.is_at_end_) {
        other.txn_ = nullptr;
        other.cursor_ = nullptr;
        other.is_valid_ = false;
        other.is_at_end_ = true;
    }

    LmdbKeyStorageIterator& operator=(LmdbKeyStorageIterator&& other) noexcept {
        if (this != &other) {
            if (cursor_) {
                mdb_cursor_close(cursor_);
            }
            if (txn_) {
                mdb_txn_abort(txn_);
            }
            env_ = other.env_;
            dbi_ = other.dbi_;
            txn_ = other.txn_;
            cursor_ = other.cursor_;
            current_key_ = other.current_key_;
            current_value_ = other.current_value_;
            is_valid_ = other.is_valid_;
            is_at_end_ = other.is_at_end_;
            other.txn_ = nullptr;
            other.cursor_ = nullptr;
            other.is_valid_ = false;
            other.is_at_end_ = true;
        }
        return *this;
    }

    /**
     * @brief Implementation: Get the key at the current iterator position
     * @return The key as a string
     */
    std::string get_key_impl() const {
        if (is_at_end_ || !is_valid_) {
            return "";
        }
        return std::string(reinterpret_cast<char*>(current_key_.mv_data), current_key_.mv_size);
    }

    /**
     * @brief Implementation: Get the value at the current iterator position
     * @return The value
     */
    ValueType get_value_impl() const {
        if (is_at_end_ || !is_valid_) {
            return ValueType();
        }
        
        // Deserialize the value from the stored string
        std::string value_str(reinterpret_cast<char*>(current_value_.mv_data), current_value_.mv_size);
        return deserialize_value(value_str);
    }

    /**
     * @brief Implementation: Increment the iterator to the next element
     */
    void increment_impl() {
        if (is_at_end_ || !is_valid_ || !cursor_) {
            is_at_end_ = true;
            return;
        }
        
        int rc = mdb_cursor_get(cursor_, &current_key_, &current_value_, MDB_NEXT);
        if (rc == MDB_SUCCESS) {
            is_valid_ = true;
        } else {
            is_at_end_ = true;
        }
    }

    /**
     * @brief Implementation: Check if this iterator is at the end
     * @return true if at end, false otherwise
     */
    bool is_end_impl() const {
        return is_at_end_ || !is_valid_;
    }

private:
    /**
     * @brief Deserialize a string back to ValueType
     * @param value_str The string representation
     * @return The deserialized value
     */
    ValueType deserialize_value(const std::string& value_str) const {
        if constexpr (std::is_pointer_v<ValueType>) {
            // For pointers, reconstruct from address
            return reinterpret_cast<ValueType>(std::stoull(value_str));
        } else {
            // For integral types, parse from string
            if constexpr (std::is_same_v<ValueType, int>) {
                return std::stoi(value_str);
            } else if constexpr (std::is_same_v<ValueType, long>) {
                return std::stol(value_str);
            } else if constexpr (std::is_same_v<ValueType, long long>) {
                return std::stoll(value_str);
            } else if constexpr (std::is_same_v<ValueType, unsigned int>) {
                return static_cast<unsigned int>(std::stoul(value_str));
            } else if constexpr (std::is_same_v<ValueType, unsigned long>) {
                return std::stoul(value_str);
            } else if constexpr (std::is_same_v<ValueType, unsigned long long>) {
                return std::stoull(value_str);
            } else if constexpr (std::is_same_v<ValueType, size_t>) {
                return static_cast<size_t>(std::stoull(value_str));
            } else if constexpr (std::is_same_v<ValueType, int64_t>) {
                return std::stoll(value_str);
            } else if constexpr (std::is_same_v<ValueType, uint64_t>) {
                return std::stoull(value_str);
            } else {
                // For other integral types, use generic conversion
                return static_cast<ValueType>(std::stoll(value_str));
            }
        }
    }
};

// Implementation of lower_bound_impl
template<KeyStorageValueType ValueType>
LmdbKeyStorageIterator<ValueType> LmdbKeyStorage<ValueType>::lower_bound_impl(const std::string& key) {
    return LmdbKeyStorageIterator<ValueType>(env_, dbi_, key);
}

// Static member definitions
template<KeyStorageValueType ValueType>
inline std::atomic_int LmdbKeyStorage<ValueType>::db_counter = 0;

template<KeyStorageValueType ValueType>
inline std::string LmdbKeyStorage<ValueType>::id = std::to_string(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count()
);
