#include "Graph.h"
#include "MetisGraph.h"
#include <cassert>
#include <iostream>
#include <set>

void test_prepare_from_graph() {
    std::cout << "Testing prepare_from_graph..." << std::endl;
    
    Graph graph;
    graph.increment_vertex_weight("A");
    graph.increment_vertex_weight("B");
    graph.increment_vertex_weight("C");
    graph.increment_edge_weight("A", "B");
    graph.increment_edge_weight("B", "C");
    graph.increment_edge_weight("C", "A");
    
    MetisGraph metis_graph;
    assert(!metis_graph.is_prepared());
    
    metis_graph.prepare_from_graph(graph);
    
    assert(metis_graph.is_prepared());
    assert(metis_graph.get_num_vertices() == 3);
    assert(metis_graph.get_num_edges() == 3);
    
    // Check vertex mappings
    const auto& vertex_to_idx = metis_graph.get_vertex_to_idx();
    const auto& idx_to_vertex = metis_graph.get_idx_to_vertex();
    
    assert(vertex_to_idx.size() == 3);
    assert(idx_to_vertex.size() == 3);
    
    // Check CSR structure
    const auto& xadj = metis_graph.get_xadj();
    const auto& adjncy = metis_graph.get_adjncy();
    
    assert(xadj.size() == 4);  // nvtxs + 1
    assert(adjncy.size() == 3);
    
    std::cout << "  ✓ Prepare from graph passed" << std::endl;
}

void test_empty_graph() {
    std::cout << "Testing empty graph..." << std::endl;
    
    Graph graph;
    MetisGraph metis_graph;
    
    bool exception_thrown = false;
    try {
        metis_graph.prepare_from_graph(graph);
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
    }
    
    assert(exception_thrown);
    
    std::cout << "  ✓ Empty graph handling passed" << std::endl;
}

void test_partition_simple() {
    std::cout << "Testing simple partitioning..." << std::endl;
    
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
    
    assert(partitions.size() == 4);
    
    // Check partition IDs are valid (0 or 1)
    std::set<int> unique_partitions;
    for (size_t i = 0; i < partitions.size(); ++i) {
        assert(partitions[i] >= 0 && partitions[i] < 2);
        unique_partitions.insert(partitions[i]);
    }
    
    // Should use both partitions
    assert(unique_partitions.size() <= 2);
    
    std::cout << "  ✓ Simple partitioning passed" << std::endl;
}

void test_partition_with_weights() {
    std::cout << "Testing partitioning with weights..." << std::endl;
    
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
    
    assert(partitions.size() == 4);
    
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
    assert(weight_part_0 + weight_part_1 == 7);
    
    std::cout << "  ✓ Partitioning with weights passed" << std::endl;
}

void test_multiple_partitions() {
    std::cout << "Testing multiple partitions..." << std::endl;
    
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
        
        assert(partitions.size() == 6);
        
        // Check partition IDs are valid
        std::set<int> unique_partitions;
        for (size_t i = 0; i < partitions.size(); ++i) {
            assert(partitions[i] >= 0 && partitions[i] < nparts);
            unique_partitions.insert(partitions[i]);
        }
        
        // Should use at most nparts partitions
        assert(unique_partitions.size() <= static_cast<size_t>(nparts));
    }
    
    std::cout << "  ✓ Multiple partitions passed" << std::endl;
}

void test_invalid_partition_parameters() {
    std::cout << "Testing invalid partition parameters..." << std::endl;
    
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
    assert(exception_thrown);
    
    exception_thrown = false;
    try {
        metis_graph.partition(-1);
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
    }
    assert(exception_thrown);
    
    exception_thrown = false;
    try {
        metis_graph.partition(100);  // More partitions than vertices
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
    }
    assert(exception_thrown);
    
    std::cout << "  ✓ Invalid partition parameters handling passed" << std::endl;
}

void test_partition_before_prepare() {
    std::cout << "Testing partition before prepare..." << std::endl;
    
    MetisGraph metis_graph;
    
    bool exception_thrown = false;
    try {
        metis_graph.partition(2);
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
    }
    
    assert(exception_thrown);
    
    std::cout << "  ✓ Partition before prepare handling passed" << std::endl;
}

int main() {
    std::cout << "=== Running MetisGraph Tests ===" << std::endl << std::endl;
    
    try {
        test_prepare_from_graph();
        test_empty_graph();
        test_partition_simple();
        test_partition_with_weights();
        test_multiple_partitions();
        test_invalid_partition_parameters();
        test_partition_before_prepare();
        
        std::cout << std::endl << "✓ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}

