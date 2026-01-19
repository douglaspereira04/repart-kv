#pragma once

#include "KeyStorage.h"
#include "KeyStorageIterator.h"
#include <tkrzw_dbm_hash.h>
#include <string>
#include <memory>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <ctime>
#include <vector>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>

// Forward declaration
template <KeyStorageValueType ValueType> class TkrzwHashKeyStorageIterator;

/**
 * @brief TKRZW HashDBM implementation of KeyStorage
 * @tparam ValueType The type of values stored (integral types or pointers)
 *
 * Uses TKRZW's hash database for high-performance key-value storage.
 * Values are serialized to strings for storage in TKRZW.
 * Requires C++20 for concepts and CRTP pattern.
 */
template <KeyStorageValueType ValueType> class TkrzwHashKeyStorage
    : public KeyStorage<TkrzwHashKeyStorage<ValueType>,
                        TkrzwHashKeyStorageIterator<ValueType>, ValueType> {
private:
    std::unique_ptr<tkrzw::HashDBM> db_;
    bool is_open_;
    static std::atomic_int db_counter_;
    static std::string id_;

public:
    // Helper to serialize value to string (public for iterator access)
    std::string value_to_string(const ValueType &value) const {
        if constexpr (std::is_integral_v<ValueType>) {
            return std::to_string(value);
        } else if constexpr (std::is_pointer_v<ValueType>) {
            std::ostringstream oss;
            oss << std::hex << reinterpret_cast<uintptr_t>(value);
            return oss.str();
        }
    }

    // Helper to deserialize string to value (public for iterator access)
    ValueType string_to_value(const std::string &str) const {
        if constexpr (std::is_integral_v<ValueType>) {
            if constexpr (std::is_same_v<ValueType, long> ||
                          std::is_same_v<ValueType, long long>) {
                return static_cast<ValueType>(std::stoll(str));
            } else if constexpr (std::is_same_v<ValueType, unsigned long> ||
                                 std::is_same_v<ValueType,
                                                unsigned long long>) {
                return static_cast<ValueType>(std::stoull(str));
            } else if constexpr (std::is_signed_v<ValueType>) {
                return static_cast<ValueType>(std::stoi(str));
            } else {
                return static_cast<ValueType>(std::stoul(str));
            }
        } else if constexpr (std::is_pointer_v<ValueType>) {
            std::istringstream iss(str);
            uintptr_t ptr_value;
            iss >> std::hex >> ptr_value;
            return reinterpret_cast<ValueType>(ptr_value);
        }
    }

    /**
     * @brief Constructor - creates an in-memory database
     */
    TkrzwHashKeyStorage() :
        db_(std::make_unique<tkrzw::HashDBM>()), is_open_(false) {
        std::string temp_path = std::string("/tmp/repart_kv_keystorage/") +
                                id_ + std::string("/tkrzw_hash_keystorage_") +
                                std::to_string(db_counter_.fetch_add(
                                    1, std::memory_order_relaxed)) +
                                ".tkh";
        std::filesystem::create_directories(
            std::string("/tmp/repart_kv_keystorage/") + id_);

        tkrzw::HashDBM::TuningParameters tuning_params;
        tuning_params.num_buckets = 100000;

        tkrzw::Status status = db_->OpenAdvanced(
            temp_path, true, tkrzw::File::OPEN_TRUNCATE, tuning_params);

        if (status == tkrzw::Status::SUCCESS) {
            is_open_ = true;
        }
    }

    /**
     * @brief Destructor
     */
    ~TkrzwHashKeyStorage() {
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
            value = string_to_value(str_value);
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

        std::string str_value = value_to_string(value);
        db_->Set(key, str_value);
    }

    /**
     * @brief Implementation: Find the first element with key not less than the
     * given key
     * @param key The key to search for
     * @return Iterator pointing to the found element or end
     *
     * Note: HashDBM doesn't maintain sorted order, so this iterates through all
     * keys.
     */
    TkrzwHashKeyStorageIterator<ValueType>
    lower_bound_impl(const std::string &key);

    /**
     * @brief Get database reference (for iterator)
     */
    tkrzw::HashDBM &get_db() { return *db_; }

    /**
     * @brief Check if database is open
     */
    bool is_open() const { return is_open_; }
};

/**
 * @brief Iterator implementation for TkrzwHashKeyStorage
 * @tparam ValueType The type of values stored
 *
 * Note: HashDBM doesn't maintain sorted order, so this iterator:
 * 1. Maintains a sorted list of all keys
 * 2. Iterates through them in order
 * 3. Fetches values from the database on demand
 */
