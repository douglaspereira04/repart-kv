#pragma once

#include "KeyStorage.h"
#include "KeyStorageIterator.h"
#include <ankerl/unordered_dense.h>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

// Forward declaration
template <KeyStorageValueType ValueType> class UnorderedDenseKeyStorageIterator;

/**
 * @brief ankerl::unordered_dense::map implementation of KeyStorage
 * @tparam ValueType The type of values stored (integral types or pointers)
 *
 * Uses ankerl::unordered_dense::map for high-performance hash-based storage.
 * Requires C++20 for concepts and CRTP pattern.
 * Note: unordered_dense::map doesn't maintain sorted order, so lower_bound
 * collects and sorts keys similar to TkrzwHashKeyStorage.
 */
template <KeyStorageValueType ValueType> class UnorderedDenseKeyStorage
    : public KeyStorage<UnorderedDenseKeyStorage<ValueType>,
                        UnorderedDenseKeyStorageIterator<ValueType>,
                        ValueType> {
private:
    ankerl::unordered_dense::map<std::string, ValueType> storage_;

public:
    /**
     * @brief Default constructor
     */
    UnorderedDenseKeyStorage() = default;

    /**
     * @brief Destructor
     */
    ~UnorderedDenseKeyStorage() = default;

    /**
     * @brief Implementation: Get a value by key
     * @param key The key to look up
     * @param value Output parameter for the retrieved value
     * @return true if the key exists, false otherwise
     */
    bool get_impl(const std::string &key, ValueType &value) const {
        auto it = storage_.find(key);
        if (it != storage_.end()) {
            value = it->second;
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
        storage_[key] = value;
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
        auto [it, inserted] = storage_.try_emplace(key, value_to_insert);
        found_value = it->second;
        return !inserted;
    }

    /**
     * @brief Implementation: Find the first element with key not less than the
     * given key
     * @param key The key to search for
     * @return Iterator pointing to the found element or end
     *
     * Note: unordered_dense::map doesn't maintain sorted order, so this:
     * 1. Collects all keys
     * 2. Sorts them
     * 3. Finds the lower_bound position
     * 4. Creates an iterator with the sorted keys starting from that position
     */
    UnorderedDenseKeyStorageIterator<ValueType>
    lower_bound_impl(const std::string &key);

    /**
     * @brief Get reference to internal storage (for iterator)
     */
    ankerl::unordered_dense::map<std::string, ValueType> &get_storage() {
        return storage_;
    }

    /**
     * @brief Get const reference to internal storage (for iterator)
     */
    const ankerl::unordered_dense::map<std::string, ValueType> &
    get_storage() const {
        return storage_;
    }
};

/**
 * @brief Iterator implementation for UnorderedDenseKeyStorage
 * @tparam ValueType The type of values stored
 *
 * Note: unordered_dense::map doesn't maintain sorted order, so this iterator:
 * 1. Maintains a sorted list of all keys
 * 2. Iterates through them in order
 * 3. Fetches values from the storage on demand
 */
template <KeyStorageValueType ValueType> class UnorderedDenseKeyStorageIterator
    : public KeyStorageIterator<UnorderedDenseKeyStorageIterator<ValueType>,
                                ValueType> {
private:
    std::shared_ptr<std::vector<std::string>>
        sorted_keys_; // Shared ownership of sorted keys
    size_t current_index_;
    const UnorderedDenseKeyStorage<ValueType> *storage_;

public:
    /**
     * @brief Constructor with sorted keys
     */
    UnorderedDenseKeyStorageIterator(
        std::shared_ptr<std::vector<std::string>> sorted_keys,
        size_t start_index,
        const UnorderedDenseKeyStorage<ValueType> *storage) :
        sorted_keys_(sorted_keys), current_index_(start_index),
        storage_(storage) {}

    /**
     * @brief Constructor for end iterator
     */
    UnorderedDenseKeyStorageIterator(
        std::nullptr_t, const UnorderedDenseKeyStorage<ValueType> *storage,
        bool) : sorted_keys_(nullptr), current_index_(0), storage_(storage) {}

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
};

// Implementation of lower_bound_impl
template <KeyStorageValueType ValueType>
UnorderedDenseKeyStorageIterator<ValueType>
UnorderedDenseKeyStorage<ValueType>::lower_bound_impl(const std::string &key) {
    // unordered_dense::map doesn't maintain sorted order, so we need to:
    // 1. Collect all keys
    // 2. Sort them
    // 3. Find the lower_bound position
    // 4. Create an iterator with the sorted keys starting from that position

    auto sorted_keys = std::make_shared<std::vector<std::string>>();

    // Collect all keys from the map
    for (const auto &pair : storage_) {
        sorted_keys->push_back(pair.first);
    }

    // Sort all keys
    std::sort(sorted_keys->begin(), sorted_keys->end());

    // Find lower_bound position
    auto lb_pos =
        std::lower_bound(sorted_keys->begin(), sorted_keys->end(), key);

    if (lb_pos == sorted_keys->end()) {
        // No key >= the given key, return end iterator
        return UnorderedDenseKeyStorageIterator<ValueType>(nullptr, this, true);
    }

    // Calculate the index position
    size_t index = std::distance(sorted_keys->begin(), lb_pos);

    // Return iterator starting at the lower_bound position
    return UnorderedDenseKeyStorageIterator<ValueType>(sorted_keys, index,
                                                       this);
}
