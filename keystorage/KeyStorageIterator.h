#pragma once

#include "KeyStorageConcepts.h"
#include <string>

/**
 * @brief CRTP base class for KeyStorage iterators
 * @tparam Derived The derived iterator type
 * @tparam ValueType The type of values stored (integral types or pointers)
 * 
 * Uses Curiously Recurring Template Pattern for compile-time polymorphism
 * without virtual functions. Requires C++20 concepts for type safety.
 */
template<typename Derived, KeyStorageValueType ValueType>
class KeyStorageIterator {
public:
    /**
     * @brief Get the key at the current iterator position
     * @return The key as a string
     */
    std::string get_key() const {
        return static_cast<const Derived*>(this)->get_key_impl();
    }

    /**
     * @brief Get the value at the current iterator position
     * @return The value
     */
    ValueType get_value() const {
        return static_cast<const Derived*>(this)->get_value_impl();
    }

    /**
     * @brief Increment the iterator to the next element
     * @return Reference to the derived iterator
     */
    Derived& operator++() {
        static_cast<Derived*>(this)->increment_impl();
        return *static_cast<Derived*>(this);
    }

    /**
     * @brief Check if this iterator is at the end
     * @return true if at end, false otherwise
     */
    bool is_end() const {
        return static_cast<const Derived*>(this)->is_end_impl();
    }

protected:
    ~KeyStorageIterator() = default;
};

