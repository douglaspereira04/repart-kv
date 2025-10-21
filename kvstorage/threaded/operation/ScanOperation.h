#pragma once

#include "Operation.h"
#include "../future/Future.h"
#include <string>
#include <vector>
#include <pthread.h>

class ScanOperation : public Operation {
private:
    std::vector<std::pair<std::string, std::string>> *results_;
    pthread_barrier_t barrier_;
    pthread_barrier_t called_barrier_;

public:
    // Constructor takes references to key and value strings
    ScanOperation(const std::string& key, std::vector<std::pair<std::string, std::string>>& values, size_t partition_count) 
        : Operation(const_cast<std::string*>(&key), Type::SCAN), results_(&values) {
            pthread_barrier_init(&barrier_, NULL, partition_count);
            pthread_barrier_init(&called_barrier_, NULL, partition_count+1); // +1 for the caller
        }

    // Destructor (default)
    ~ScanOperation() {
        destroy_barriers();
    }

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
        return *results_;
    }

    // Destroy barriers
    void destroy_barriers() {
        pthread_barrier_destroy(&barrier_);
        pthread_barrier_destroy(&called_barrier_);
    }

    // Sync workers returning coordinator
    bool is_coordinator() {
        return pthread_barrier_wait(&barrier_) == PTHREAD_BARRIER_SERIAL_THREAD;
    }

    // Get limit
    size_t limit() {
        return results_->size();
    }

    // Sync workers with caller
    void sync() {
        pthread_barrier_wait(&called_barrier_);
    }
};
