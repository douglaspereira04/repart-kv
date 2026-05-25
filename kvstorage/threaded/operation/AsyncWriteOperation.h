#pragma once

#include "Operation.h"
#include <string>

class AsyncWriteOperation : public Operation {
private:
    std::string value_;

public:
    AsyncWriteOperation(const std::string &key, const std::string &value) :
        Operation(new std::string(key), Type::ASYNC_WRITE), value_(value) {}

    virtual ~AsyncWriteOperation() override { delete key_; }

    AsyncWriteOperation(const AsyncWriteOperation &) = delete;
    AsyncWriteOperation &operator=(const AsyncWriteOperation &) = delete;
    AsyncWriteOperation(AsyncWriteOperation &&) = delete;
    AsyncWriteOperation &operator=(AsyncWriteOperation &&) = delete;

    std::string &value() { return value_; }
};
