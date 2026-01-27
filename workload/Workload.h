#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace workload {

// Default value for write operations (1 KB)
inline const std::string DEFAULT_VALUE(1 * 1024, '*');

/**
 * @brief Operation types for the workload
 */
enum class OperationType {
    READ = 0,
    WRITE = 1,
    SCAN = 2
};

/**
 * @brief Represents a single operation in the workload
 */
struct Operation {
    OperationType type;
    std::string key;
    std::string value; // For writes (1024-byte value if not specified)
    size_t limit;      // For scans

    Operation(OperationType t, const std::string &k, size_t l = 0) :
        type(t), key(k), limit(l) {}
};

/**
 * @brief Read and parse a workload file
 * @param filepath Path to the workload file
 * @return Vector of operations
 *
 * File format:
 * - 0,<key>         : READ operation
 * - 1,<key>         : WRITE operation (with 1KB default value)
 * - 2,<key>,<limit> : SCAN operation
 */
inline std::vector<Operation> read_workload(const std::string &filepath) {
    std::vector<Operation> operations;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open workload file: " + filepath);
    }

    std::string line;
    size_t line_number = 0;

    while (std::getline(file, line)) {
        line_number++;

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::string command_str, key, limit_str;

        // Parse command
        if (!std::getline(iss, command_str, ',')) {
            std::cerr << "Warning: Skipping malformed line " << line_number
                      << ": " << line << std::endl;
            continue;
        }

        // Parse key
        if (!std::getline(iss, key, ',')) {
            std::cerr << "Warning: Skipping malformed line " << line_number
                      << " (missing key): " << line << std::endl;
            continue;
        }

        int command = std::stoi(command_str);

        switch (command) {
            case 0: // READ
                operations.emplace_back(OperationType::READ, key);
                break;

            case 1: // WRITE
                // Use fixed 1KB default value for writes
                operations.emplace_back(OperationType::WRITE, key);
                break;

            case 2: // SCAN
                if (!std::getline(iss, limit_str, ',')) {
                    std::cerr << "Warning: Skipping malformed line "
                              << line_number
                              << " (SCAN missing limit): " << line << std::endl;
                    continue;
                }
                try {
                    size_t limit = std::stoull(limit_str);
                    operations.emplace_back(OperationType::SCAN, key, limit);
                } catch (const std::exception &e) {
                    std::cerr << "Warning: Skipping line " << line_number
                              << " (invalid limit): " << line << std::endl;
                }
                break;

            default:
                std::cerr << "Warning: Skipping line " << line_number
                          << " (unknown command " << command << "): " << line
                          << std::endl;
                break;
        }
    }

    file.close();
    return operations;
}

} // namespace workload
