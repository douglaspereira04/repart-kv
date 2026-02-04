#pragma once

#include "KeyStorage.h"
#include "KeyStorageIterator.h"
#include "absl/container/btree_map.h"
#include <string>

// Forward declaration
template <KeyStorageValueType ValueType> class AbslBtreeKeyStorageIterator;

/**
 * @brief absl::btree_map implementation of KeyStorage
 * @tparam ValueType The type of values stored (integral types or pointers)
 *
 * Requires C++20 for concepts and CRTP pattern.
 */
template <KeyStorageValueType ValueType> class AbslBtreeKeyStorage
    : public KeyStorage<AbslBtreeKeyStorage<ValueType>,
                        AbslBtreeKeyStorageIterator<ValueType>, ValueType> {
private:
    absl::btree_map<std::string, ValueType> storage_;

public:
    /**
     * @brief Default constructor
     */
    AbslBtreeKeyStorage() = default;

    /**
     * @brief Destructor
     */
    ~AbslBtreeKeyStorage() = default;

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
     * @brief Implementation: Find the first element with key not less than the
     * given key
     * @param key The key to search for
     * @return Iterator pointing to the found element or end
     */
    AbslBtreeKeyStorageIterator<ValueType>
    lower_bound_impl(const std::string &key);

    /**
     * @brief Get reference to internal storage (for iterator)
     */
    absl::btree_map<std::string, ValueType> &get_storage() { return storage_; }

    /**
     * @brief Get const reference to internal storage (for iterator)
     */
    const absl::btree_map<std::string, ValueType> &get_storage() const {
        return storage_;
    }
};

/**
 * @brief Iterator implementation for AbslBtreeKeyStorage using absl::btree_map
 * @tparam ValueType The type of values stored
 *
 * Requires C++20 for concepts and CRTP pattern.
 */
template <KeyStorageValueType ValueType> class AbslBtreeKeyStorageIterator
    : public KeyStorageIterator<AbslBtreeKeyStorageIterator<ValueType>,
                                ValueType> {
private:
    typename absl::btree_map<std::string, ValueType>::iterator current_;
    typename absl::btree_map<std::string, ValueType>::iterator end_;

public:
    /**
     * @brief Constructor
     * @param current Current iterator position
     * @param end End iterator position
     */
    AbslBtreeKeyStorageIterator(
        typename absl::btree_map<std::string, ValueType>::iterator current,
        typename absl::btree_map<std::string, ValueType>::iterator end) :
        current_(current), end_(end) {}

    /**
     * @brief Implementation: Get the key at the current iterator position
     * @return The key as a string
     */
    std::string get_key_impl() const {
        if (current_ == end_) {
            return "";
        }
        return current_->first;
    }

    /**
     * @brief Implementation: Get the value at the current iterator position
     * @return The value
     */
    ValueType get_value_impl() const {
        if (current_ == end_) {
            return ValueType();
        }
        return current_->second;
    }

    /**
     * @brief Implementation: Increment the iterator to the next element
     */
    void increment_impl() {
        if (current_ != end_) {
            ++current_;
        }
    }

    /**
     * @brief Implementation: Check if this iterator is at the end
     * @return true if at end, false otherwise
     */
    bool is_end_impl() const { return current_ == end_; }
};

// Implementation of lower_bound_impl
template <KeyStorageValueType ValueType> AbslBtreeKeyStorageIterator<ValueType>
AbslBtreeKeyStorage<ValueType>::lower_bound_impl(const std::string &key) {
    auto it = storage_.lower_bound(key);
    return AbslBtreeKeyStorageIterator<ValueType>(it, storage_.end());
}
