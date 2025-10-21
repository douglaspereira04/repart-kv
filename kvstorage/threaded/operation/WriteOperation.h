#pragma once

#include "Operation.h"
#include <string>

class WriteOperation : public Operation {
private:
    std::string value_;

public:
    // Constructor takes references to key and value strings
    WriteOperation(const std::string& key, const std::string& value) 
        : Operation(new std::string(key), Type::WRITE), value_(value) {}

    // Destructor
    ~WriteOperation() {
        // There is no waiting for writes
        // Delete the key string since its owned by the operation
        delete this->key_;
    }

    // Copy constructor and assignment operator are deleted
    // to prevent copying of WriteOperation objects
    WriteOperation(const WriteOperation&) = delete;
    WriteOperation& operator=(const WriteOperation&) = delete;

    // Move constructor and assignment operator are deleted
    // to prevent moving of WriteOperation objects
    WriteOperation(WriteOperation&&) = delete;
    WriteOperation& operator=(WriteOperation&&) = delete;

    // Get value reference method
    std::string& value() {
        return value_;
    }
};
