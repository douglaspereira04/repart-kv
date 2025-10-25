#include "Graph.h"
#include <iostream>
#include <iomanip>

int main() {
    Graph graph;

    std::cout << "=== Graph Example Usage ===" << std::endl << std::endl;

    // Increment vertex weights
    std::cout << "Incrementing vertex weights:" << std::endl;
    std::cout << "  Vertex 'A' weight: " << graph.increment_vertex_weight("A")
              << std::endl;
    std::cout << "  Vertex 'A' weight: " << graph.increment_vertex_weight("A")
              << std::endl;
    std::cout << "  Vertex 'B' weight: " << graph.increment_vertex_weight("B")
              << std::endl;
    std::cout << "  Vertex 'C' weight: " << graph.increment_vertex_weight("C")
              << std::endl;
    std::cout << "  Vertex 'A' weight: " << graph.increment_vertex_weight("A")
              << std::endl;
    std::cout << std::endl;

    // Increment edge weights
    std::cout << "Incrementing edge weights:" << std::endl;
    std::cout << "  Edge A->B weight: " << graph.increment_edge_weight("A", "B")
              << std::endl;
    std::cout << "  Edge A->B weight: " << graph.increment_edge_weight("A", "B")
              << std::endl;
    std::cout << "  Edge A->C weight: " << graph.increment_edge_weight("A", "C")
              << std::endl;
    std::cout << "  Edge B->C weight: " << graph.increment_edge_weight("B", "C")
              << std::endl;
    std::cout << "  Edge A->B weight: " << graph.increment_edge_weight("A", "B")
              << std::endl;
    std::cout << "  Edge C->A weight: " << graph.increment_edge_weight("C", "A")
              << std::endl;
    std::cout << std::endl;

    // Display graph statistics
    std::cout << "Graph Statistics:" << std::endl;
    std::cout << "  Total vertices: " << graph.get_vertex_count() << std::endl;
    std::cout << "  Total edges: " << graph.get_edge_count() << std::endl;
    std::cout << std::endl;

    // Display all vertices and their weights
    std::cout << "Vertices and Weights:" << std::endl;
    for (const auto &[vertex, weight] : graph.get_vertices()) {
        std::cout << "  " << std::setw(8) << vertex << ": " << weight
                  << std::endl;
    }
    std::cout << std::endl;

    // Display all edges and their weights
    std::cout << "Edges and Weights:" << std::endl;
    for (const auto &[source, destinations] : graph.get_edges()) {
        for (const auto &[destination, weight] : destinations) {
            std::cout << "  " << std::setw(8) << (source + " -> " + destination)
                      << ": " << weight << std::endl;
        }
    }
    std::cout << std::endl;

    // Test query methods
    std::cout << "Query Methods:" << std::endl;
    std::cout << "  Has vertex 'A': " << (graph.has_vertex("A") ? "Yes" : "No")
              << std::endl;
    std::cout << "  Has vertex 'D': " << (graph.has_vertex("D") ? "Yes" : "No")
              << std::endl;
    std::cout << "  Has edge A->B: "
              << (graph.has_edge("A", "B") ? "Yes" : "No") << std::endl;
    std::cout << "  Has edge B->A: "
              << (graph.has_edge("B", "A") ? "Yes" : "No") << std::endl;
    std::cout << "  Weight of vertex 'A': " << graph.get_vertex_weight("A")
              << std::endl;
    std::cout << "  Weight of vertex 'D': " << graph.get_vertex_weight("D")
              << std::endl;
    std::cout << "  Weight of edge A->B: " << graph.get_edge_weight("A", "B")
              << std::endl;
    std::cout << "  Weight of edge B->A: " << graph.get_edge_weight("B", "A")
              << std::endl;

    return 0;
}
