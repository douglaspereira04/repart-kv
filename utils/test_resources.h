#pragma once

#include <filesystem>
#include <string>

namespace repart_kv_test {

/**
 * @brief Absolute path to \c ./test_resources under the process current
 *        directory. The directory is created on first use. Use for key
 *        storages, KV partition paths, and storage engines in tests so
 *        artifacts stay under one tree.
 */
[[nodiscard]] inline const std::string &test_resources_dir() {
    static const std::string dir = [] {
        std::filesystem::path p =
            std::filesystem::current_path() / "test_resources";
        std::filesystem::create_directories(p);
        return p.string();
    }();
    return dir;
}

} // namespace repart_kv_test
