#include "HardRepartitioningKeyValueStorage.h"
#include "SoftRepartitioningKeyValueStorage.h"
#include "../keystorage/MapKeyStorage.h"
#include "../storage/MapStorageEngine.h"
#include "../utils/test_assertions.h"
#include <iostream>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_tracking_disabled() {
    TEST("tracking_disabled")
        SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
        
        // Verify tracking is disabled by default
        ASSERT_FALSE(storage.enable_tracking());
        std::cout << "  ✓ Tracking is disabled by default" << std::endl;
        
        // Write and read keys without tracking
        storage.write("key1", "value1");
        storage.write("key2", "value2");
        storage.read("key1");
        storage.read("key2");
        
        // Verify graph is still empty (no tracking occurred)
        const Graph& graph = storage.graph();
        ASSERT_EQ(0, graph.get_vertex_count());
        std::cout << "  ✓ Graph remains empty when tracking is disabled" << std::endl;
    END_TEST("tracking_disabled")
}

void test_tracking_enabled() {
    TEST("tracking_enabled")
        SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
        
        // Enable tracking
        storage.enable_tracking(true);
        ASSERT_TRUE(storage.enable_tracking());
        std::cout << "  ✓ Tracking enabled" << std::endl;
        
        // Write some keys (each write increments vertex weight by 1)
        storage.write("key1", "value1");
        storage.write("key2", "value2");
        storage.write("key3", "value3");
        
        const Graph& graph = storage.graph();
        
        // Each write should create a vertex with weight 1
        ASSERT_EQ(3, graph.get_vertex_count());
        ASSERT_EQ(1, graph.get_vertex_weight("key1"));
        ASSERT_EQ(1, graph.get_vertex_weight("key2"));
        ASSERT_EQ(1, graph.get_vertex_weight("key3"));
        std::cout << "  ✓ Three vertices created with weight 1 each" << std::endl;
        
        // Read key1 twice (should increment its weight by 2)
        storage.read("key1");
        storage.read("key1");
        
        ASSERT_EQ(3, graph.get_vertex_weight("key1"));  // 1 write + 2 reads
        ASSERT_EQ(1, graph.get_vertex_weight("key2"));  // Still 1
        std::cout << "  ✓ key1 weight is now 3 (1 write + 2 reads)" << std::endl;
        
        // Write to key1 again (should increment by 1)
        storage.write("key1", "updated_value1");
        
        ASSERT_EQ(4, graph.get_vertex_weight("key1"));
        std::cout << "  ✓ key1 weight is now 4 after another write" << std::endl;
    END_TEST("tracking_enabled")
}

void test_access_frequency_tracking() {
    TEST("access_frequency_tracking")
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
        
        ASSERT_EQ(11, hot_weight);   // 1 write + 10 reads
        ASSERT_EQ(4, warm_weight);   // 1 write + 3 reads
        ASSERT_EQ(1, cold_weight);   // 1 write only
        
        std::cout << "  ✓ hot_key weight: " << hot_weight << " (1 write + 10 reads)" << std::endl;
        std::cout << "  ✓ warm_key weight: " << warm_weight << " (1 write + 3 reads)" << std::endl;
        std::cout << "  ✓ cold_key weight: " << cold_weight << " (1 write only)" << std::endl;
        std::cout << "  ✓ Access frequency correctly tracked" << std::endl;
    END_TEST("access_frequency_tracking")
}

void test_clear_graph() {
    TEST("clear_graph")
        SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
        storage.enable_tracking(true);
        
        // Add some tracked data
        storage.write("key1", "value1");
        storage.write("key2", "value2");
        storage.read("key1");
        
        const Graph& graph = storage.graph();
        ASSERT_EQ(2, graph.get_vertex_count());
        std::cout << "  ✓ Graph has 2 vertices" << std::endl;
        
        // Clear the graph
        storage.clear_graph();
        
        ASSERT_EQ(0, graph.get_vertex_count());
        std::cout << "  ✓ Graph cleared successfully" << std::endl;
        
        // New accesses should start fresh
        storage.write("key1", "new_value");
        ASSERT_EQ(1, graph.get_vertex_weight("key1"));
        std::cout << "  ✓ New accesses tracked from fresh state" << std::endl;
    END_TEST("clear_graph")
}

void test_toggle_tracking() {
    TEST("toggle_tracking")
        SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
        
        // Enable tracking and do some operations
        storage.enable_tracking(true);
        storage.write("key1", "value1");
        storage.read("key1");
        
        const Graph& graph = storage.graph();
        ASSERT_EQ(2, graph.get_vertex_weight("key1"));
        std::cout << "  ✓ key1 tracked with weight 2" << std::endl;
        
        // Disable tracking
        storage.enable_tracking(false);
        storage.read("key1");
        storage.read("key1");
        
        // Weight should remain the same (no tracking)
        ASSERT_EQ(2, graph.get_vertex_weight("key1"));
        std::cout << "  ✓ Weight unchanged while tracking disabled" << std::endl;
        
        // Re-enable tracking
        storage.enable_tracking(true);
        storage.read("key1");
        
        // Weight should now increase
        ASSERT_EQ(3, graph.get_vertex_weight("key1"));
        std::cout << "  ✓ Tracking resumed after re-enabling" << std::endl;
    END_TEST("toggle_tracking")
}

