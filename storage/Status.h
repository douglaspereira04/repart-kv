#pragma once
#include <string>

/**
 * @brief Status codes for storage engine operations
 */
enum class Status {
    SUCCESS,
    NOT_FOUND,
    ERROR,
};

/**
 * @brief Convert Status enum to string representation
 * @param status The status to convert
 * @return String representation of the status
 */
inline std::string to_string(Status status) {
    switch (status) {
        case Status::SUCCESS:
            return "SUCCESS";
        case Status::NOT_FOUND:
            return "NOT_FOUND";
        case Status::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}