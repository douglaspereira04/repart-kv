#pragma once

#include <string>

enum class Type {
    READ,
    WRITE,
    SCAN
};

class Request {
private:
    Type type_;
    std::string* key_;

public:
    // Constructor takes a key pointer and type
    Request(std::string* key, Type type) : type_(type), key_(key) {}

    // Destructor (default)
    ~Request() = default;

    // Copy constructor and assignment operator are deleted
    // to prevent copying of Request objects
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    // Move constructor and assignment operator are deleted
    // to prevent moving of Request objects
    Request(Request&&) = delete;
    Request& operator=(Request&&) = delete;

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