void test_scan_with_graph_tracking() {
    TEST("scan_with_graph_tracking")
        SoftRepartitioningKeyValueStorage<MapStorageEngine, MapKeyStorage, MapKeyStorage> storage(4);
        storage.enable_tracking(true);
        
        // Write some keys with a common prefix
        storage.write("user:001", "alice");
        storage.write("user:002", "bob");
        storage.write("user:003", "charlie");
        storage.write("user:004", "diana");
        
        const Graph& graph = storage.graph();
        
        // After writes, each vertex should have weight 1
        ASSERT_EQ(1, graph.get_vertex_weight("user:001"));
        ASSERT_EQ(1, graph.get_vertex_weight("user:002"));
        ASSERT_EQ(1, graph.get_vertex_weight("user:003"));
        ASSERT_EQ(1, graph.get_vertex_weight("user:004"));
        std::cout << "  ✓ Four vertices created with weight 1 each" << std::endl;
        
        // No edges should exist yet
        ASSERT_EQ(0, graph.get_edge_count());
        std::cout << "  ✓ No edges exist before scan" << std::endl;
        
        // Perform a scan (which calls multi_key_graph_update)
        auto results = storage.scan("user:", 3);
        
        // Scan should return 3 results
        ASSERT_EQ(3, results.size());
        std::cout << "  ✓ Scan returned 3 results" << std::endl;
        
        // After scan, the 3 scanned keys should have weight 2 (1 write + 1 scan)
        // We need to check which keys were returned by the scan
        std::vector<std::string> scanned_keys;
        for (const auto& [key, value] : results) {
            scanned_keys.push_back(key);
            ASSERT_EQ(2, graph.get_vertex_weight(key));
        }
        std::cout << "  ✓ Scanned keys have weight 2 (1 write + 1 scan)" << std::endl;
        
        // Edges should now exist between all pairs of scanned keys
        // For 3 keys, we should have 3 edges: (0,1), (0,2), (1,2)
        ASSERT_EQ(3, graph.get_edge_count());
        std::cout << "  ✓ Three edges created between scanned keys" << std::endl;
        
        // Verify edges exist between all pairs
        for (size_t i = 0; i < scanned_keys.size(); ++i) {
            for (size_t j = i + 1; j < scanned_keys.size(); ++j) {
                ASSERT_TRUE(graph.has_edge(scanned_keys[i], scanned_keys[j]));
                ASSERT_EQ(1, graph.get_edge_weight(scanned_keys[i], scanned_keys[j]));
            }
        }
        std::cout << "  ✓ All edge pairs verified with weight 1" << std::endl;
    END_TEST("scan_with_graph_tracking")
}

void test_repeated_scans() {
    TEST("repeated_scans")
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
        ASSERT_EQ(2, first_scan.size());
        
        std::string key1 = first_scan[0].first;
        std::string key2 = first_scan[1].first;
        
        // Each key should have weight 7 (1 write + 6 scans)
        ASSERT_EQ(7, graph.get_vertex_weight(key1));
        ASSERT_EQ(7, graph.get_vertex_weight(key2));
        std::cout << "  ✓ Keys accessed 7 times (1 write + 6 scans)" << std::endl;
        
        // The edge between them should have weight 6 (one for each scan)
        ASSERT_TRUE(graph.has_edge(key1, key2));
        ASSERT_EQ(6, graph.get_edge_weight(key1, key2));
        std::cout << "  ✓ Edge weight is 6 (incremented on each scan)" << std::endl;
        
        std::cout << "  ✓ Repeated scans correctly build edge weights" << std::endl;
    END_TEST("repeated_scans")
}

void test_co_access_patterns() {
    TEST("co_access_patterns")
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
            ASSERT_EQ(11, edge_weight);
            std::cout << "  ✓ group1 keys have strong co-access edge (weight: " << edge_weight << ")" << std::endl;
        }
        
        if (group2_keys.size() >= 2) {
            std::string g2_key1 = group2_keys[0].first;
            std::string g2_key2 = group2_keys[1].first;
            
            // Edge weight should be 10 + 1
            int edge_weight = graph.get_edge_weight(g2_key1, g2_key2);
            ASSERT_EQ(11, edge_weight);
            std::cout << "  ✓ group2 keys have strong co-access edge (weight: " << edge_weight << ")" << std::endl;
        }
        
        // Check that keys from different groups have no edges
        if (group1_keys.size() >= 1 && group2_keys.size() >= 1) {
            std::string g1_key = group1_keys[0].first;
            std::string g2_key = group2_keys[0].first;
            
            ASSERT_FALSE(graph.has_edge(g1_key, g2_key));
            std::cout << "  ✓ Keys from different groups have no edges" << std::endl;
        }
        
        std::cout << "  ✓ Co-access patterns correctly detected" << std::endl;
    END_TEST("co_access_patterns")
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Testing Graph Tracking in RepartitioningKeyValueStorage" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    test_tracking_disabled();
    test_tracking_enabled();
    test_access_frequency_tracking();
    test_clear_graph();
    test_toggle_tracking();
    test_scan_with_graph_tracking();
    test_repeated_scans();
    test_co_access_patterns();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Overall Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << tests_failed << std::endl;
    std::cout << "Total tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "✓ All Graph Tracking Tests PASSED!" << std::endl;
        std::cout << std::endl;
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
    } else {
        std::cout << "✗ Some Graph Tracking tests failed!" << std::endl;
        return 1;
    }
}

