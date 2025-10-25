#pragma once

#include "Operation.h"
#include "../future/Future.h"
#include <string>
#include <vector>
#include <pthread.h>

class SyncOperation : public Operation {
private:
    pthread_barrier_t barrier_;

public:
    // Constructor takes references to key and value strings
    SyncOperation(size_t partition_count) : Operation(nullptr, Type::SYNC) {
        pthread_barrier_init(&barrier_, NULL, partition_count);
    }

    // Destructor (default)
    ~SyncOperation() { destroy_barrier(); }

    // Copy constructor and assignment operator are deleted
    // to prevent copying of ScanOperation objects
    SyncOperation(const SyncOperation &) = delete;
    SyncOperation &operator=(const SyncOperation &) = delete;

    // Move constructor and assignment operator are deleted
    // to prevent moving of ScanOperation objects
    SyncOperation(SyncOperation &&) = delete;
    SyncOperation &operator=(SyncOperation &&) = delete;

    // Wait for all partitions to synchronize
    bool sync() {
        return pthread_barrier_wait(&barrier_) == PTHREAD_BARRIER_SERIAL_THREAD;
    }

    // Destroy barrier
    void destroy_barrier() { pthread_barrier_destroy(&barrier_); }
};