template <KeyStorageValueType ValueType> class TkrzwHashKeyStorageIterator
    : public KeyStorageIterator<TkrzwHashKeyStorageIterator<ValueType>,
                                ValueType> {
private:
    std::shared_ptr<std::vector<std::string>>
        sorted_keys_; // Shared ownership of sorted keys
    size_t current_index_;
    const TkrzwHashKeyStorage<ValueType> *storage_;

public:
    /**
     * @brief Constructor with sorted keys
     */
    TkrzwHashKeyStorageIterator(
        std::shared_ptr<std::vector<std::string>> sorted_keys,
        size_t start_index, const TkrzwHashKeyStorage<ValueType> *storage) :
        sorted_keys_(sorted_keys), current_index_(start_index),
        storage_(storage) {}

    /**
     * @brief Constructor for end iterator
     */
    TkrzwHashKeyStorageIterator(std::nullptr_t,
                                const TkrzwHashKeyStorage<ValueType> *storage,
                                bool) :
        sorted_keys_(nullptr), current_index_(0), storage_(storage) {}

    /**
     * @brief Implementation: Get the key at the current iterator position
     * @return The key as a string
     */
    std::string get_key_impl() const {
        if (!sorted_keys_ || current_index_ >= sorted_keys_->size()) {
            return "";
        }
        return (*sorted_keys_)[current_index_];
    }

    /**
     * @brief Implementation: Get the value at the current iterator position
     * @return The value
     */
    ValueType get_value_impl() const {
        if (!sorted_keys_ || current_index_ >= sorted_keys_->size() ||
            !storage_) {
            return ValueType();
        }

        ValueType value;
        if (storage_->get((*sorted_keys_)[current_index_], value)) {
            return value;
        }
        return ValueType();
    }

    /**
     * @brief Implementation: Increment the iterator to the next element
     */
    void increment_impl() {
        if (sorted_keys_ && current_index_ < sorted_keys_->size()) {
            current_index_++;
        }
    }

    /**
     * @brief Implementation: Check if this iterator is at the end
     * @return true if at end, false otherwise
     */
    bool is_end_impl() const {
        return !sorted_keys_ || current_index_ >= sorted_keys_->size();
    }

private:
    // Make string_to_value accessible to iterator
    friend class TkrzwHashKeyStorage<ValueType>;
};

// Implementation of lower_bound_impl
template <KeyStorageValueType ValueType> TkrzwHashKeyStorageIterator<ValueType>
TkrzwHashKeyStorage<ValueType>::lower_bound_impl(const std::string &key) {
    if (!is_open_) {
        return TkrzwHashKeyStorageIterator<ValueType>(nullptr, this, true);
    }

    // HashDBM doesn't maintain sorted order, so we need to:
    // 1. Collect all keys
    // 2. Sort them
    // 3. Find the lower_bound position
    // 4. Create an iterator with the sorted keys starting from that position

    auto sorted_keys = std::make_shared<std::vector<std::string>>();
    auto temp_iter = db_->MakeIterator();
    temp_iter->First();

    std::string temp_key, temp_value;
    while (temp_iter->Get(&temp_key, &temp_value) == tkrzw::Status::SUCCESS) {
        sorted_keys->push_back(temp_key);
        if (temp_iter->Next() != tkrzw::Status::SUCCESS) {
            break;
        }
    }

    // Sort all keys
    std::sort(sorted_keys->begin(), sorted_keys->end());

    // Find lower_bound position
    auto lb_pos =
        std::lower_bound(sorted_keys->begin(), sorted_keys->end(), key);

    if (lb_pos == sorted_keys->end()) {
        // No key >= the given key, return end iterator
        return TkrzwHashKeyStorageIterator<ValueType>(nullptr, this, true);
    }

    // Calculate the index position
    size_t index = std::distance(sorted_keys->begin(), lb_pos);

    // Return iterator starting at the lower_bound position
    return TkrzwHashKeyStorageIterator<ValueType>(sorted_keys, index, this);
}

// Static member definitions
template <KeyStorageValueType ValueType>
inline std::atomic_int TkrzwHashKeyStorage<ValueType>::db_counter_ = 0;

template <KeyStorageValueType ValueType>
inline std::string TkrzwHashKeyStorage<ValueType>::id_ = std::to_string(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count());
