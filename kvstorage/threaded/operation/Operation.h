#pragma once

#include "storage/Status.h"
#include <string>

enum class Type {
    READ,
    WRITE,
    SCAN,
    DONE
};

class Operation {
private:
    Type type_;
    std::string* key_;
    Status status_;

public:
    // Constructor takes a key pointer and type
    Operation(std::string* key, Type type) : type_(type), key_(key), status_(Status::PENDING) {}

    // Destructor (default)
    ~Operation() = default;

    // Copy constructor and assignment operator are deleted
    // to prevent copying of Operation objects
    Operation(const Operation&) = delete;
    Operation& operator=(const Operation&) = delete;

    // Move constructor and assignment operator are deleted
    // to prevent moving of Operation objects
    Operation(Operation&&) = delete;
    Operation& operator=(Operation&&) = delete;

    Type type() const {
        return type_;
    }

    const std::string& key() const {
        return *key_;
    }

    const std::string& key(size_t idx) const {
        return key_[idx];
    }

    Status status() const {
        return status_;
    }

    void status(Status status) {
        status_ = status;
    }
};
