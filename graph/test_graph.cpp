#include "Graph.h"
#include "../utils/test_assertions.h"
#include <iostream>
#include <chrono>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void testBasicVertexOperations() {
    TEST("basic_vertex_operations")
        Graph graph;

        // Test creating a new vertex
        ASSERT_EQ(1, graph.increment_vertex_weight("A"));
        ASSERT_TRUE(graph.has_vertex("A"));
        ASSERT_EQ(1, graph.get_vertex_weight("A"));

        // Test incrementing existing vertex
        ASSERT_EQ(2, graph.increment_vertex_weight("A"));
        ASSERT_EQ(2, graph.get_vertex_weight("A"));

        // Test multiple vertices
        ASSERT_EQ(1, graph.increment_vertex_weight("B"));
        ASSERT_EQ(1, graph.increment_vertex_weight("C"));
        ASSERT_EQ(3, graph.get_vertex_count());

        // Test non-existent vertex
        ASSERT_FALSE(graph.has_vertex("D"));
        ASSERT_EQ(0, graph.get_vertex_weight("D"));

        std::cout << "  ✓ Basic vertex operations passed" << std::endl;
    END_TEST("basic_vertex_operations")
}

void testBasicEdgeOperations() {
    TEST("basic_edge_operations")
        Graph graph;

        // Test creating a new edge
        ASSERT_EQ(1, graph.increment_edge_weight("A", "B"));
        ASSERT_TRUE(graph.has_edge("A", "B"));
        ASSERT_EQ(1, graph.get_edge_weight("A", "B"));

        // Test incrementing existing edge
        ASSERT_EQ(2, graph.increment_edge_weight("A", "B"));
        ASSERT_EQ(2, graph.get_edge_weight("A", "B"));

        // Test multiple edges
        ASSERT_EQ(1, graph.increment_edge_weight("A", "C"));
        ASSERT_EQ(1, graph.increment_edge_weight("B", "C"));
        ASSERT_EQ(3, graph.get_edge_count());

        // Test directed edges (A->B exists but B->A doesn't)
        ASSERT_TRUE(graph.has_edge("A", "B"));
        ASSERT_FALSE(graph.has_edge("B", "A"));

        // Test non-existent edge
        ASSERT_FALSE(graph.has_edge("X", "Y"));
        ASSERT_EQ(0, graph.get_edge_weight("X", "Y"));

        std::cout << "  ✓ Basic edge operations passed" << std::endl;
    END_TEST("basic_edge_operations")
}

void testCombinedOperations() {
    TEST("combined_operations")
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

        ASSERT_EQ(6, graph.get_vertex_weight("A"));
        ASSERT_EQ(6, graph.get_edge_weight("A", "B"));

        std::cout << "  ✓ Combined operations passed" << std::endl;
    END_TEST("combined_operations")
}

void testClearOperation() {
    TEST("clear_operation")
        Graph graph;

        // Add some data
        graph.increment_vertex_weight("A");
        graph.increment_vertex_weight("B");
        graph.increment_edge_weight("A", "B");

        ASSERT_EQ(2, graph.get_vertex_count());
        ASSERT_EQ(1, graph.get_edge_count());

        // Clear the graph
        graph.clear();

        ASSERT_EQ(0, graph.get_vertex_count());
        ASSERT_EQ(0, graph.get_edge_count());
        ASSERT_FALSE(graph.has_vertex("A"));
        ASSERT_FALSE(graph.has_edge("A", "B"));

        std::cout << "  ✓ Clear operation passed" << std::endl;
    END_TEST("clear_operation")
}

void testPerformance() {
    TEST("performance")
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
    END_TEST("performance")
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Testing Graph Implementation" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    testBasicVertexOperations();
    testBasicEdgeOperations();
    testCombinedOperations();
    testClearOperation();
    testPerformance();

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Overall Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << tests_failed << std::endl;
    std::cout << "Total tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << std::endl;

    if (tests_failed == 0) {
        std::cout << "✓ All Graph tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some Graph tests failed!" << std::endl;
        return 1;
    }
}

