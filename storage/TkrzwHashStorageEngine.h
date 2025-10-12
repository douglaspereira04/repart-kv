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
     * @brief Implementation: Scan for key-value pairs from a starting point
     * @param key_prefix The starting key (lower_bound)
     * @param limit Maximum number of key-value pairs to return
     * @return Vector of key-value pairs where keys >= key_prefix (in sorted order)
     * 
     * Note: HashDBM doesn't maintain sorted order, so this collects all key-value pairs,
     * sorts them by key, and returns those >= key_prefix.
     */
    std::vector<std::pair<std::string, std::string>> scan_impl(const std::string& key_prefix, size_t limit) const {
        std::vector<std::pair<std::string, std::string>> results;
        
        if (!is_open_) {
            return results;
        }
        
        // Collect all key-value pairs first (HashDBM is unordered)
        std::vector<std::pair<std::string, std::string>> all_pairs;
        auto iter = db_->MakeIterator();
        iter->First();
        
        std::string key, value;
        while (iter->Get(&key, &value) == tkrzw::Status::SUCCESS) {
            all_pairs.push_back({key, value});
            if (iter->Next() != tkrzw::Status::SUCCESS) {
                break;
            }
        }
        
        // Sort all pairs by key
        std::sort(all_pairs.begin(), all_pairs.end(), 
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // Find first key >= key_prefix and collect up to limit
        results.reserve(std::min(limit, all_pairs.size()));
        for (const auto& pair : all_pairs) {
            if (pair.first >= key_prefix) {
                results.push_back(pair);
                if (results.size() >= limit) {
                    break;
                }
            }
        }
        
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

