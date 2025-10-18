#pragma once

#include <string>

enum class Type {
    READ,
    WRITE,
    SCAN
};

class Operation {
private:
    Type type_;
    std::string* key_;

public:
    // Constructor takes a key pointer and type
    Operation(std::string* key, Type type) : type_(type), key_(key) {}

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
};
