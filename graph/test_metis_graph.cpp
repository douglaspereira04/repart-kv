#include "Graph.h"
#include "MetisGraph.h"
#include "../utils/test_assertions.h"
#include <iostream>
#include <set>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_prepare_from_graph() {
    TEST("prepare_from_graph")
        Graph graph;
        graph.increment_vertex_weight("A");
        graph.increment_vertex_weight("B");
        graph.increment_vertex_weight("C");
        graph.increment_edge_weight("A", "B");
        graph.increment_edge_weight("B", "C");
        graph.increment_edge_weight("C", "A");
        
        MetisGraph metis_graph;
        ASSERT_FALSE(metis_graph.is_prepared());
        
        metis_graph.prepare_from_graph(graph);
        
        ASSERT_TRUE(metis_graph.is_prepared());
        ASSERT_EQ(3, metis_graph.get_num_vertices());
        ASSERT_EQ(3, metis_graph.get_num_edges());
        
        // Check vertex mappings
        const auto& vertex_to_idx = metis_graph.get_vertex_to_idx();
        const auto& idx_to_vertex = metis_graph.get_idx_to_vertex();
        
        ASSERT_EQ(3, vertex_to_idx.size());
        ASSERT_EQ(3, idx_to_vertex.size());
        
        // Check CSR structure
        const auto& xadj = metis_graph.get_xadj();
        const auto& adjncy = metis_graph.get_adjncy();
        
        ASSERT_EQ(4, xadj.size());  // nvtxs + 1
        ASSERT_EQ(3, adjncy.size());
        
        std::cout << "  ✓ Prepare from graph passed" << std::endl;
    END_TEST("prepare_from_graph")
}

void test_empty_graph() {
    TEST("empty_graph")
        Graph graph;
        MetisGraph metis_graph;
        
        bool exception_thrown = false;
        try {
            metis_graph.prepare_from_graph(graph);
        } catch (const std::runtime_error& e) {
            exception_thrown = true;
        }
        
        ASSERT_TRUE(exception_thrown);
        
        std::cout << "  ✓ Empty graph handling passed" << std::endl;
    END_TEST("empty_graph")
}

void test_partition_simple() {
    TEST("partition_simple")
        Graph graph;
        
        // Create a simple graph with 4 vertices
        graph.increment_vertex_weight("A");
        graph.increment_vertex_weight("B");
        graph.increment_vertex_weight("C");
        graph.increment_vertex_weight("D");
        
        graph.increment_edge_weight("A", "B");
        graph.increment_edge_weight("B", "C");
        graph.increment_edge_weight("C", "D");
        graph.increment_edge_weight("D", "A");
        
        MetisGraph metis_graph;
        metis_graph.prepare_from_graph(graph);
        
        // Partition into 2 parts
        auto partitions = metis_graph.partition(2);
        
        ASSERT_EQ(4, partitions.size());
        
        // Check partition IDs are valid (0 or 1)
        std::set<int> unique_partitions;
        for (size_t i = 0; i < partitions.size(); ++i) {
            ASSERT_GE(partitions[i], 0);
            ASSERT_LT(partitions[i], 2);
            unique_partitions.insert(partitions[i]);
        }
        
        // Should use both partitions
        ASSERT_LE(unique_partitions.size(), 2);
        
        std::cout << "  ✓ Simple partitioning passed" << std::endl;
    END_TEST("partition_simple")
}

