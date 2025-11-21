#pragma once

#include "../graph/Graph.h"
#include "../graph/MetisGraph.h"
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <semaphore>
#include <utility>

/**
 * @brief Tracker class for tracking key access patterns
 *
 * This class manages a queue of key-value pairs and processes them in a
 * background thread. It uses a semaphore to coordinate between the producer
 * (update method) and consumer (tracking_loop method).
 */
template <typename PartitionMapType> class Tracker {
private:
    std::queue<std::vector<std::string>>
        queue_;   // Queue of pairs (keys vector and size)
    Graph graph_; // Graph for tracking access patterns
    std::counting_semaphore<10000>
        semaphore_;               // Semaphore for queue coordination
    bool running_;                // Flag to control the tracking loop
    std::thread tracking_thread_; // Background thread for tracking loop
    std::mutex queue_mutex_;      // Mutex for thread-safe queue access
    MetisGraph metis_graph_;      // METIS graph for partitioning
    std::mutex graph_lock_;       // Mutex to lock the graph

    /**
     * @brief Tracking loop that processes items from the queue
     *
     * Continuously waits for items in the queue, processes them, and removes
     * them. The loop ends when running_ is set to false and the queue is empty.
     */
    void tracking_loop() {
        while (running_) {
            semaphore_.acquire();
            std::vector<std::string> keys;
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (queue_.empty()) {
                    continue;
                }
                keys = std::move(queue_.front());
                queue_.pop();
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
     * Initializes the tracker and starts the background tracking thread.
     */
    Tracker() :
        semaphore_(0), running_(true),
        tracking_thread_(std::thread(&Tracker::tracking_loop, this)) {}

    /**
     * @brief Release method to stop the tracking loop
     *
     * Sets running to false, inserts a dummy vector with length 0 into the
     * queue, and releases the semaphore to wake up the tracking thread.
     */
    void release() {
        // Set the ending flag
        running_ = false;

        // Insert a dummy vector with length 0 (empty vector) as sentinel
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            queue_.emplace(); // Empty vector
        }

        // Release semaphore to wake up the tracking thread
        semaphore_.release();
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

        // Insert into queue
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            queue_.emplace(std::move(keys));
        }

        // Release semaphore to signal that an item is available
        semaphore_.release();
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

        // Insert into queue
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            queue_.emplace(std::move(keys_copy));
        }

        // Release semaphore to signal that an item is available
        semaphore_.release();
    }

    /**
     * @brief Multi-move-update method to insert multiple keys into the queue
     * @param keys Rvalue reference to the vector of keys
     *
     * Moves the vector of keys into the queue without copying.
     */
    void multi_move_update(std::vector<std::string> &&keys) {
        // Insert into queue by moving (no copy)
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            queue_.emplace(std::move(keys));
        }

        // Release semaphore to signal that an item is available
        semaphore_.release();
    }

    void clear_graph() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!queue_.empty()) {
            queue_.pop();
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
        while (queue_.size() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

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
