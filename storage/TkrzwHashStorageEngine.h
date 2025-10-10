#pragma once

#include "StorageEngine.h"
#include <tkrzw_dbm_hash.h>
#include <string>
#include <vector>
#include <memory>
#include <ctime>
#include <algorithm>

/**
 * @brief TKRZW HashDBM-based implementation of StorageEngine
 * 
 * Uses TKRZW's hash database (HashDBM) for high-performance key-value storage.
 * TKRZW is a modern, efficient key-value database library that provides
 * excellent performance for both read and write operations.
 * 
 * Requires C++20 and libtkrzw-dev to be installed.
 * 
 * Note: This class is NOT thread-safe by default. Users must manually
 * call lock()/unlock() or lock_shared()/unlock_shared() when needed.
 */
class TkrzwHashStorageEngine : public StorageEngine<TkrzwHashStorageEngine> {
private:
    std::unique_ptr<tkrzw::HashDBM> db_;
    bool is_open_;

public:
    /**
     * @brief Constructor - creates an in-memory database
     * 
     * Note: TKRZW HashDBM doesn't support true in-memory mode.
     * For in-memory storage, consider using tkrzw::BabyDBM or
     * a temporary file that gets deleted.
     */
    TkrzwHashStorageEngine() : db_(std::make_unique<tkrzw::HashDBM>()), is_open_(false) {
        // TKRZW HashDBM requires a file path, so we use /tmp for in-memory-like behavior
        // The file will be created but can be considered temporary
        std::string temp_path = "/tmp/tkrzw_temp_" + std::to_string(time(nullptr)) + ".tkh";

        
        tkrzw::Status status = db_->OpenAdvanced(
            temp_path,
            true,  // writable
            tkrzw::File::OPEN_TRUNCATE
        );
        
        if (status == tkrzw::Status::SUCCESS) {
            is_open_ = true;
        }
    }

    /**
     * @brief Constructor with file path - creates a persistent database
     * @param file_path Path to the database file
     * @param num_buckets Number of hash buckets (default: 1000000)
     */
    explicit TkrzwHashStorageEngine(const std::string& file_path, int64_t num_buckets = 1000000) 
        : db_(std::make_unique<tkrzw::HashDBM>()), is_open_(false) {
        
        tkrzw::HashDBM::TuningParameters tuning_params;
        tuning_params.num_buckets = num_buckets;
        
        tkrzw::Status status = db_->OpenAdvanced(
            file_path, 
            true,  // writable
            tkrzw::File::OPEN_DEFAULT,  // file opening options
            tuning_params
        );
        
        if (status == tkrzw::Status::SUCCESS) {
            is_open_ = true;
        }
    }

    /**
     * @brief Destructor - closes the database
     */
    ~TkrzwHashStorageEngine() {
        if (is_open_) {
            db_->Close();
            is_open_ = false;
        }
    }

    // Disable copy
    TkrzwHashStorageEngine(const TkrzwHashStorageEngine&) = delete;
    TkrzwHashStorageEngine& operator=(const TkrzwHashStorageEngine&) = delete;

    // Enable move
    TkrzwHashStorageEngine(TkrzwHashStorageEngine&& other) noexcept 
        : db_(std::move(other.db_)), is_open_(other.is_open_) {
        other.is_open_ = false;
    }

    TkrzwHashStorageEngine& operator=(TkrzwHashStorageEngine&& other) noexcept {
        if (this != &other) {
            if (is_open_) {
                db_->Close();
            }
            db_ = std::move(other.db_);
            is_open_ = other.is_open_;
            other.is_open_ = false;
        }
        return *this;
    }

    /**
     * @brief Implementation: Read a value by key
     * @param key The key to read
     * @return The value associated with the key, or empty string if not found
     */
    std::string read_impl(const std::string& key) const {
        if (!is_open_) {
            return "";
        }
        
        std::string value;
        tkrzw::Status status = db_->Get(key, &value);
        
        if (status == tkrzw::Status::SUCCESS) {
            return value;
        }
        return "";
    }

    /**
     * @brief Implementation: Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     */
    void write_impl(const std::string& key, const std::string& value) {
        if (!is_open_) {
            return;
        }
        
        db_->Set(key, value);
    }

    /**
     * @brief Implementation: Scan for keys starting with a given prefix
     * @param key_prefix The prefix to search for
     * @param limit Maximum number of keys to return
     * @return Vector of keys matching the prefix
     * 
     * Note: HashDBM doesn't maintain sorted order, so this iterates through
     * all keys and filters by prefix. For sorted scans, consider using TreeDBM.
     */
    std::vector<std::string> scan_impl(const std::string& key_prefix, size_t limit) const {
        std::vector<std::string> results;
        
        if (!is_open_) {
            return results;
        }
        
        results.reserve(std::min(limit, static_cast<size_t>(1000)));
        
        // Create an iterator and start from first key
        auto iter = db_->MakeIterator();
        iter->First();
        
        // Iterate through all keys (HashDBM is unordered)
        std::string key, value;
        while (results.size() < limit) {
            tkrzw::Status status = iter->Get(&key, &value);
            if (status != tkrzw::Status::SUCCESS) {
                break;  // No more keys
            }
            
            // Check if key starts with prefix
            if (key_prefix.empty() || key.compare(0, key_prefix.length(), key_prefix) == 0) {
                results.push_back(key);
            }
            
            // Move to next key
            if (iter->Next() != tkrzw::Status::SUCCESS) {
                break;
            }
        }
        
        // Sort results for consistency (since HashDBM is unordered)
        std::sort(results.begin(), results.end());
        
        return results;
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
        return db_->CountSimple();
    }

    /**
     * @brief Synchronize the database to storage
     * @return true if successful, false otherwise
     */
    bool sync() {
        if (!is_open_) {
            return false;
        }
        return db_->Synchronize(false) == tkrzw::Status::SUCCESS;
    }

    /**
     * @brief Clear all entries from the database
     */
    void clear() {
        if (!is_open_) {
            return;
        }
        db_->Clear();
    }

    /**
     * @brief Remove a key from the database
     * @param key The key to remove
     * @return true if the key was removed, false if not found
     */
    bool remove(const std::string& key) {
        if (!is_open_) {
            return false;
        }
        return db_->Remove(key) == tkrzw::Status::SUCCESS;
    }
};

