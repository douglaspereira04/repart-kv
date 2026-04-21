#pragma once

#include "KeyStorage.h"
#include "KeyStorageIterator.h"
#include "KeyStorageValueBinary.h"
#include <tkrzw_dbm_tree.h>
#include <string>
#include <memory>
#include <vector>
#include <string_view>
#include <type_traits>
#include <ctime>
#include <atomic>
#include <chrono>
#include <filesystem>

// Forward declaration
template <KeyStorageValueType ValueType> class TkrzwTreeKeyStorageIterator;

/**
 * @brief TKRZW TreeDBM implementation of KeyStorage
 * @tparam ValueType The type of values stored (integral types or pointers)
 *
 * Uses TKRZW's tree database for sorted key-value storage.
 * TreeDBM maintains keys in sorted order, making lower_bound very efficient.
 * Values are stored as raw sizeof(ValueType) bytes in TKRZW.
 * Requires C++20 for concepts and CRTP pattern.
 */
template <KeyStorageValueType ValueType> class TkrzwTreeKeyStorage
    : public KeyStorage<TkrzwTreeKeyStorage<ValueType>,
                        TkrzwTreeKeyStorageIterator<ValueType>, ValueType> {
private:
    std::unique_ptr<tkrzw::TreeDBM> db_;
    bool is_open_;
    static std::atomic_int db_counter_;
    static std::string id_;

public:
    // Deserialize value bytes (public for iterator access)
    ValueType value_from_bytes(std::string_view bytes) const {
        ValueType out{};
        if (!key_storage_value_from_bytes(bytes, out)) {
            return ValueType{};
        }
        return out;
    }

    /**
     * @brief Constructor - creates an in-memory database
     */
    TkrzwTreeKeyStorage() :
        db_(std::make_unique<tkrzw::TreeDBM>()), is_open_(false) {
        std::string temp_path = std::string("/tmp/repart_kv_keystorage/") +
                                id_ + std::string("/tkrzw_tree_keystorage_") +
                                std::to_string(db_counter_.fetch_add(
                                    1, std::memory_order_relaxed)) +
                                ".tkt";
        std::filesystem::create_directories(
            std::string("/tmp/repart_kv_keystorage/") + id_);

        tkrzw::TreeDBM::TuningParameters tuning_params;
        tuning_params.max_page_size = 8192;
        tuning_params.max_branches = 256;

        tkrzw::Status status = db_->OpenAdvanced(
            temp_path, true, tkrzw::File::OPEN_TRUNCATE, tuning_params);

        if (status == tkrzw::Status::SUCCESS) {
            is_open_ = true;
        }
    }

    /**
     * @brief Destructor
     */
    ~TkrzwTreeKeyStorage() {
        if (is_open_) {
            db_->Close();
            is_open_ = false;
        }
    }

    /**
     * @brief Implementation: Get a value by key
     * @param key The key to look up
     * @param value Output parameter for the retrieved value
     * @return true if the key exists, false otherwise
     */
    bool get_impl(const std::string &key, ValueType &value) const {
        if (!is_open_) {
            return false;
        }

        std::string str_value;
        tkrzw::Status status = db_->Get(key, &str_value);

        if (status == tkrzw::Status::SUCCESS) {
            value = value_from_bytes(str_value);
            return true;
        }
        return false;
    }

    /**
     * @brief Implementation: Put a key-value pair into storage
     * @param key The key to store
     * @param value The value to associate with the key
     */
    void put_impl(const std::string &key, const ValueType &value) {
        if (!is_open_) {
            return;
        }

        db_->Set(key, key_storage_value_as_bytes(value));
    }

    /**
     * @brief Implementation: Get a value by key, or insert it if it doesn't
     * exist
     * @param key The key to look up
     * @param value_to_insert The value to insert if the key doesn't exist
     * @param found_value Output parameter for the retrieved (or inserted) value
     * @return true if the key already existed, false if it was newly inserted
     */
    bool get_or_insert_impl(const std::string &key,
                            const ValueType &value_to_insert,
                            ValueType &found_value) {
        if (!is_open_) {
            found_value = value_to_insert;
            return false;
        }

        std::string str_value;
        tkrzw::Status status = db_->Get(key, &str_value);
        if (status == tkrzw::Status::SUCCESS) {
            found_value = value_from_bytes(str_value);
            return true;
        }

        found_value = value_to_insert;
        db_->Set(key, key_storage_value_as_bytes(value_to_insert), false);
        return false;
    }

    /**
     * @brief Implementation: Scan for key-value pairs from a starting key
     * @param key_start The starting key (first key >= key_start)
     * @param limit Maximum number of pairs to return
     * @param results Output vector of (key, value) pairs
     */
    void scan_impl(const std::string &key_start, size_t limit,
                   std::vector<std::pair<std::string, ValueType>> &results) {
        results.clear();
        if (!is_open_ || limit == 0)
            return;
        auto it = lower_bound_impl(key_start);
        while (!it.is_end() && results.size() < limit) {
            results.emplace_back(it.get_key(), it.get_value());
            ++it;
        }
    }

    /**
     * @brief Implementation: Find the first element with key not less than the
     * given key
     * @param key The key to search for
     * @return Iterator pointing to the found element or end
     *
     * Note: TreeDBM maintains sorted order, making this operation very
     * efficient.
     */
    TkrzwTreeKeyStorageIterator<ValueType>
    lower_bound_impl(const std::string &key);

    /**
     * @brief Get database reference (for iterator)
     */
    tkrzw::TreeDBM &get_db() { return *db_; }

    /**
     * @brief Check if database is open
     */
    bool is_open() const { return is_open_; }
};

