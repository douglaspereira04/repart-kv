#pragma once

#include <string>
#include <vector>
#include <concepts>
#include "Status.h"

/**
 * @brief Concept for types that implement the StorageEngine interface
 */
template<typename T>
concept StorageEngineImpl = requires(T engine, const T constEngine, const std::string& key, const std::string& value, size_t limit, std::vector<std::pair<std::string, std::string>>& results) {
    { constEngine.read_impl(key, value) } -> std::same_as<Status>;
    { engine.write_impl(key, value) } -> std::same_as<Status>;
    { constEngine.scan_impl(key, limit, results) } -> std::same_as<Status>;
};

