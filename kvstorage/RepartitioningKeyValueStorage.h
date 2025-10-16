#pragma once

#include "PartitionedKeyValueStorage.h"
#include "../graph/Graph.h"
#include "../graph/MetisGraph.h"
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <atomic>

/**
 * @brief CRTP abstract base class for repartitioning key-value storage implementations
 * 
 * This abstract class defines the common interface for repartitioning key-value storage
 * implementations using CRTP (Curiously Recurring Template Pattern) to provide
 * zero-overhead abstractions without virtual functions.
 * 
 * @tparam Derived The concrete implementation type (CRTP pattern)
 * @tparam StorageEngineType The storage engine type (must derive from StorageEngine)
 * 
 * Common functionality includes:
 * - Access pattern tracking via graph structures
 * - METIS-based graph partitioning
 * - Automatic repartitioning with configurable intervals
 * - Thread-safe operations with fine-grained locking
 * - Tracking enable/disable functionality
 * 
 * Derived classes must implement:
 * - std::string read_impl(const std::string& key)
 * - void write_impl(const std::string& key, const std::string& value)
 * - std::vector<std::pair<std::string, std::string>> scan_impl(const std::string& initial_key_prefix, size_t limit)
 * - void repartition_impl()
 * - void single_key_graph_update_impl(const std::string& key)
 * - void multi_key_graph_update_impl(const std::vector<std::string>& keys)
 * - void repartition_loop_impl()
 */
template<typename Derived, typename StorageEngineType>
class RepartitioningKeyValueStorage 
    : public PartitionedKeyValueStorage<Derived, StorageEngineType> {
public:
    /**
     * @brief Default constructor
     */
    RepartitioningKeyValueStorage() = default;

    // These methods are implemented by derived classes as part of the CRTP pattern
    // Derived classes must implement:
    // - std::string read_impl(const std::string& key)
    // - void write_impl(const std::string& key, const std::string& value)  
    // - std::vector<std::pair<std::string, std::string>> scan_impl(const std::string& initial_key_prefix, size_t limit)

    /**
     * @brief Repartition data across available partitions
     * 
     * This method uses METIS graph partitioning to redistribute keys across
     * partitions based on their access patterns. Keys that are frequently
     * accessed together will be placed in the same partition to optimize
     * scan performance and minimize cross-partition operations.
     */
    void repartition() {
        static_cast<Derived*>(this)->repartition_impl();
    }

    /**
     * @brief Enable or disable tracking of key access patterns
     * @param enable If true, enables tracking; if false, disables it
     */
    void enable_tracking(bool enable) {
        static_cast<Derived*>(this)->enable_tracking_impl(enable);
    }

    /**
     * @brief Check if tracking is currently enabled
     * @return true if tracking is enabled, false otherwise
     */
    bool enable_tracking() const {
        return static_cast<const Derived*>(this)->enable_tracking_impl();
    }

    /**
     * @brief Check if repartitioning is currently in progress
     * @return true if repartitioning is in progress, false otherwise
     */
    bool is_repartitioning() const {
        return static_cast<const Derived*>(this)->is_repartitioning_impl();
    }

    /**
     * @brief Get a const reference to the access pattern graph
     * @return Const reference to the graph tracking key access patterns
     */
    const Graph& graph() const {
        return static_cast<const Derived*>(this)->graph_impl();
    }

    /**
     * @brief Clear all tracking data from the graph
     */
    void clear_graph() {
        static_cast<Derived*>(this)->clear_graph_impl();
    }

protected:
    /**
     * @brief Protected destructor (CRTP pattern)
     */
    ~RepartitioningKeyValueStorage() = default;
};

