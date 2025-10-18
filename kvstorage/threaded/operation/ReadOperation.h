#pragma once

#include "Operation.h"
#include "../future/Future.h"
#include <string>

class ReadOperation : public Operation {
private:
    Future<std::string> future_;

public:
    // Constructor takes references to key and value strings
    ReadOperation(std::string& key, std::string& value) 
        : Operation(&key, Type::READ), future_(value) {}

    // Destructor (default)
    ~ReadOperation() = default;

    // Copy constructor and assignment operator are deleted
    // to prevent copying of ReadOperation objects
    ReadOperation(const ReadOperation&) = delete;
    ReadOperation& operator=(const ReadOperation&) = delete;

    // Move constructor and assignment operator are deleted
    // to prevent moving of ReadOperation objects
    ReadOperation(ReadOperation&&) = delete;
    ReadOperation& operator=(ReadOperation&&) = delete;

    // Get value reference method
    std::string& value() {
        return future_.value();
    }

    // Notify method that invokes the future notify
    void notify() {
        future_.notify();
    }
};
