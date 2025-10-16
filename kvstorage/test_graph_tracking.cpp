#include "HardRepartitioningKeyValueStorage.h"
#include "SoftRepartitioningKeyValueStorage.h"
#include "../keystorage/MapKeyStorage.h"
#include "../storage/MapStorageEngine.h"
#include <iostream>
#include <cassert>

void test_tracking_disabled() {
    std::cout << "Test 1: Tracking Disabled" << std::endl;
    std::cout << "-------------------------" << std::endl;
    
    SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    
    // Verify tracking is disabled by default
    assert(!storage.enable_tracking());
    std::cout << "  ✓ Tracking is disabled by default" << std::endl;
    
    // Write and read keys without tracking
    storage.write("key1", "value1");
    storage.write("key2", "value2");
    storage.read("key1");
    storage.read("key2");
    
    // Verify graph is still empty (no tracking occurred)
    const Graph& graph = storage.graph();
    assert(graph.get_vertex_count() == 0);
    std::cout << "  ✓ Graph remains empty when tracking is disabled" << std::endl;
    std::cout << "  ✓ Test 1 PASSED" << std::endl << std::endl;
}

void test_tracking_enabled() {
    std::cout << "Test 2: Tracking Enabled" << std::endl;
    std::cout << "------------------------" << std::endl;
    
    SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    
    // Enable tracking
    storage.enable_tracking(true);
    assert(storage.enable_tracking());
    std::cout << "  ✓ Tracking enabled" << std::endl;
    
    // Write some keys (each write increments vertex weight by 1)
    storage.write("key1", "value1");
    storage.write("key2", "value2");
    storage.write("key3", "value3");
    
    const Graph& graph = storage.graph();
    
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
    
    SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    storage.enable_tracking(true);
    
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
    
    const Graph& graph = storage.graph();
    
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
    
    SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    storage.enable_tracking(true);
    
    // Add some tracked data
    storage.write("key1", "value1");
    storage.write("key2", "value2");
    storage.read("key1");
    
    const Graph& graph = storage.graph();
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
    
    SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    
    // Enable tracking and do some operations
    storage.enable_tracking(true);
    storage.write("key1", "value1");
    storage.read("key1");
    
    const Graph& graph = storage.graph();
    assert(graph.get_vertex_weight("key1") == 2);
    std::cout << "  ✓ key1 tracked with weight 2" << std::endl;
    
    // Disable tracking
    storage.enable_tracking(false);
    storage.read("key1");
    storage.read("key1");
    
    // Weight should remain the same (no tracking)
    assert(graph.get_vertex_weight("key1") == 2);
    std::cout << "  ✓ Weight unchanged while tracking disabled" << std::endl;
    
    // Re-enable tracking
    storage.enable_tracking(true);
    storage.read("key1");
    
    // Weight should now increase
    assert(graph.get_vertex_weight("key1") == 3);
    std::cout << "  ✓ Tracking resumed after re-enabling" << std::endl;
    std::cout << "  ✓ Test 5 PASSED" << std::endl << std::endl;
}

void test_scan_with_graph_tracking() {
    std::cout << "Test 6: Scan with Graph Tracking" << std::endl;
    std::cout << "---------------------------------" << std::endl;
    
    SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    storage.enable_tracking(true);
    
    // Write some keys with a common prefix
    storage.write("user:001", "alice");
    storage.write("user:002", "bob");
    storage.write("user:003", "charlie");
    storage.write("user:004", "diana");
    
    const Graph& graph = storage.graph();
    
    // After writes, each vertex should have weight 1
    assert(graph.get_vertex_weight("user:001") == 1);
    assert(graph.get_vertex_weight("user:002") == 1);
    assert(graph.get_vertex_weight("user:003") == 1);
    assert(graph.get_vertex_weight("user:004") == 1);
    std::cout << "  ✓ Four vertices created with weight 1 each" << std::endl;
    
    // No edges should exist yet
    assert(graph.get_edge_count() == 0);
    std::cout << "  ✓ No edges exist before scan" << std::endl;
    
    // Perform a scan (which calls multi_key_graph_update)
    auto results = storage.scan("user:", 3);
    
    // Scan should return 3 results
    assert(results.size() == 3);
    std::cout << "  ✓ Scan returned 3 results" << std::endl;
    
    // After scan, the 3 scanned keys should have weight 2 (1 write + 1 scan)
    // We need to check which keys were returned by the scan
    std::vector<std::string> scanned_keys;
    for (const auto& [key, value] : results) {
        scanned_keys.push_back(key);
        assert(graph.get_vertex_weight(key) == 2);
    }
    std::cout << "  ✓ Scanned keys have weight 2 (1 write + 1 scan)" << std::endl;
    
    // Edges should now exist between all pairs of scanned keys
    // For 3 keys, we should have 3 edges: (0,1), (0,2), (1,2)
    assert(graph.get_edge_count() == 3);
    std::cout << "  ✓ Three edges created between scanned keys" << std::endl;
    
    // Verify edges exist between all pairs
    for (size_t i = 0; i < scanned_keys.size(); ++i) {
        for (size_t j = i + 1; j < scanned_keys.size(); ++j) {
            assert(graph.has_edge(scanned_keys[i], scanned_keys[j]));
            assert(graph.get_edge_weight(scanned_keys[i], scanned_keys[j]) == 1);
        }
    }
    std::cout << "  ✓ All edge pairs verified with weight 1" << std::endl;
    std::cout << "  ✓ Test 6 PASSED" << std::endl << std::endl;
}

