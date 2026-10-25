#pragma once

#include <chrono>
#include <cstdlib>
#include <string>

/**
 * Directory name under repart_kv_storage / repart_kv_keystorage.
 * REPART_KV_STORAGE_ID pins it so a later process can reopen a copied tree.
 */
inline std::string repart_kv_instance_id() {
    if (const char *fixed = std::getenv("REPART_KV_STORAGE_ID")) {
        if (fixed[0] != '\0') {
            return std::string(fixed);
        }
    }
    return std::to_string(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count());
}

/** When set, engine destructors leave database files on disk. */
inline bool repart_kv_keep_storage() {
    const char *value = std::getenv("REPART_KV_KEEP_STORAGE");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
}