/**
 * @brief Iterator implementation for TkrzwTreeKeyStorage
 * @tparam ValueType The type of values stored
 */
template <KeyStorageValueType ValueType> class TkrzwTreeKeyStorageIterator
    : public KeyStorageIterator<TkrzwTreeKeyStorageIterator<ValueType>,
                                ValueType> {
private:
    std::unique_ptr<tkrzw::DBM::Iterator> iter_;
    std::string current_key_;
    std::string current_value_;
    bool at_end_;
    const TkrzwTreeKeyStorage<ValueType> *storage_;

    void load_current() {
        if (!iter_) {
            at_end_ = true;
            return;
        }

        tkrzw::Status status = iter_->Get(&current_key_, &current_value_);
        if (status != tkrzw::Status::SUCCESS) {
            at_end_ = true;
        }
    }

public:
    /**
     * @brief Constructor
     */
    TkrzwTreeKeyStorageIterator(std::unique_ptr<tkrzw::DBM::Iterator> iter,
                                const TkrzwTreeKeyStorage<ValueType> *storage,
                                bool at_end = false) :
        iter_(std::move(iter)), at_end_(at_end), storage_(storage) {
        if (!at_end_ && iter_) {
            load_current();
        }
    }

    /**
     * @brief Implementation: Get the key at the current iterator position
     * @return The key as a string
     */
    std::string get_key_impl() const {
        if (at_end_) {
            return "";
        }
        return current_key_;
    }

    /**
     * @brief Implementation: Get the value at the current iterator position
     * @return The value
     */
    ValueType get_value_impl() const {
        if (at_end_ || !storage_) {
            return ValueType();
        }
        return storage_->value_from_bytes(current_value_);
    }

    /**
     * @brief Implementation: Increment the iterator to the next element
     */
    void increment_impl() {
        if (at_end_ || !iter_) {
            return;
        }

        if (iter_->Next() == tkrzw::Status::SUCCESS) {
            load_current();
        } else {
            at_end_ = true;
        }
    }

    /**
     * @brief Implementation: Check if this iterator is at the end
     * @return true if at end, false otherwise
     */
    bool is_end_impl() const { return at_end_; }
};

// Implementation of lower_bound_impl
template <KeyStorageValueType ValueType> TkrzwTreeKeyStorageIterator<ValueType>
TkrzwTreeKeyStorage<ValueType>::lower_bound_impl(const std::string &key) {
    if (!is_open_) {
        return TkrzwTreeKeyStorageIterator<ValueType>(nullptr, this, true);
    }

    auto iter = db_->MakeIterator();

    // TreeDBM is sorted, so we can jump directly to the key
    if (key.empty()) {
        iter->First();
    } else {
        iter->Jump(key);
    }

    return TkrzwTreeKeyStorageIterator<ValueType>(std::move(iter), this);
}

// Static member definitions
template <KeyStorageValueType ValueType>
inline std::atomic_int TkrzwTreeKeyStorage<ValueType>::db_counter_ = 0;

template <KeyStorageValueType ValueType>
inline std::string TkrzwTreeKeyStorage<ValueType>::id_ = std::to_string(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count());
