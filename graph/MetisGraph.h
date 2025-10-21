#ifndef METIS_GRAPH_H
#define METIS_GRAPH_H

#include "Graph.h"
#include <metis.h>
#include <vector>
#include <ankerl/unordered_dense.h>
#include <string>
#include <stdexcept>
#include <algorithm>

/**
 * @brief Wrapper class for partitioning graphs using METIS library.
 * 
 * This class converts a Graph instance into METIS-compatible format (CSR - Compressed Sparse Row)
 * and provides methods to partition the graph using METIS algorithms.
 */
class MetisGraph {
private:
    // Number of vertices
    idx_t nvtxs_;
    
    // Number of constraints (typically 1)
    idx_t ncon_;
    
    // Adjacency structure in CSR format
    // xadj[i] stores the starting index in adjncy for vertex i's neighbors
    std::vector<idx_t> xadj_;
    
    // Adjacency list - stores all edges
    std::vector<idx_t> adjncy_;
    
    // Vertex weights (optional)
    std::vector<idx_t> vwgt_;
    
    // Edge weights (optional)
    std::vector<idx_t> adjwgt_;
    
    // Mapping from vertex string to integer index
    ankerl::unordered_dense::map<std::string, idx_t> vertex_to_idx_;
    
    // Mapping from integer index to vertex string
    std::vector<std::string> idx_to_vertex_;
    
    // Flag indicating if graph has been prepared
    bool prepared_;

    // Partition result array
    std::vector<idx_t> part_;

public:
    /**
     * @brief Default constructor
     */
    MetisGraph() : nvtxs_(0), ncon_(1), prepared_(false) {}

    /**
     * @brief Prepares the METIS graph data structures from a Graph instance.
     * 
     * This method extracts all necessary parameters from the Graph and converts
     * them into METIS-compatible CSR format. It stores vertex mappings, adjacency
     * structure, and weights.
     * 
     * @param graph The Graph instance to convert
     * @throws std::runtime_error if the graph is empty
     */
    void prepare_from_graph(const Graph& graph) {
        const auto& vertices = graph.get_vertices();
        const auto& edges = graph.get_edges();
        
        if (vertices.empty()) {
            throw std::runtime_error("Cannot prepare METIS graph from empty graph");
        }
        
        // Reset state
        vertex_to_idx_.clear();
        idx_to_vertex_.clear();
        xadj_.clear();
        adjncy_.clear();
        vwgt_.clear();
        adjwgt_.clear();
        
        // Build vertex mappings
        nvtxs_ = 0;
        for (const auto& [vertex_name, weight] : vertices) {
            vertex_to_idx_[vertex_name] = nvtxs_;
            idx_to_vertex_.push_back(vertex_name);
            vwgt_.push_back(static_cast<idx_t>(weight));
            nvtxs_++;
        }
        
        // Build CSR adjacency structure
        xadj_.reserve(nvtxs_ + 1);
        xadj_.push_back(0);
        
        idx_t edge_count = 0;
        
        for (idx_t i = 0; i < nvtxs_; ++i) {
            const std::string& vertex_name = idx_to_vertex_[i];
            
            // Check if this vertex has outgoing edges
            auto edge_it = edges.find(vertex_name);
            if (edge_it != edges.end()) {
                // Store neighbors and weights in sorted order for consistency
                std::vector<std::pair<idx_t, idx_t>> neighbors;
                
                for (const auto& [neighbor_name, edge_weight] : edge_it->second) {
                    auto neighbor_idx_it = vertex_to_idx_.find(neighbor_name);
                    if (neighbor_idx_it != vertex_to_idx_.end()) {
                        idx_t neighbor_idx = neighbor_idx_it->second;
                        neighbors.push_back({neighbor_idx, static_cast<idx_t>(edge_weight)});
                    }
                }
                
                // Sort neighbors by index
                std::sort(neighbors.begin(), neighbors.end());
                
                // Add to adjacency list
                for (const auto& [neighbor_idx, edge_weight] : neighbors) {
                    adjncy_.push_back(neighbor_idx);
                    adjwgt_.push_back(edge_weight);
                    edge_count++;
                }
            }
            
            xadj_.push_back(edge_count);
        }
        
        ncon_ = 1;
        prepared_ = true;
    }

    /**
     * @brief Gets the partition result array.
     * 
     * @return Const reference to the partition result array
     */
    const std::vector<idx_t>& get_partition_result() const {
        return part_;
    }
    
