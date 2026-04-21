#pragma once

#include <type_traits>

/**
 * @brief Concept for valid KeyStorage value types.
 *
 * Values may be scalars, pointers, or trivial structs (e.g. fixed-size POD
 * aggregates) that round-trip via memcpy of their object representation; see
 * KeyStorageValueBinary.h. Pointer fields persist address bits only, not
 * pointed-to storage.
 *
 * Default construction is required for iterator / error sentinel paths.
 */
template <typename T>
concept KeyStorageValueType =
    std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>;
