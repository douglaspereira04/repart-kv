#pragma once

#include "Operation.h"
#include "../future/Future.h"
#include <string>
#include <vector>
#include <pthread.h>

class ScanOperation : public Operation {
private:
    Future<std::vector<std::pair<std::string, std::string>>> future_;
    pthread_barrier_t barrier_;

public:
    // Constructor takes references to key and value strings
    ScanOperation(const std::string& key, std::vector<std::pair<std::string, std::string>>& values, size_t partition_count) 
        : Operation(const_cast<std::string*>(&key), Type::SCAN), future_(values) {
            pthread_barrier_init(&barrier_, NULL, partition_count);
        }

    // Destructor (default)
    ~ScanOperation() = default;

    // Copy constructor and assignment operator are deleted
    // to prevent copying of ScanOperation objects
    ScanOperation(const ScanOperation&) = delete;
    ScanOperation& operator=(const ScanOperation&) = delete;

    // Move constructor and assignment operator are deleted
    // to prevent moving of ScanOperation objects
    ScanOperation(ScanOperation&&) = delete;
    ScanOperation& operator=(ScanOperation&&) = delete;

    // Get value reference method
    std::vector<std::pair<std::string, std::string>>& values() {
        return future_.value();
    }

    // Notify method that invokes the future notify
    void notify() {
        future_.notify();
    }

    // Wait for all partitions to synchronize
    bool is_coordinator() {
        return pthread_barrier_wait(&barrier_) == PTHREAD_BARRIER_SERIAL_THREAD;
    }

    // Destroy barrier method
    void destroy_barrier() {
        pthread_barrier_destroy(&barrier_);
    }

    void wait() {
        future_.wait();
    }
};
