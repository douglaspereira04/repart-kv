#pragma once

#include "Operation.h"
#include "../../../storage/Status.h"
#include <pthread.h>
#include <mutex>
#include <string>
#include <vector>

/**
 * Barrier among workers; each flushes
 * storages_snapshot_[partition_worker_index] using
 * StorageEngineType::force_sync(), then caller waits via sync().
 */
template <typename StorageEngineType> class HardFlushPartitionsOperation
    : public Operation {
private:
    std::string key_token_;
    pthread_barrier_t worker_barrier_;
    pthread_barrier_t main_barrier_;
    std::vector<StorageEngineType *> storages_snapshot_;
    mutable std::mutex status_mutex_;
    Status combined_;

public:
    HardFlushPartitionsOperation(
        size_t partition_count,
        std::vector<StorageEngineType *> storages_snapshot) :
        Operation(nullptr, Type::PARTITION_FLUSH),
        storages_snapshot_(std::move(storages_snapshot)),
        combined_(Status::SUCCESS) {
        key_token_.assign(1, '\0');
        key_ = &key_token_;
        pthread_barrier_init(&worker_barrier_, nullptr,
                             static_cast<unsigned int>(partition_count));
        pthread_barrier_init(&main_barrier_, nullptr,
                             static_cast<unsigned int>(partition_count + 1));
    }

    ~HardFlushPartitionsOperation() override { destroy_barriers(); }

    HardFlushPartitionsOperation(const HardFlushPartitionsOperation &) = delete;
    HardFlushPartitionsOperation &
    operator=(const HardFlushPartitionsOperation &) = delete;

    void destroy_barriers() {
        pthread_barrier_destroy(&worker_barrier_);
        pthread_barrier_destroy(&main_barrier_);
    }

    void finish_worker_fence() { (void)pthread_barrier_wait(&worker_barrier_); }

    void sync() { pthread_barrier_wait(&main_barrier_); }

    const std::vector<StorageEngineType *> &storages() const {
        return storages_snapshot_;
    }

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
