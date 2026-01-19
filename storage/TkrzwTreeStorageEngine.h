#pragma once

#include "StorageEngine.h"
#include <tkrzw_dbm_tree.h>
#include <string>
#include <vector>
#include <memory>
#include <ctime>
#include <atomic>
#include <chrono>
#include <filesystem>

/**
 * @brief TKRZW TreeDBM-based implementation of StorageEngine
 *
 * Uses TKRZW's tree database (TreeDBM) for sorted key-value storage.
 * TreeDBM maintains keys in sorted order, making range queries and
 * prefix scans very efficient. TKRZW is a modern, efficient key-value
 * database library that provides excellent performance.
 *
 * Requires C++20 and libtkrzw-dev to be installed.
 *
 * Key differences from HashDBM:
 * - Keys are stored in sorted order (lexicographic)
 * - Efficient range queries and prefix scans
 * - Slightly slower writes than HashDBM
 * - Better for scan-heavy workloads
 *
 * Note: This class is NOT thread-safe by default. Users must manually
 * call lock()/unlock() or lock_shared()/unlock_shared() when needed.
 */
class TkrzwTreeStorageEngine : public StorageEngine<TkrzwTreeStorageEngine> {
private:
    std::unique_ptr<tkrzw::TreeDBM> db_;
    bool is_open_;
    static std::atomic_int db_counter_;
    static std::string id_;

public:
    /**
     * @brief Constructor - creates an in-memory database
     * @param level The hierarchy level for this storage engine (default: 0)
     *
     * Note: TKRZW TreeDBM doesn't support true in-memory mode.
     * Uses a temporary file for in-memory-like behavior.
     */
    explicit TkrzwTreeStorageEngine(size_t level = 0) :
        StorageEngine<TkrzwTreeStorageEngine>(level),
        db_(std::make_unique<tkrzw::TreeDBM>()), is_open_(false) {
        // TKRZW TreeDBM requires a file path, so we use /tmp for in-memory-like
        // behavior The file will be created but can be considered temporary
        std::string temp_path = std::string("/tmp/repart_kv_storage/") + id_ +
                                std::string("/tkrzw_tree_temp_") +
                                std::to_string(db_counter_.fetch_add(
                                    1, std::memory_order_relaxed)) +
                                ".tkt";
        std::filesystem::create_directories(
            std::string("/tmp/repart_kv_storage/") + id_);

        tkrzw::TreeDBM::TuningParameters tuning_params;
        tuning_params.max_page_size = 8192;
        tuning_params.max_branches = 256;

        tkrzw::Status status =
            db_->OpenAdvanced(temp_path,
                              true,                       // writable
                              tkrzw::File::OPEN_TRUNCATE, // Create new
                              tuning_params);

        if (status == tkrzw::Status::SUCCESS) {
            is_open_ = true;
        }
    }

    /**
     * @brief Constructor with file path - creates a persistent database
     * @param file_path Path to the database file
     * @param max_page_size Maximum page size in bytes (default: 8192)
     * @param max_branches Maximum number of branches per node (default: 256)
     * @param level The hierarchy level for this storage engine (default: 0)
     */
    explicit TkrzwTreeStorageEngine(const std::string &file_path,
                                    int32_t max_page_size = 8192,
                                    int32_t max_branches = 256,
                                    size_t level = 0) :
        StorageEngine<TkrzwTreeStorageEngine>(level),
        db_(std::make_unique<tkrzw::TreeDBM>()), is_open_(false) {

        tkrzw::TreeDBM::TuningParameters tuning_params;
        tuning_params.max_page_size = max_page_size;
        tuning_params.max_branches = max_branches;

        tkrzw::Status status =
            db_->OpenAdvanced(file_path,
                              true,                      // writable
                              tkrzw::File::OPEN_DEFAULT, // file opening options
                              tuning_params);

        if (status == tkrzw::Status::SUCCESS) {
            is_open_ = true;
        }
    }

    /**
     * @brief Destructor - closes the database
     */
    ~TkrzwTreeStorageEngine() {
        if (is_open_) {
            db_->Close();
            is_open_ = false;
        }
    }

    // Disable copy
    TkrzwTreeStorageEngine(const TkrzwTreeStorageEngine &) = delete;
    TkrzwTreeStorageEngine &operator=(const TkrzwTreeStorageEngine &) = delete;

    // Enable move
    TkrzwTreeStorageEngine(TkrzwTreeStorageEngine &&other) noexcept :
        StorageEngine<TkrzwTreeStorageEngine>(other.level_),
        db_(std::move(other.db_)), is_open_(other.is_open_) {
        other.is_open_ = false;
    }

    TkrzwTreeStorageEngine &operator=(TkrzwTreeStorageEngine &&other) noexcept {
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
     * @param value Reference to store the value associated with the key
     * @return Status code indicating the result of the operation
     */
    Status read_impl(const std::string &key, std::string &value) const {
        tkrzw::Status status = db_->Get(key, &value);

        if (status == tkrzw::Status::SUCCESS) {
            return Status::SUCCESS;
        }
        return Status::NOT_FOUND;
    }

    /**
     * @brief Implementation: Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     * @return Status code indicating the result of the operation
     */
    Status write_impl(const std::string &key, const std::string &value) {
        tkrzw::Status status = db_->Set(key, value);
        if (status == tkrzw::Status::SUCCESS) {
            return Status::SUCCESS;
        }
        return Status::ERROR;
    }

    /**
     * @brief Implementation: Scan for key-value pairs from a starting point
     * @param initial_key_prefix The starting key (lower_bound)
     * @param limit Maximum number of key-value pairs to return
     * @param results Reference to store the results of the scan
     * @return Status code indicating the result of the operation
     *
     * Note: TreeDBM maintains sorted order, making this very efficient.
     * We use Jump() to go directly to the first key >= key_prefix.
     */
    Status
    scan_impl(const std::string &initial_key_prefix, size_t limit,
              std::vector<std::pair<std::string, std::string>> &results) const {
        results.reserve(std::min(limit, static_cast<size_t>(1000)));

        // Create an iterator
        auto iter = db_->MakeIterator();

        // For empty prefix, start from beginning; otherwise jump to prefix
        if (initial_key_prefix.empty()) {
            iter->First();
        } else {
            // Jump to the first key >= initial_key_prefix (TreeDBM is sorted)
            iter->Jump(initial_key_prefix);
        }

        // Collect key-value pairs (already in sorted order, no filtering)
        std::string key, value;
        results.resize(limit);
        size_t i = 0;
        while (i < limit) {
            tkrzw::Status status = iter->Get(&key, &value);
            if (status != tkrzw::Status::SUCCESS) {
                break; // No more entries
            }

            results[i] = {key, value};
            ++i;
            // Move to next entry
            if (iter->Next() != tkrzw::Status::SUCCESS) {
                break;
            }
        }
        if (i < limit) {
            results.resize(i);
            if (i == 0) {
                return Status::NOT_FOUND;
            }
        }

        // Results are already sorted (TreeDBM maintains order)
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
     * @return Status code indicating the result of the operation
     */
    Status remove(const std::string &key) {
        tkrzw::Status status = db_->Remove(key);
        if (status == tkrzw::Status::SUCCESS) {
            return Status::SUCCESS;
        }
        return Status::ERROR;
    }
};

// Static member definitions
inline std::atomic_int TkrzwTreeStorageEngine::db_counter_ = 0;
inline std::string TkrzwTreeStorageEngine::id_ = std::to_string(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count());