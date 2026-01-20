#pragma once

#include "ReadOperation.h"
#include "../../storage/StorageEngine.h"

template <typename StorageEngineType> class HardReadOperation : public ReadOperation {
private:
    StorageEngineType *storage_;

public:
    // Constructor takes references to key, value, and storage pointer
    HardReadOperation(const std::string &key, std::string &value,
                      StorageEngineType *storage) :
        ReadOperation(key, value), storage_(storage) {}

    // Destructor (default)
    ~HardReadOperation() = default;

    // Copy constructor and assignment operator are deleted
    HardReadOperation(const HardReadOperation &) = delete;
    HardReadOperation &operator=(const HardReadOperation &) = delete;

    // Move constructor and assignment operator are deleted
    HardReadOperation(HardReadOperation &&) = delete;
    HardReadOperation &operator=(HardReadOperation &&) = delete;

    // Get storage pointer
    StorageEngineType *storage() const { return storage_; }
};
