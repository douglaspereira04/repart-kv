#pragma once

#include "AsyncWriteOperation.h"

template <typename StorageEngineType> class HardAsyncWriteOperation
    : public AsyncWriteOperation {
private:
    StorageEngineType *storage_;

public:
    HardAsyncWriteOperation(const std::string &key, const std::string &value,
                            StorageEngineType *storage) :
        AsyncWriteOperation(key, value), storage_(storage) {}

    HardAsyncWriteOperation(const HardAsyncWriteOperation &) = delete;
    HardAsyncWriteOperation &
    operator=(const HardAsyncWriteOperation &) = delete;
    HardAsyncWriteOperation(HardAsyncWriteOperation &&) = delete;
    HardAsyncWriteOperation &operator=(HardAsyncWriteOperation &&) = delete;

    StorageEngineType *storage() const { return storage_; }
};
