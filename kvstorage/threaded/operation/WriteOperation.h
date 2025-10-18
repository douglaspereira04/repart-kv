#pragma once

#include "Operation.h"
#include <string>

template<typename StorageType>
class WriteOperation : public Operation<StorageType> {
private:
    std::string* value_;

public:
    // Constructor takes references to key and value strings
    WriteOperation(std::string& key, std::string& value) 
        : Operation<StorageType>(&key, Type::WRITE), value_(&value) {}

    // Constructor takes references to key, value strings, and storage engine
    WriteOperation(std::string& key, std::string& value, StorageEngine<StorageType>& storage) 
        : Operation<StorageType>(&key, Type::WRITE, storage), value_(&value) {}

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
