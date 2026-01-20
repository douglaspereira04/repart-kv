#pragma once

#include "WriteOperation.h"
#include "../../storage/StorageEngine.h"

template <typename StorageEngineType> class HardWriteOperation : public WriteOperation {
private:
    StorageEngineType *storage_;

public:
    // Constructor takes references to key, value, and storage pointer
    HardWriteOperation(const std::string &key, const std::string &value,
                       StorageEngineType *storage) :
        WriteOperation(key, value), storage_(storage) {}

    // Destructor (default)
    ~HardWriteOperation() = default;

    // Copy constructor and assignment operator are deleted
    HardWriteOperation(const HardWriteOperation &) = delete;
    HardWriteOperation &operator=(const HardWriteOperation &) = delete;

    // Move constructor and assignment operator are deleted
    HardWriteOperation(HardWriteOperation &&) = delete;
    HardWriteOperation &operator=(HardWriteOperation &&) = delete;

    // Get storage pointer
    StorageEngineType *storage() const { return storage_; }
};
