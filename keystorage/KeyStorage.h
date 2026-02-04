#pragma once

#include "KeyStorageConcepts.h"
#include <string>

/**
 * @brief CRTP base class for key-value storage
 * @tparam Derived The derived storage type
 * @tparam IteratorType The iterator type for this storage
 * @tparam ValueType The type of values stored (integral types or pointers)
 *
 * This is a generic key-value mapping data structure with string keys
 * and values of integral number types or pointers.
 * Uses Curiously Recurring Template Pattern for compile-time polymorphism
 * without virtual functions. Requires C++20 concepts for type safety.
 */
template <typename Derived, typename IteratorType,
          KeyStorageValueType ValueType>
class KeyStorage {
public:
    /**
     * @brief Get a value by key
     * @param key The key to look up
     * @param value Output parameter for the retrieved value
     * @return true if the key exists, false otherwise
     */
    bool get(const std::string &key, ValueType &value) const {
        return static_cast<const Derived *>(this)->get_impl(key, value);
    }

    /**
     * @brief Put a key-value pair into storage
     * @param key The key to store
     * @param value The value to associate with the key
     */
    void put(const std::string &key, const ValueType &value) {
        static_cast<Derived *>(this)->put_impl(key, value);
    }

    /**
     * @brief Get a value by key, or insert it if it doesn't exist
     * @param key The key to look up
     * @param value_to_insert The value to insert if the key doesn't exist
     * @param found_value Output parameter for the retrieved (or inserted) value
     * @return true if the key already existed, false if it was newly inserted
     */
    bool get_or_insert(const std::string &key, const ValueType &value_to_insert,
                       ValueType &found_value) {
        return static_cast<Derived *>(this)->get_or_insert_impl(
            key, value_to_insert, found_value);
    }

    /**
     * @brief Find the first element with key not less than the given key
     * @param key The key to search for
     * @return Iterator pointing to the found element or end
     */
    IteratorType lower_bound(const std::string &key) {
        return static_cast<Derived *>(this)->lower_bound_impl(key);
    }

protected:
    ~KeyStorage() = default;
};
