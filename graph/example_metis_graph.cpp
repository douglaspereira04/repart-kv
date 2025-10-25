#include "Graph.h"
#include "MetisGraph.h"
#include <iostream>
#include <map>

int main() {
    std::cout << "=== METIS Graph Partitioning Example ===" << std::endl
              << std::endl;

    // Create a sample graph
    Graph graph;

    std::cout << "Building sample graph..." << std::endl;

    // Create vertices with weights
    graph.increment_vertex_weight("A");
    graph.increment_vertex_weight("A"); // Weight = 2
    graph.increment_vertex_weight("B");
    graph.increment_vertex_weight("C");
    graph.increment_vertex_weight("D");
    graph.increment_vertex_weight("E");
    graph.increment_vertex_weight("E"); // Weight = 2
    graph.increment_vertex_weight("F");

    // Create edges with weights
    graph.increment_edge_weight("A", "B");
    graph.increment_edge_weight("A", "B"); // Weight = 2
    graph.increment_edge_weight("A", "C");
    graph.increment_edge_weight("B", "C");
    graph.increment_edge_weight("B", "D");
    graph.increment_edge_weight("C", "D");
    graph.increment_edge_weight("C", "E");
    graph.increment_edge_weight("D", "E");
    graph.increment_edge_weight("D", "F");
    graph.increment_edge_weight("E", "F");
    graph.increment_edge_weight("E", "F"); // Weight = 2

    std::cout << "  Vertices: " << graph.get_vertex_count() << std::endl;
    std::cout << "  Edges: " << graph.get_edge_count() << std::endl;
    std::cout << std::endl;

    // Prepare METIS graph
    std::cout << "Preparing METIS graph..." << std::endl;
    MetisGraph metis_graph;

    try {
        metis_graph.prepare_from_graph(graph);
        std::cout << "  Successfully prepared graph for METIS" << std::endl;
        std::cout << "  Number of vertices: " << metis_graph.get_num_vertices()
                  << std::endl;
        std::cout << "  Number of edges: " << metis_graph.get_num_edges()
                  << std::endl;
        std::cout << std::endl;

        // Display CSR format details
        std::cout << "CSR Format Details:" << std::endl;
        const auto &vwgt = metis_graph.get_vertex_weights();
        const auto &idx_to_vertex = metis_graph.get_idx_to_vertex();

        std::cout << "  Vertex -> Index mapping:" << std::endl;
        for (size_t i = 0; i < idx_to_vertex.size(); ++i) {
            std::cout << "    " << idx_to_vertex[i] << " -> " << i
                      << " (weight: " << vwgt[i] << ")" << std::endl;
        }
        std::cout << std::endl;

        // Partition into 2 parts
        std::cout << "Partitioning into 2 parts..." << std::endl;
        metis_graph.partition(2);
        auto partitions_2 = metis_graph.get_partition_result();

        // Group vertices by partition
        std::map<int, std::vector<std::string>> parts_2;
        for (size_t i = 0; i < partitions_2.size(); ++i) {
            parts_2[partitions_2[i]].push_back(idx_to_vertex[i]);
        }

        for (const auto &[part_id, vertices] : parts_2) {
            std::cout << "  Partition " << part_id << ": ";
            for (size_t i = 0; i < vertices.size(); ++i) {
                std::cout << vertices[i];
                if (i < vertices.size() - 1)
                    std::cout << ", ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;

        // Partition into 3 parts
        std::cout << "Partitioning into 3 parts..." << std::endl;
        metis_graph.partition(3);
        auto partitions_3 = metis_graph.get_partition_result();

        // Group vertices by partition
        std::map<int, std::vector<std::string>> parts_3;
        for (size_t i = 0; i < partitions_3.size(); ++i) {
            parts_3[partitions_3[i]].push_back(idx_to_vertex[i]);
        }

        for (const auto &[part_id, vertices] : parts_3) {
            std::cout << "  Partition " << part_id << ": ";
            for (size_t i = 0; i < vertices.size(); ++i) {
                std::cout << vertices[i];
                if (i < vertices.size() - 1)
                    std::cout << ", ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;

        // Calculate partition statistics
        std::cout << "Partition Statistics (3 parts):" << std::endl;
        for (const auto &[part_id, vertices] : parts_3) {
            int total_weight = 0;
            for (const auto &vertex : vertices) {
                total_weight += graph.get_vertex_weight(vertex);
            }
            std::cout << "  Partition " << part_id << ": " << vertices.size()
                      << " vertices, " << "total weight: " << total_weight
                      << std::endl;
        }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << std::endl << "✓ Example completed successfully!" << std::endl;
    return 0;
}
