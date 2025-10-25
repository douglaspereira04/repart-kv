#pragma once

#include <thread>
#include <semaphore>
#include <boost/lockfree/spsc_queue.hpp>
#include "kvstorage/threaded/operation/DoneOperation.h"
#include "operation/Operation.h"
#include "operation/ReadOperation.h"
#include "operation/SyncOperation.h"
#include "operation/WriteOperation.h"
#include "operation/ScanOperation.h"

/**
 * @brief Worker class for processing operations in a soft partition
 *
 * This class manages a queue of operations and processes them in a separate
 * thread. It uses semaphores to control queue capacity and ensure thread-safe
 * operation processing.
 *
 * @tparam StorageEngineType The storage engine type (must derive from
 * StorageEngine)
 * @tparam Q Maximum queue size for operations
 */
template <typename StorageEngineType, size_t Q> class SoftPartitionWorker {
private:
    StorageEngineType &storage_; // Storage engine reference
    boost::lockfree::spsc_queue<Operation *>
        queue_; // Lock-free SPSC queue of operations with capacity Q
    std::counting_semaphore<Q>
        available_sem_; // Semaphore with permits equal to items in queue
    std::counting_semaphore<Q>
        free_sem_;              // Semaphore with permits equal to free spaces
    std::thread worker_thread_; // Thread running worker_loop method

public:
    /**
     * @brief Constructor
     * @param storage Reference to the storage engine
     */
    explicit SoftPartitionWorker(StorageEngineType &storage) :
        storage_(storage), queue_(Q), // Initialize SPSC queue with capacity Q
        available_sem_(0),            // Initially no operations available
        free_sem_(Q),                 // Initially Q free spaces available
        worker_thread_(std::thread(&SoftPartitionWorker::worker_loop, this)) {}

    /**
     * @brief Destructor - stops the worker thread
     */
    ~SoftPartitionWorker() {
        // Signal the worker thread to stop by enqueueing a special operation
        stop();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    /**
     * @brief Read operation
     * @param operation The read operation to perform
     */
    void read(ReadOperation *operation) {
        Status status = storage_.read(operation->key(), operation->value());
        operation->status(status);
        operation->notify();
    }

    /**
     * @brief Write operation
     * @param operation The read operation to perform
     */
    void write(WriteOperation *operation) {
        storage_.write(operation->key(), operation->value());
        delete operation;
    }

    /**
     * @brief Scan operation
     * @param operation The scan operation to perform
     */
    void scan(ScanOperation *operation) {
        bool is_coordinator = operation->is_coordinator();
        if (is_coordinator) {
            size_t limit = operation->limit();
            Status status =
                storage_.scan(operation->key(), limit, operation->values());
            operation->status(status);
        }
        operation->sync();
    }

    /**
     * @brief Sync operation
     * @param operation The sync operation to perform
     */
    void sync(SyncOperation *operation) {
        bool is_coordinator = operation->sync();
        if (is_coordinator) {
            delete operation;
        }
    }

    /**
     * @brief Dequeue an operation from the queue
     * @param operation Output parameter to store the dequeued operation
     *
     * Waits for an available operation, removes it from the queue, and signals
     * a free space.
     */
    void dequeue(Operation *&operation) {
        // Wait for an available operation
        available_sem_.acquire();

        // Dequeue from SPSC queue (lock-free)
        if (!queue_.pop(operation)) {
            // SPSC queue pop can fail if queue is empty, but we have semaphore
            // so this should not happen
            throw std::runtime_error("SPSC queue pop failed");
        }

        // Signal that a free space is available
        free_sem_.release();
    }

    /**
     * @brief Enqueue an operation to the queue
     * @param operation The operation to enqueue
     *
     * Waits for a free space in the queue, inserts the operation, and signals
     * an available operation.
     */
    void enqueue(Operation *operation) {
        // Wait for a free space in the queue
        free_sem_.acquire();

        // Enqueue to SPSC queue (lock-free)
        if (!queue_.push(operation)) {
            // SPSC queue push can fail if queue is full, but we have semaphore
            // so this should not happen, but we'll retry just in case
            throw std::runtime_error("SPSC queue push failed");
        }

        // Signal that an operation is available
        available_sem_.release();
    }

    void stop() {
        DoneOperation operation;
        enqueue(&operation);
        operation.wait();
    }

    /**
     * @brief Worker loop that processes operations from the queue
     *
     * Continuously dequeues operations and processes them based on their type.
     * Calls the respective function (read, write, or scan) for each operation.
     */
    void worker_loop() {
        Operation *operation;
        while (true) {
            dequeue(operation);
            // Process the operation based on its type
            switch (operation->type()) {
                case Type::READ:
                    read(static_cast<ReadOperation *>(operation));
                    break;
                case Type::WRITE:
                    write(static_cast<WriteOperation *>(operation));
                    break;
                case Type::SCAN:
                    scan(static_cast<ScanOperation *>(operation));
                    break;
                case Type::SYNC:
                    sync(static_cast<SyncOperation *>(operation));
                    break;
                case Type::DONE:
                    static_cast<DoneOperation *>(operation)->wait();
                    return;
                default:
                    break;
            }
        }
    }
};
