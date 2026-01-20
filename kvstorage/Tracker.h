#pragma once

#include "../graph/Graph.h"
#include "../graph/MetisGraph.h"
#include <tbb/concurrent_queue.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <utility>

/**
 * @brief Tracker class for tracking key access patterns
 *
 * This class manages a queue of key-value pairs and processes them in a
 * background thread. It uses TBB's concurrent_bounded_queue which provides
 * built-in blocking behavior for coordination between the producer (update
 * method) and consumer (tracking_loop method).
 */
template <typename PartitionMapType> class Tracker {
private:
    tbb::concurrent_bounded_queue<std::vector<std::string>>
        queue_;    // High-performance thread-safe bounded queue from TBB with
                   // blocking pop
    Graph graph_;  // Graph for tracking access patterns
    bool running_; // Flag to control the tracking loop
    std::thread tracking_thread_; // Background thread for tracking loop
    MetisGraph metis_graph_;      // METIS graph for partitioning
    std::mutex graph_lock_;       // Mutex to lock the graph

    /**
     * @brief Tracking loop that processes items from the queue
     *
     * Continuously waits for items in the queue using blocking pop(), processes
     * them, and removes them. The loop ends when running_ is set to false and
     * the queue is empty.
     */
    void tracking_loop() {
        std::vector<std::string> keys;
        while (running_) {
            // Blocking pop: waits until an item is available
            queue_.pop(keys);

            // Check if we should stop after getting the item
            if (!running_) {
                break;
            }

            // Lock the graph
            std::lock_guard<std::mutex> lock(graph_lock_);

            // Process the item based on size
            if (keys.size() == 1) {
                // Single key: increment vertex weight
                graph_.increment_vertex_weight(keys[0]);
            } else if (keys.size() > 1) {
                // Multiple keys: increment vertex weights and create edges
                for (size_t i = 0; i < keys.size(); ++i) {
                    graph_.increment_vertex_weight(keys[i]);
                }

                // Add or increment edges between all pairs of keys
                for (size_t i = 0; i < keys.size(); ++i) {
                    for (size_t j = i + 1; j < keys.size(); ++j) {
                        graph_.increment_edge_weight(keys[i], keys[j]);
                    }
                }
            }
        }
    }

public:
    /**
     * @brief Constructor
     *
     * Initializes the tracker with a bounded queue of capacity 1000000 and
     * starts the background tracking thread.
     */
    Tracker() :
        running_(true),
        tracking_thread_(std::thread(&Tracker::tracking_loop, this)) {
        queue_.set_capacity(1000000);
    }

    /**
     * @brief Release method to stop the tracking loop
     *
     * Sets running to false and inserts a dummy vector into the queue to wake
     * up the blocking pop() in the tracking thread.
     */
    void release() {
        // Set the ending flag
        running_ = false;

        // Insert a dummy vector to wake up the blocking pop() in tracking_loop
        queue_.push(std::vector<std::string>()); // Empty vector
    }

    /**
     * @brief Destructor
     *
     * Stops the tracking loop by calling release(), then joins the thread.
     */
    ~Tracker() {
        release();

        // Join the thread if it's joinable
        if (tracking_thread_.joinable()) {
            tracking_thread_.join();
        }
    }

    /**
     * @brief Update method to insert a single key into the queue
     * @param key Reference to the string key
     *
     * Copies the string into a vector and inserts it into the queue.
     */
    void update(const std::string &key) {
        // Create a vector with a single key (copy the string)
        std::vector<std::string> keys;
        keys.push_back(key);

        // Insert into queue (thread-safe, blocking pop() will wake up
        // automatically)
        queue_.push(std::move(keys));
    }

    /**
     * @brief Multi-update method to insert multiple keys into the queue
     * @param keys Reference to the vector of keys
     *
     * Copies the vector of keys and inserts it into the queue.
     */
    void multi_update(const std::vector<std::string> &keys) {
        // Copy the vector of keys
        std::vector<std::string> keys_copy = keys;

        // Insert into queue (thread-safe, blocking pop() will wake up
        // automatically)
        queue_.push(std::move(keys_copy));
    }

    /**
     * @brief Multi-move-update method to insert multiple keys into the queue
     * @param keys Rvalue reference to the vector of keys
     *
     * Moves the vector of keys into the queue without copying.
     */
    void multi_move_update(std::vector<std::string> &&keys) {
        // Insert into queue by moving (no copy, thread-safe, blocking pop()
        // will wake up automatically)
        queue_.push(std::move(keys));
    }

    void clear_graph() {
        // Drain the queue (thread-safe, no mutex needed)
        std::vector<std::string> dummy;
        while (queue_.try_pop(dummy)) {
            // Keep popping until empty
        }
        graph_.clear();
    }

    size_t ready() const { return graph_.get_vertex_count() > 1; }

    /**
     * @brief Get a const reference to the graph
     * @return Const reference to the tracking graph
     */
    const Graph &graph() const { return graph_; }

    /**
     * @brief Get a non-const reference to the graph
     * @return Reference to the tracking graph
     */
    Graph &graph() { return graph_; }

    bool prepare_for_partition_map_update(size_t partition_count) {
        // Wait for the queue to be empty
        // This allows already submitted keys not to be considered for the
        // current repartioning
        // Note: concurrent_bounded_queue.size() is approximate, so we use
        // try_pop to check
        std::vector<std::string> dummy;
        while (queue_.try_pop(dummy)) {
            // Keep popping until empty
        }
        // Give a small delay to ensure all items are processed
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Lock the graph
        std::lock_guard<std::mutex> lock(graph_lock_);

        bool success = false;
        if (ready()) {
            try {
                metis_graph_.prepare_from_graph(graph_);
                metis_graph_.partition(partition_count);
                success = true;
            } catch (const std::exception &e) {
                // If METIS fails, keep the old partition map
                // This can happen if the graph is too small or has other issues
            }
        }
        return success;
    }

    void update_partition_map(PartitionMapType &partition_map) {
        std::vector<idx_t> metis_partitions =
            metis_graph_.get_partition_result();
        const auto &idx_to_vertex = metis_graph_.get_idx_to_vertex();
        for (size_t i = 0; i < metis_partitions.size(); ++i) {
            partition_map.put(idx_to_vertex[i],
                              static_cast<size_t>(metis_partitions[i]));
        }
        // Lock the graph to clear it
        std::lock_guard<std::mutex> lock(graph_lock_);
        graph_.clear();

        // Note that the queue was not cleared, thus, next repartitioning might
        // consider some realy old tracked keys
    }
};
