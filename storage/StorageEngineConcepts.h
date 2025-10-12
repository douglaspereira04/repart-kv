#pragma once

#include <string>
#include <vector>
#include <concepts>

/**
 * @brief Concept for types that implement the StorageEngine interface
 */
template<typename T>
concept StorageEngineImpl = requires(T engine, const T constEngine, const std::string& key, const std::string& value, size_t limit) {
    { constEngine.read_impl(key) } -> std::convertible_to<std::string>;
    { engine.write_impl(key, value) } -> std::same_as<void>;
    { constEngine.scan_impl(key, limit) } -> std::convertible_to<std::vector<std::pair<std::string, std::string>>>;
};

