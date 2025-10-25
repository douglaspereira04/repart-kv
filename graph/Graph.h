#ifndef GRAPH_H
#define GRAPH_H

#include <ankerl/unordered_dense.h>
#include <string>

/**
 * @brief A weighted directed graph implementation using adjacency list
 * representation.
 *
 * This class provides efficient operations for managing vertices and edges with
 * integer weights. Both vertices and edges are identified by strings and have
 * associated integer weights.
 */
class Graph {
private:
    // Map of vertex name to vertex weight
    ankerl::unordered_dense::map<std::string, int> vertices_;

    // Map of source vertex to map of destination vertex to edge weight
    // edges[source][destination] = weight
    ankerl::unordered_dense::map<std::string,
                                 ankerl::unordered_dense::map<std::string, int>>
        edges_;

public:
    /**
     * @brief Default constructor
     */
    Graph() = default;

    /**
     * @brief Increments the weight of a vertex by 1.
     * If the vertex does not exist, it is created with weight 1.
     *
     * @param vertex The name of the vertex
     * @return The new weight of the vertex after incrementing
     */
    int increment_vertex_weight(const std::string &vertex) {
        // Using operator[] which creates entry with default value (0) if not
        // exists Then increment and return
        return ++vertices_[vertex];
    }

    /**
     * @brief Increments the weight of an edge by 1.
     * If the edge does not exist, it is created with weight 1.
     *
     * @param source The source vertex of the edge
     * @param destination The destination vertex of the edge
     * @return The new weight of the edge after incrementing
     */
    int increment_edge_weight(const std::string &source,
                              const std::string &destination) {
        // Using operator[] on nested maps - creates entries with default value
        // (0) if not exists Then increment and return
        return ++edges_[source][destination];
    }

    /**
     * @brief Gets the weight of a vertex.
     *
     * @param vertex The name of the vertex
     * @return The weight of the vertex, or 0 if the vertex does not exist
     */
    int get_vertex_weight(const std::string &vertex) const {
        auto it = vertices_.find(vertex);
        return (it != vertices_.end()) ? it->second : 0;
    }

    /**
     * @brief Gets the weight of an edge.
     *
     * @param source The source vertex of the edge
     * @param destination The destination vertex of the edge
     * @return The weight of the edge, or 0 if the edge does not exist
     */
    int get_edge_weight(const std::string &source,
                        const std::string &destination) const {
        auto src_it = edges_.find(source);
        if (src_it != edges_.end()) {
            auto dest_it = src_it->second.find(destination);
            if (dest_it != src_it->second.end()) {
                return dest_it->second;
            }
        }
        return 0;
    }

    /**
     * @brief Checks if a vertex exists in the graph.
     *
     * @param vertex The name of the vertex
     * @return true if the vertex exists, false otherwise
     */
    bool has_vertex(const std::string &vertex) const {
        return vertices_.find(vertex) != vertices_.end();
    }

    /**
     * @brief Checks if an edge exists in the graph.
     *
     * @param source The source vertex of the edge
     * @param destination The destination vertex of the edge
     * @return true if the edge exists, false otherwise
     */
    bool has_edge(const std::string &source,
                  const std::string &destination) const {
        auto src_it = edges_.find(source);
        if (src_it != edges_.end()) {
            return src_it->second.find(destination) != src_it->second.end();
        }
        return false;
    }

    /**
     * @brief Gets the number of vertices in the graph.
     *
     * @return The number of vertices
     */
    size_t get_vertex_count() const { return vertices_.size(); }

    /**
     * @brief Gets the number of edges in the graph.
     *
     * @return The number of edges
     */
    size_t get_edge_count() const {
        size_t count = 0;
        for (const auto &[source, destinations] : edges_) {
            count += destinations.size();
        }
        return count;
    }

    /**
     * @brief Gets a const reference to the vertices map.
     *
     * @return Const reference to the vertices map
     */
    const ankerl::unordered_dense::map<std::string, int> &get_vertices() const {
        return vertices_;
    }

    /**
     * @brief Gets a const reference to the edges map.
     *
     * @return Const reference to the edges map
     */
    const ankerl::unordered_dense::map<
        std::string, ankerl::unordered_dense::map<std::string, int>> &
    get_edges() const {
        return edges_;
    }

    /**
     * @brief Clears all vertices and edges from the graph.
     */
    void clear() {
        vertices_.clear();
        edges_.clear();
    }
};

#endif // GRAPH_H
