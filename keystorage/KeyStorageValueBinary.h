#pragma once

#include "KeyStorageConcepts.h"
#include <cstring>
#include <memory>
#include <string_view>
#include <type_traits>

/**
 * @brief View over the object representation of a KeyStorage value (no text
 * encoding).
 */
template <KeyStorageValueType T> [[nodiscard]] inline std::string_view
key_storage_value_as_bytes(const T &value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    return std::string_view(
        reinterpret_cast<const char *>(std::addressof(value)), sizeof(T));
}

/**
 * @brief Restore a value from bytes produced by key_storage_value_as_bytes.
 * @return false if byte length does not match sizeof(T)
 */
template <KeyStorageValueType T> [[nodiscard]] inline bool
key_storage_value_from_bytes(std::string_view bytes, T &out) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    if (bytes.size() != sizeof(T)) {
        return false;
    }
    std::memcpy(std::addressof(out), bytes.data(), sizeof(T));
    return true;
}
