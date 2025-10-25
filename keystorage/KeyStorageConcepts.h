#pragma once

#include <string>
#include <concepts>

/**
 * @brief Concept for valid KeyStorage value types (integral or pointer types)
 */
template <typename T>
concept KeyStorageValueType = std::integral<T> || std::is_pointer_v<T>;
