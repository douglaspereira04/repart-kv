#pragma once

#include "ScanOperation.h"
#include "../../storage/StorageEngine.h"
#include <vector>

template <typename StorageEngineType> class HardScanOperation
    : public ScanOperation {
private:
    std::vector<StorageEngineType *> storages_;
    std::vector<size_t> partition_array_;

public:
    // Constructor takes references to key, values, partition count, storage
    // pointers, and partition array
    HardScanOperation(const std::string &key,
                      std::vector<std::pair<std::string, std::string>> &values,
                      size_t partition_count,
                      const std::vector<StorageEngineType *> &storages,
                      const std::vector<size_t> &partition_array) :
        ScanOperation(key, values, partition_count), storages_(storages),
        partition_array_(partition_array) {}

    // Destructor (default)
    ~HardScanOperation() = default;

    // Copy constructor and assignment operator are deleted
    HardScanOperation(const HardScanOperation &) = delete;
    HardScanOperation &operator=(const HardScanOperation &) = delete;

    // Move constructor and assignment operator are deleted
    HardScanOperation(HardScanOperation &&) = delete;
    HardScanOperation &operator=(HardScanOperation &&) = delete;

    // Get storage pointers
    const std::vector<StorageEngineType *> &storages() const {
        return storages_;
    }

    // Get partition array
    const std::vector<size_t> &partition_array() const {
        return partition_array_;
    }
};