void test_repeated_scans() {
    std::cout << "Test 7: Repeated Scans Build Edge Weights" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    
    SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    storage.enable_tracking(true);
    
    // Write keys
    storage.write("item:a", "value_a");
    storage.write("item:b", "value_b");
    storage.write("item:c", "value_c");
    
    const Graph& graph = storage.graph();
    
    // Perform the same scan 5 times
    for (int i = 0; i < 5; ++i) {
        storage.scan("item:", 2);
    }
    
    // Get the first two keys (sorted by the storage) - this is the 6th scan
    auto first_scan = storage.scan("item:", 2);
    assert(first_scan.size() == 2);
    
    std::string key1 = first_scan[0].first;
    std::string key2 = first_scan[1].first;
    
    // Each key should have weight 7 (1 write + 6 scans)
    assert(graph.get_vertex_weight(key1) == 7);
    assert(graph.get_vertex_weight(key2) == 7);
    std::cout << "  ✓ Keys accessed 7 times (1 write + 6 scans)" << std::endl;
    
    // The edge between them should have weight 6 (one for each scan)
    assert(graph.has_edge(key1, key2));
    assert(graph.get_edge_weight(key1, key2) == 6);
    std::cout << "  ✓ Edge weight is 6 (incremented on each scan)" << std::endl;
    
    std::cout << "  ✓ Repeated scans correctly build edge weights" << std::endl;
    std::cout << "  ✓ Test 7 PASSED" << std::endl << std::endl;
}

void test_co_access_patterns() {
    std::cout << "Test 8: Co-Access Pattern Detection" << std::endl;
    std::cout << "------------------------------------" << std::endl;
    
    SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
    storage.enable_tracking(true);
    
    // Write keys in different groups
    storage.write("group1:a", "value");
    storage.write("group1:b", "value");
    storage.write("group1:c", "value");
    storage.write("group2:x", "value");
    storage.write("group2:y", "value");
    storage.write("group2:z", "value");
    
    const Graph& graph = storage.graph();
    
    // Scan group1 keys together multiple times
    for (int i = 0; i < 10; ++i) {
        storage.scan("group1:", 3);
    }
    
    // Scan group2 keys together multiple times
    for (int i = 0; i < 10; ++i) {
        storage.scan("group2:", 3);
    }
    
    // Get the actual keys from scans
    auto group1_keys = storage.scan("group1:", 3);
    auto group2_keys = storage.scan("group2:", 3);
    
    // Check that keys within the same group have strong edges
    if (group1_keys.size() >= 2) {
        std::string g1_key1 = group1_keys[0].first;
        std::string g1_key2 = group1_keys[1].first;
        
        // Edge weight should be 10 (scanned together 10 times) + 1 (final scan)
        int edge_weight = graph.get_edge_weight(g1_key1, g1_key2);
        assert(edge_weight == 11);
        std::cout << "  ✓ group1 keys have strong co-access edge (weight: " << edge_weight << ")" << std::endl;
    }
    
    if (group2_keys.size() >= 2) {
        std::string g2_key1 = group2_keys[0].first;
        std::string g2_key2 = group2_keys[1].first;
        
        // Edge weight should be 10 + 1
        int edge_weight = graph.get_edge_weight(g2_key1, g2_key2);
        assert(edge_weight == 11);
        std::cout << "  ✓ group2 keys have strong co-access edge (weight: " << edge_weight << ")" << std::endl;
    }
    
    // Check that keys from different groups have no edges
    if (group1_keys.size() >= 1 && group2_keys.size() >= 1) {
        std::string g1_key = group1_keys[0].first;
        std::string g2_key = group2_keys[0].first;
        
        assert(!graph.has_edge(g1_key, g2_key));
        std::cout << "  ✓ Keys from different groups have no edges" << std::endl;
    }
    
    std::cout << "  ✓ Co-access patterns correctly detected" << std::endl;
    std::cout << "  ✓ Test 8 PASSED" << std::endl << std::endl;
}

int main() {
    std::cout << "=== Testing Graph Tracking in RepartitioningKeyValueStorage ===" << std::endl << std::endl;
    
    try {
        test_tracking_disabled();
        test_tracking_enabled();
        test_access_frequency_tracking();
        test_clear_graph();
        test_toggle_tracking();
        test_scan_with_graph_tracking();
        test_repeated_scans();
        test_co_access_patterns();
        
        std::cout << "========================================" << std::endl;
        std::cout << "  All Graph Tracking Tests PASSED!" << std::endl;
        std::cout << "========================================" << std::endl << std::endl;
        
        std::cout << "Summary:" << std::endl;
        std::cout << "  ✓ Tracking can be enabled/disabled dynamically" << std::endl;
        std::cout << "  ✓ Graph correctly tracks key access frequencies" << std::endl;
        std::cout << "  ✓ Hot keys can be identified by vertex weights" << std::endl;
        std::cout << "  ✓ Graph can be cleared for fresh tracking periods" << std::endl;
        std::cout << "  ✓ Scan operations track co-access patterns with edges" << std::endl;
        std::cout << "  ✓ Edge weights reflect frequency of co-access" << std::endl;
        std::cout << "  ✓ Keys accessed together can be identified for co-location" << std::endl;
        std::cout << "  ✓ Ready for intelligent repartitioning decisions" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}

