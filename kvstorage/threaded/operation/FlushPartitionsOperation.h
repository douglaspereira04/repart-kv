#pragma once

#include "Operation.h"
#include "../../../storage/Status.h"
#include <pthread.h>
#include <mutex>
#include <string>

/**
 * Barrier among soft partition workers, then coordinator runs force_sync on the
 * shared storage engine once; caller waits via sync().
 */
class FlushPartitionsOperation : public Operation {
private:
    std::string key_token_;
    pthread_barrier_t worker_barrier_;
    pthread_barrier_t main_barrier_;
    mutable std::mutex status_mutex_;
    Status combined_;

public:
    explicit FlushPartitionsOperation(size_t partition_count) :
        Operation(nullptr, Type::PARTITION_FLUSH), combined_(Status::SUCCESS) {
        key_token_.assign(1, '\0');
        key_ = &key_token_;
        pthread_barrier_init(&worker_barrier_, nullptr,
                             static_cast<unsigned int>(partition_count));
        pthread_barrier_init(&main_barrier_, nullptr,
                             static_cast<unsigned int>(partition_count + 1));
    }

    ~FlushPartitionsOperation() override { destroy_barriers(); }

    FlushPartitionsOperation(const FlushPartitionsOperation &) = delete;
    FlushPartitionsOperation &
    operator=(const FlushPartitionsOperation &) = delete;

    void destroy_barriers() {
        pthread_barrier_destroy(&worker_barrier_);
        pthread_barrier_destroy(&main_barrier_);
    }

    /** Exactly one waiter returns true (coordinator thread for soft flush). */
    bool finish_worker_fence() {
        return pthread_barrier_wait(&worker_barrier_) ==
               PTHREAD_BARRIER_SERIAL_THREAD;
    }

    void sync() { pthread_barrier_wait(&main_barrier_); }

    void combine(Status s) {
        if (s != Status::SUCCESS) {
            std::lock_guard<std::mutex> g(status_mutex_);
            if (combined_ == Status::SUCCESS) {
                combined_ = s;
            }
        }
    }

    Status status() const {
        std::lock_guard<std::mutex> g(status_mutex_);
        return combined_;
    }
};