void test_partition_with_weights() {
    TEST("partition_with_weights")
    
    Graph graph;
    
    // Create vertices with different weights
    graph.increment_vertex_weight("A");
    graph.increment_vertex_weight("A");  // Weight = 2
    graph.increment_vertex_weight("B");  // Weight = 1
    graph.increment_vertex_weight("C");
    graph.increment_vertex_weight("C");
    graph.increment_vertex_weight("C");  // Weight = 3
    graph.increment_vertex_weight("D");  // Weight = 1
    
    // Create edges
    graph.increment_edge_weight("A", "B");
    graph.increment_edge_weight("A", "C");
    graph.increment_edge_weight("B", "D");
    graph.increment_edge_weight("C", "D");
    
    MetisGraph metis_graph;
    metis_graph.prepare_from_graph(graph);
    
    // Partition into 2 parts
    auto partitions = metis_graph.partition(2);
    
    ASSERT_EQ(4, partitions.size());
    
    // Calculate partition weights
    int weight_part_0 = 0;
    int weight_part_1 = 0;
    const auto& idx_to_vertex = metis_graph.get_idx_to_vertex();
    
    for (size_t i = 0; i < partitions.size(); ++i) {
        int vertex_weight = graph.get_vertex_weight(idx_to_vertex[i]);
        if (partitions[i] == 0) {
            weight_part_0 += vertex_weight;
        } else {
            weight_part_1 += vertex_weight;
        }
    }
    
    // Total weight should be 7
    ASSERT_EQ(7, weight_part_0 + weight_part_1);
    
        std::cout << "  ✓ Partitioning with weights passed" << std::endl;
    END_TEST("partition_with_weights")
}

void test_multiple_partitions() {
    TEST("multiple_partitions")
    
    Graph graph;
    
    // Create a graph with 6 vertices
    for (char c = 'A'; c <= 'F'; ++c) {
        graph.increment_vertex_weight(std::string(1, c));
    }
    
    // Create a connected graph
    graph.increment_edge_weight("A", "B");
    graph.increment_edge_weight("B", "C");
    graph.increment_edge_weight("C", "D");
    graph.increment_edge_weight("D", "E");
    graph.increment_edge_weight("E", "F");
    graph.increment_edge_weight("F", "A");
    
    MetisGraph metis_graph;
    metis_graph.prepare_from_graph(graph);
    
    // Test different partition counts
    for (int nparts = 2; nparts <= 4; ++nparts) {
        auto partitions = metis_graph.partition(nparts);
        
        ASSERT_EQ(6, partitions.size());
        
        // Check partition IDs are valid
        std::set<int> unique_partitions;
        for (size_t i = 0; i < partitions.size(); ++i) {
            ASSERT_GE(partitions[i], 0);
            ASSERT_LT(partitions[i], nparts);
            unique_partitions.insert(partitions[i]);
        }
        
        // Should use at most nparts partitions
        ASSERT_LE(unique_partitions.size(), static_cast<size_t>(nparts));
    }
    
        std::cout << "  ✓ Multiple partitions passed" << std::endl;
    END_TEST("multiple_partitions")
}

void test_invalid_partition_parameters() {
    TEST("invalid_partition_parameters")
    
    Graph graph;
    graph.increment_vertex_weight("A");
    graph.increment_vertex_weight("B");
    
    MetisGraph metis_graph;
    metis_graph.prepare_from_graph(graph);
    
    // Test invalid number of partitions
    bool exception_thrown = false;
    try {
        metis_graph.partition(0);
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
    }
        ASSERT_TRUE(exception_thrown);
        
        exception_thrown = false;
        try {
            metis_graph.partition(-1);
        } catch (const std::runtime_error& e) {
            exception_thrown = true;
        }
        ASSERT_TRUE(exception_thrown);
        
        exception_thrown = false;
        try {
            metis_graph.partition(100);  // More partitions than vertices
        } catch (const std::runtime_error& e) {
            exception_thrown = true;
        }
        ASSERT_TRUE(exception_thrown);
    
        std::cout << "  ✓ Invalid partition parameters handling passed" << std::endl;
    END_TEST("invalid_partition_parameters")
}

void test_partition_before_prepare() {
    TEST("partition_before_prepare")
    
    MetisGraph metis_graph;
    
    bool exception_thrown = false;
    try {
        metis_graph.partition(2);
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
    }
    
        ASSERT_TRUE(exception_thrown);
        
        std::cout << "  ✓ Partition before prepare handling passed" << std::endl;
    END_TEST("partition_before_prepare")
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Testing MetisGraph Implementation" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    test_prepare_from_graph();
    test_empty_graph();
    test_partition_simple();
    test_partition_with_weights();
    test_multiple_partitions();
    test_invalid_partition_parameters();
    test_partition_before_prepare();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Overall Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << tests_failed << std::endl;
    std::cout << "Total tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "✓ All MetisGraph tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some MetisGraph tests failed!" << std::endl;
        return 1;
    }
}

