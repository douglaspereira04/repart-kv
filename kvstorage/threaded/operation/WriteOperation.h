#pragma once

#include "Operation.h"
#include <string>

class WriteOperation : public Operation {
private:
    std::string* value_;

public:
    // Constructor takes references to key and value strings
    WriteOperation(std::string& key, std::string& value) 
        : Operation(&key, Type::WRITE), value_(&value) {}

    // Destructor (default)
    ~WriteOperation() = default;

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
        return *value_;
    }
};
