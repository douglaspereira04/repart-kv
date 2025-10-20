#pragma once

#include "Operation.h"
#include <pthread.h>

class DoneOperation : public Operation {
private:
    pthread_barrier_t barrier_;

public:
    // Constructor takes references to key and value strings
    DoneOperation() 
        : Operation(nullptr, Type::DONE) {
            pthread_barrier_init(&barrier_, NULL, 2);
        }

    // Destructor (default)
    ~DoneOperation() = default;

    // Copy constructor and assignment operator are deleted
    // to prevent copying of DoneOperation objects
    DoneOperation(const DoneOperation&) = delete;
    DoneOperation& operator=(const DoneOperation&) = delete;

    // Move constructor and assignment operator are deleted
    // to prevent moving of ScanOperation objects
    DoneOperation(DoneOperation&&) = delete;
    DoneOperation& operator=(DoneOperation&&) = delete;

    // Wait method that waits for the barrier
    void wait() {
        pthread_barrier_wait(&barrier_);
    }
};
