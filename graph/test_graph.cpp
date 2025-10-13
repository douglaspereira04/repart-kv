#include "Graph.h"
#include <cassert>
#include <iostream>
#include <chrono>

void testBasicVertexOperations() {
    std::cout << "Testing basic vertex operations..." << std::endl;
    Graph graph;

    // Test creating a new vertex
    assert(graph.increment_vertex_weight("A") == 1);
    assert(graph.has_vertex("A"));
    assert(graph.get_vertex_weight("A") == 1);

    // Test incrementing existing vertex
    assert(graph.increment_vertex_weight("A") == 2);
    assert(graph.get_vertex_weight("A") == 2);

    // Test multiple vertices
    assert(graph.increment_vertex_weight("B") == 1);
    assert(graph.increment_vertex_weight("C") == 1);
    assert(graph.get_vertex_count() == 3);

    // Test non-existent vertex
    assert(!graph.has_vertex("D"));
    assert(graph.get_vertex_weight("D") == 0);

    std::cout << "  ✓ Basic vertex operations passed" << std::endl;
}

void testBasicEdgeOperations() {
    std::cout << "Testing basic edge operations..." << std::endl;
    Graph graph;

    // Test creating a new edge
    assert(graph.increment_edge_weight("A", "B") == 1);
    assert(graph.has_edge("A", "B"));
    assert(graph.get_edge_weight("A", "B") == 1);

    // Test incrementing existing edge
    assert(graph.increment_edge_weight("A", "B") == 2);
    assert(graph.get_edge_weight("A", "B") == 2);

    // Test multiple edges
    assert(graph.increment_edge_weight("A", "C") == 1);
    assert(graph.increment_edge_weight("B", "C") == 1);
    assert(graph.get_edge_count() == 3);

    // Test directed edges (A->B exists but B->A doesn't)
    assert(graph.has_edge("A", "B"));
    assert(!graph.has_edge("B", "A"));

    // Test non-existent edge
    assert(!graph.has_edge("X", "Y"));
    assert(graph.get_edge_weight("X", "Y") == 0);

    std::cout << "  ✓ Basic edge operations passed" << std::endl;
}

void testCombinedOperations() {
    std::cout << "Testing combined operations..." << std::endl;
    Graph graph;

    // Create vertices and edges
    graph.increment_vertex_weight("A");
    graph.increment_vertex_weight("B");
    graph.increment_edge_weight("A", "B");

    // Increment multiple times
    for (int i = 0; i < 5; ++i) {
        graph.increment_vertex_weight("A");
        graph.increment_edge_weight("A", "B");
    }

    assert(graph.get_vertex_weight("A") == 6);
    assert(graph.get_edge_weight("A", "B") == 6);

    std::cout << "  ✓ Combined operations passed" << std::endl;
}

void testClearOperation() {
    std::cout << "Testing clear operation..." << std::endl;
    Graph graph;

    // Add some data
    graph.increment_vertex_weight("A");
    graph.increment_vertex_weight("B");
    graph.increment_edge_weight("A", "B");

    assert(graph.get_vertex_count() == 2);
    assert(graph.get_edge_count() == 1);

    // Clear the graph
    graph.clear();

    assert(graph.get_vertex_count() == 0);
    assert(graph.get_edge_count() == 0);
    assert(!graph.has_vertex("A"));
    assert(!graph.has_edge("A", "B"));

    std::cout << "  ✓ Clear operation passed" << std::endl;
}

void testPerformance() {
    std::cout << "Testing performance..." << std::endl;
    Graph graph;

    const int numOperations = 100000;

    // Test vertex insertion performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; ++i) {
        graph.increment_vertex_weight("vertex_" + std::to_string(i % 1000));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  " << numOperations << " vertex operations in " 
              << duration.count() << "ms" << std::endl;

    // Test edge insertion performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; ++i) {
        int src = i % 1000;
        int dest = (i + 1) % 1000;
        graph.increment_edge_weight("vertex_" + std::to_string(src), 
                                 "vertex_" + std::to_string(dest));
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  " << numOperations << " edge operations in " 
              << duration.count() << "ms" << std::endl;

    std::cout << "  ✓ Performance test completed" << std::endl;
}

int main() {
    std::cout << "=== Running Graph Tests ===" << std::endl << std::endl;

    try {
        testBasicVertexOperations();
        testBasicEdgeOperations();
        testCombinedOperations();
        testClearOperation();
        testPerformance();

        std::cout << std::endl << "✓ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}