    /**
     * @brief Partitions the graph using METIS.
     * 
     * @param num_partitions Number of partitions to create
     * @throws std::runtime_error if graph not prepared or METIS fails
     */
    void partition(int num_partitions) {
        if (!prepared_) {
            throw std::runtime_error("Graph must be prepared before partitioning");
        }
        
        if (num_partitions <= 0) {
            throw std::runtime_error("Number of partitions must be positive");
        }
        
        if (num_partitions > nvtxs_) {
            throw std::runtime_error("Number of partitions cannot exceed number of vertices");
        }
        
        // METIS parameters
        idx_t nparts = static_cast<idx_t>(num_partitions);
        idx_t objval;  // Edge-cut or communication volume
        
        // Partition result array
        part_.resize(nvtxs_);
        
        // METIS options (use defaults)
        idx_t options[METIS_NOPTIONS];
        METIS_SetDefaultOptions(options);
        
        // Call METIS partitioning function
        int ret;
        if (num_partitions <= 8) {
            // Use recursive bisection for small number of partitions
            ret = METIS_PartGraphRecursive(
                &nvtxs_,              // Number of vertices
                &ncon_,               // Number of constraints
                xadj_.data(),         // Adjacency structure
                adjncy_.data(),       // Adjacency list
                vwgt_.data(),         // Vertex weights
                nullptr,              // Vertex sizes (for communication volume)
                adjwgt_.data(),       // Edge weights
                &nparts,              // Number of partitions
                nullptr,              // Target partition weights (use equal)
                nullptr,              // Imbalance tolerance (use default)
                options,              // Options array
                &objval,              // Output: edge-cut
                part_.data()           // Output: partition assignment
            );
        } else {
            // Use k-way partitioning for larger number of partitions
            ret = METIS_PartGraphKway(
                &nvtxs_,              // Number of vertices
                &ncon_,               // Number of constraints
                xadj_.data(),         // Adjacency structure
                adjncy_.data(),       // Adjacency list
                vwgt_.data(),         // Vertex weights
                nullptr,              // Vertex sizes
                adjwgt_.data(),       // Edge weights
                &nparts,              // Number of partitions
                nullptr,              // Target partition weights
                nullptr,              // Imbalance tolerance
                options,              // Options array
                &objval,              // Output: edge-cut
                part_.data()           // Output: partition assignment
            );
        }
        
        if (ret != METIS_OK) {
            throw std::runtime_error("METIS partitioning failed with error code: " + std::to_string(ret));
        }
    }
    
    /**
     * @brief Gets the number of vertices in the prepared graph.
     * 
     * @return Number of vertices
     */
    idx_t get_num_vertices() const {
        return nvtxs_;
    }
    
    /**
     * @brief Gets the number of edges in the prepared graph.
     * 
     * @return Number of edges
     */
    size_t get_num_edges() const {
        return adjncy_.size();
    }
    
    /**
     * @brief Checks if the graph has been prepared.
     * 
     * @return true if prepared, false otherwise
     */
    bool is_prepared() const {
        return prepared_;
    }
    
    /**
     * @brief Gets the mapping from vertex string to integer index.
     * 
     * @return Const reference to the vertex-to-index map
     */
    const ankerl::unordered_dense::map<std::string, idx_t>& get_vertex_to_idx() const {
        return vertex_to_idx_;
    }
    
    /**
     * @brief Gets the mapping from integer index to vertex string.
     * 
     * @return Const reference to the index-to-vertex vector
     */
    const std::vector<std::string>& get_idx_to_vertex() const {
        return idx_to_vertex_;
    }
    
    /**
     * @brief Gets the CSR adjacency structure (xadj array).
     * 
     * @return Const reference to xadj vector
     */
    const std::vector<idx_t>& get_xadj() const {
        return xadj_;
    }
    
    /**
     * @brief Gets the CSR adjacency list (adjncy array).
     * 
     * @return Const reference to adjncy vector
     */
    const std::vector<idx_t>& get_adjncy() const {
        return adjncy_;
    }
    
    /**
     * @brief Gets the vertex weights array.
     * 
     * @return Const reference to vertex weights vector
     */
    const std::vector<idx_t>& get_vertex_weights() const {
        return vwgt_;
    }
    
    /**
     * @brief Gets the edge weights array.
     * 
     * @return Const reference to edge weights vector
     */
    const std::vector<idx_t>& get_edge_weights() const {
        return adjwgt_;
    }
};

#endif // METIS_GRAPH_H

