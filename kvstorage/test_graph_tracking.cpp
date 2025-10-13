#include "RepartitioningKeyValueStorage.h"
#include "../keystorage/MapKeyStorage.h"
#include "../storage/MapStorageEngine.h"
#include <iostream>
#include <cassert>

void test_tracking_disabled() {
    std::cout << "Test 1: Tracking Disabled" << std::endl;
    std::cout << "-------------------------" << std::endl;
    
    RepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    
    // Verify tracking is disabled by default
    assert(!storage.is_tracking_enabled());
    std::cout << "  ✓ Tracking is disabled by default" << std::endl;
    
    // Write and read keys without tracking
    storage.write("key1", "value1");
    storage.write("key2", "value2");
    storage.read("key1");
    storage.read("key2");
    
    // Verify graph is still empty (no tracking occurred)
    const Graph& graph = storage.get_graph();
    assert(graph.get_vertex_count() == 0);
    std::cout << "  ✓ Graph remains empty when tracking is disabled" << std::endl;
    std::cout << "  ✓ Test 1 PASSED" << std::endl << std::endl;
}

void test_tracking_enabled() {
    std::cout << "Test 2: Tracking Enabled" << std::endl;
    std::cout << "------------------------" << std::endl;
    
    RepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    
    // Enable tracking
    storage.set_tracking_enabled(true);
    assert(storage.is_tracking_enabled());
    std::cout << "  ✓ Tracking enabled" << std::endl;
    
    // Write some keys (each write increments vertex weight by 1)
    storage.write("key1", "value1");
    storage.write("key2", "value2");
    storage.write("key3", "value3");
    
    const Graph& graph = storage.get_graph();
    
    // Each write should create a vertex with weight 1
    assert(graph.get_vertex_count() == 3);
    assert(graph.get_vertex_weight("key1") == 1);
    assert(graph.get_vertex_weight("key2") == 1);
    assert(graph.get_vertex_weight("key3") == 1);
    std::cout << "  ✓ Three vertices created with weight 1 each" << std::endl;
    
    // Read key1 twice (should increment its weight by 2)
    storage.read("key1");
    storage.read("key1");
    
    assert(graph.get_vertex_weight("key1") == 3);  // 1 write + 2 reads
    assert(graph.get_vertex_weight("key2") == 1);  // Still 1
    std::cout << "  ✓ key1 weight is now 3 (1 write + 2 reads)" << std::endl;
    
    // Write to key1 again (should increment by 1)
    storage.write("key1", "updated_value1");
    
    assert(graph.get_vertex_weight("key1") == 4);
    std::cout << "  ✓ key1 weight is now 4 after another write" << std::endl;
    std::cout << "  ✓ Test 2 PASSED" << std::endl << std::endl;
}

void test_access_frequency_tracking() {
    std::cout << "Test 3: Access Frequency Tracking" << std::endl;
    std::cout << "----------------------------------" << std::endl;
    
    RepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    storage.set_tracking_enabled(true);
    
    // Simulate different access patterns
    storage.write("hot_key", "value");
    storage.write("warm_key", "value");
    storage.write("cold_key", "value");
    
    // Access hot_key many times
    for (int i = 0; i < 10; ++i) {
        storage.read("hot_key");
    }
    
    // Access warm_key a few times
    for (int i = 0; i < 3; ++i) {
        storage.read("warm_key");
    }
    
    // Don't access cold_key at all after writing
    
    const Graph& graph = storage.get_graph();
    
    // Check weights reflect access patterns
    int hot_weight = graph.get_vertex_weight("hot_key");
    int warm_weight = graph.get_vertex_weight("warm_key");
    int cold_weight = graph.get_vertex_weight("cold_key");
    
    assert(hot_weight == 11);   // 1 write + 10 reads
    assert(warm_weight == 4);   // 1 write + 3 reads
    assert(cold_weight == 1);   // 1 write only
    
    std::cout << "  ✓ hot_key weight: " << hot_weight << " (1 write + 10 reads)" << std::endl;
    std::cout << "  ✓ warm_key weight: " << warm_weight << " (1 write + 3 reads)" << std::endl;
    std::cout << "  ✓ cold_key weight: " << cold_weight << " (1 write only)" << std::endl;
    std::cout << "  ✓ Access frequency correctly tracked" << std::endl;
    std::cout << "  ✓ Test 3 PASSED" << std::endl << std::endl;
}

void test_clear_graph() {
    std::cout << "Test 4: Clear Graph" << std::endl;
    std::cout << "-------------------" << std::endl;
    
    RepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    storage.set_tracking_enabled(true);
    
    // Add some tracked data
    storage.write("key1", "value1");
    storage.write("key2", "value2");
    storage.read("key1");
    
    const Graph& graph = storage.get_graph();
    assert(graph.get_vertex_count() == 2);
    std::cout << "  ✓ Graph has 2 vertices" << std::endl;
    
    // Clear the graph
    storage.clear_graph();
    
    assert(graph.get_vertex_count() == 0);
    std::cout << "  ✓ Graph cleared successfully" << std::endl;
    
    // New accesses should start fresh
    storage.write("key1", "new_value");
    assert(graph.get_vertex_weight("key1") == 1);
    std::cout << "  ✓ New accesses tracked from fresh state" << std::endl;
    std::cout << "  ✓ Test 4 PASSED" << std::endl << std::endl;
}

void test_toggle_tracking() {
    std::cout << "Test 5: Toggle Tracking" << std::endl;
    std::cout << "-----------------------" << std::endl;
    
    RepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    
    // Enable tracking and do some operations
    storage.set_tracking_enabled(true);
    storage.write("key1", "value1");
    storage.read("key1");
    
    const Graph& graph = storage.get_graph();
    assert(graph.get_vertex_weight("key1") == 2);
    std::cout << "  ✓ key1 tracked with weight 2" << std::endl;
    
    // Disable tracking
    storage.set_tracking_enabled(false);
    storage.read("key1");
    storage.read("key1");
    
    // Weight should remain the same (no tracking)
    assert(graph.get_vertex_weight("key1") == 2);
    std::cout << "  ✓ Weight unchanged while tracking disabled" << std::endl;
    
    // Re-enable tracking
    storage.set_tracking_enabled(true);
    storage.read("key1");
    
    // Weight should now increase
    assert(graph.get_vertex_weight("key1") == 3);
    std::cout << "  ✓ Tracking resumed after re-enabling" << std::endl;
    std::cout << "  ✓ Test 5 PASSED" << std::endl << std::endl;
}

int main() {
    std::cout << "=== Testing Graph Tracking in RepartitioningKeyValueStorage ===" << std::endl << std::endl;
    
    try {
        test_tracking_disabled();
        test_tracking_enabled();
        test_access_frequency_tracking();
        test_clear_graph();
        test_toggle_tracking();
        
        std::cout << "========================================" << std::endl;
        std::cout << "  All Graph Tracking Tests PASSED!" << std::endl;
        std::cout << "========================================" << std::endl << std::endl;
        
        std::cout << "Summary:" << std::endl;
        std::cout << "  ✓ Tracking can be enabled/disabled dynamically" << std::endl;
        std::cout << "  ✓ Graph correctly tracks key access frequencies" << std::endl;
        std::cout << "  ✓ Hot keys can be identified by vertex weights" << std::endl;
        std::cout << "  ✓ Graph can be cleared for fresh tracking periods" << std::endl;
        std::cout << "  ✓ Ready for intelligent repartitioning decisions" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}

