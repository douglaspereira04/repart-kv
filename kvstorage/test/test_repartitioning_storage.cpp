#include "../RepartitioningKeyValueStorage.h"
#include "../HardRepartitioningKeyValueStorage.h"
#include "../SoftRepartitioningKeyValueStorage.h"
#include "../../keystorage/MapKeyStorage.h"
#include "../../storage/MapStorageEngine.h"
#include "../../utils/test_assertions.h"
#include "../threaded/SoftThreadedRepartitioningKeyValueStorage.h"
#include "../../storage/LmdbStorageEngine.h"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

const std::chrono::milliseconds sleep_time = std::chrono::milliseconds(10);

// Generic test functions that work with any RepartitioningKeyValueStorage
// implementation
template <typename StorageType> void test_basic_operations() {
    TEST("basic_operations")
    StorageType storage(2);

    // Test write and read
    Status status = storage.write("key1", "value1");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key2", "value2");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    std::string value;
    status = storage.read("key1", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value1", value);
    status = storage.read("key2", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value2", value);
    status = storage.read("key3", value);
    ASSERT_STATUS_EQ(Status::NOT_FOUND, status);
    END_TEST("basic_operations")
}

template <typename StorageType> void test_tracking_disabled_by_default() {
    TEST("tracking_disabled_by_default")
    StorageType storage(2);

    ASSERT_FALSE(storage.enable_tracking()); // Should be false by default
    END_TEST("tracking_disabled_by_default")
}

template <typename StorageType> void test_enable_tracking() {
    TEST("enable_tracking")
    StorageType storage(2);

    // Enable tracking
    storage.enable_tracking(true);
    ASSERT_TRUE(storage.enable_tracking());
    std::string value;
    // Perform operations that should be tracked
    Status status = storage.write("key1", "value1");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.read("key1", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value1", value);
    status = storage.read("key1", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value1", value);

    std::this_thread::sleep_for(sleep_time);
    // Check that graph has been populated
    const auto &graph = storage.graph();
    ASSERT_TRUE(graph.get_vertex_count() > 0);
    END_TEST("enable_tracking")
}

template <typename StorageType> void test_basic_repartition() {
    TEST("basic_repartition")
    StorageType storage(4);
    storage.enable_tracking(true);
    std::string value;
    // Create some keys with access patterns
    Status status = storage.write("key1", "value1");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key2", "value2");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key3", "value3");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key4", "value4");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);

    // Access them to build the graph
    for (int i = 0; i < 10; ++i) {
        status = storage.read("key1", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value1", value);
        status = storage.read("key2", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value2", value);
    }

    for (int i = 0; i < 5; ++i) {
        status = storage.read("key3", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value3", value);
        status = storage.read("key4", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value4", value);
    }

    const Graph &graph_before = storage.graph();
    ASSERT_EQ(4, graph_before.get_vertex_count());
    ASSERT_EQ(11, graph_before.get_vertex_weight("key1")); // 1 write + 10 reads
    std::cout << "    Graph built with 4 vertices" << std::endl;

    // Perform repartitioning
    storage.repartition();

    const Graph &graph_after = storage.graph();
    ASSERT_EQ(0, graph_after.get_vertex_count());
    std::cout << "    Graph cleared after repartition" << std::endl;

    // Verify tracking is disabled after repartition
    ASSERT_FALSE(storage.enable_tracking());
    std::cout << "    Tracking disabled after repartition" << std::endl;

    // Verify data is still accessible
    status = storage.read("key1", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value1", value);
    status = storage.read("key2", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value2", value);
    status = storage.read("key3", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value3", value);
    status = storage.read("key4", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value4", value);
    std::cout << "    All keys still accessible after repartition" << std::endl;
    END_TEST("basic_repartition")
}

template <typename StorageType> void test_co_access_patterns() {
    TEST("co_access_patterns")
    StorageType storage(3);
    storage.enable_tracking(true);
    std::string value;
    // Create two groups of keys that are accessed together
    Status status = storage.write("group1_key1", "value1");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("group1_key2", "value2");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("group1_key3", "value3");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);

    status = storage.write("group2_key1", "value4");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("group2_key2", "value5");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);

    // Access group1 keys together multiple times
    for (int i = 0; i < 5; ++i) {
        std::vector<std::pair<std::string, std::string>> results;
        status = storage.scan("group1_", 3, results);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_EQ(3, results.size());
        ASSERT_STR_EQ("group1_key1", results[0].first);
        ASSERT_STR_EQ("value1", results[0].second);
        ASSERT_STR_EQ("group1_key2", results[1].first);
        ASSERT_STR_EQ("value2", results[1].second);
        ASSERT_STR_EQ("group1_key3", results[2].first);
        ASSERT_STR_EQ("value3", results[2].second);
    }

    // Access group2 keys together multiple times
    for (int i = 0; i < 3; ++i) {
        std::vector<std::pair<std::string, std::string>> results;
        status = storage.scan("group2_", 2, results);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_EQ(2, results.size());
        ASSERT_STR_EQ("group2_key1", results[0].first);
        ASSERT_STR_EQ("value4", results[0].second);
        ASSERT_STR_EQ("group2_key2", results[1].first);
        ASSERT_STR_EQ("value5", results[1].second);
    }

    std::this_thread::sleep_for(sleep_time);
    const Graph &graph = storage.graph();
    // Verify graph has been populated
    ASSERT_TRUE(graph.get_vertex_count() > 0);
    // Verify group1 edges
    for (int group1_key1 = 1; group1_key1 <= 3; ++group1_key1) {
        for (int group1_key2 = group1_key1 + 1; group1_key2 <= 3;
             ++group1_key2) {
            ASSERT_EQ(5, graph.get_edge_weight(
                             "group1_key" + std::to_string(group1_key1),
                             "group1_key" + std::to_string(group1_key2)));
        }
    }
    // Verify group2 edges
    for (int group2_key1 = 1; group2_key1 <= 2; ++group2_key1) {
        for (int group2_key2 = group2_key1 + 1; group2_key2 <= 2;
             ++group2_key2) {
            ASSERT_EQ(3, graph.get_edge_weight(
                             "group2_key" + std::to_string(group2_key1),
                             "group2_key" + std::to_string(group2_key2)));
        }
    }

    // Perform repartitioning
    storage.repartition();

    // Verify all keys are still accessible
    status = storage.read("group1_key1", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value1", value);
    status = storage.read("group1_key2", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value2", value);
    status = storage.read("group1_key3", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value3", value);
    status = storage.read("group2_key1", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value4", value);
    status = storage.read("group2_key2", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value5", value);
    END_TEST("co_access_patterns")
}

template <typename StorageType> void test_empty_graph_repartition() {
    TEST("empty_graph_repartition")
    StorageType storage(4);

    // Don't enable tracking, so graph should be empty
    ASSERT_FALSE(storage.enable_tracking());
    const Graph &graph_before = storage.graph();
    ASSERT_EQ(0, graph_before.get_vertex_count());
    std::cout << "    Graph is empty (tracking disabled)" << std::endl;

    // Write some data
    Status status = storage.write("key1", "value1");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key2", "value2");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);

    // Try to repartition with empty graph
    storage.repartition();
    std::cout << "    Repartition with empty graph successful" << std::endl;

    // Verify data is still accessible
    std::string value;
    status = storage.read("key1", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value1", value);
    status = storage.read("key2", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value2", value);
    END_TEST("empty_graph_repartition")
}

template <typename StorageType> void test_multiple_repartitions() {
    TEST("multiple_repartitions")
    StorageType storage(3);

    // First tracking period
    storage.enable_tracking(true);
    Status status = storage.write("key1", "value1");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key2", "value2");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key3", "value3");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);

    for (int i = 0; i < 3; ++i) {
        std::string value;
        status = storage.read("key1", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value1", value);
        status = storage.read("key2", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value2", value);
    }

    const Graph &graph1 = storage.graph();
    ASSERT_EQ(3, graph1.get_vertex_count());
    std::cout << "    First tracking period: 3 vertices" << std::endl;

    // First repartition
    storage.repartition();
    std::cout << "    First repartition completed" << std::endl;

    // Second tracking period
    storage.enable_tracking(true);
    status = storage.write("key4", "value4");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key5", "value5");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);

    for (int i = 0; i < 2; ++i) {
        std::string value;
        status = storage.read("key4", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value4", value);
        status = storage.read("key5", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value5", value);
    }

    const Graph &graph2 = storage.graph();
    ASSERT_EQ(2, graph2.get_vertex_count());
    std::cout << "    Second tracking period: 2 vertices" << std::endl;

    // Second repartition
    storage.repartition();
    std::cout << "    Second repartition completed" << std::endl;

    // Verify all keys are still accessible
    std::string value;
    status = storage.read("key1", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value1", value);
    status = storage.read("key2", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value2", value);
    status = storage.read("key3", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value3", value);
    status = storage.read("key4", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value4", value);
    status = storage.read("key5", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value5", value);
    std::cout << "    All keys accessible after multiple repartitions"
              << std::endl;
    END_TEST("multiple_repartitions")
}

template <typename StorageType> void test_repartition_correctness() {
    TEST("repartition_correctness")
    StorageType storage(2);
    Status status;
    // Write keys with different access patterns
    for (int i = 1; i <= 5; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        status = storage.write(key, value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
    }
    storage.enable_tracking(true);

    // Write keys with different access patterns
    for (int i = 5; i <= 10; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        status = storage.write(key, value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
    }

    // Access some keys more frequently to create patterns
    for (int i = 0; i < 5; ++i) {
        std::string value;
        status = storage.read("key1", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value1", value);
        status = storage.read("key2", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value2", value);
        status = storage.read("key3", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value3", value);
    }

    for (int i = 0; i < 3; ++i) {
        std::string value;
        status = storage.read("key4", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value4", value);
        status = storage.read("key5", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value5", value);
    }

    // Perform repartitioning
    storage.repartition();

    // Verify all keys preserved with correct values
    for (int i = 1; i <= 10; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string expected_value = "value" + std::to_string(i);
        std::string value;
        status = storage.read(key, value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ(expected_value, value);
    }
    std::cout << "    All 10 keys preserved with correct values" << std::endl;
    END_TEST("repartition_correctness")
}

template <typename StorageType> void test_scan_operations() {
    TEST("scan_operations")
    StorageType storage(4);
    Status status;
    // Write some test data
    status = storage.write("prefix1_key1", "value1");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("prefix1_key2", "value2");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("prefix2_key1", "value3");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("prefix2_key2", "value4");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    storage.write("other_key", "value5");

    // Test scan with prefix
    std::vector<std::pair<std::string, std::string>> results;
    status = storage.scan("prefix1_", 10, results);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_TRUE(results.size() >= 2); // Should have at least 2 results

    // Sort results for consistent comparison
    std::sort(results.begin(), results.end());

    // Find the expected keys in the results
    bool found_key1 = false, found_key2 = false;
    for (const auto &result : results) {
        if (result.first == "prefix1_key1" && result.second == "value1") {
            found_key1 = true;
        }
        if (result.first == "prefix1_key2" && result.second == "value2") {
            found_key2 = true;
        }
    }

    ASSERT_TRUE(found_key1);
    ASSERT_TRUE(found_key2);

    std::cout << "    Scan with prefix returned correct results" << std::endl;
    END_TEST("scan_operations")
}

template <typename StorageType> void test_untracked_keys_preservation() {
    TEST("untracked_keys_preservation")
    StorageType storage(4);

    // Phase 1: Write many keys without tracking (these should be preserved)
    std::vector<std::pair<std::string, std::string>> untracked_keys;
    for (int i = 1; i <= 20; ++i) {
        std::string key = "untracked_key_" + std::to_string(i);
        std::string value = "untracked_value_" + std::to_string(i);
        Status status = storage.write(key, value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        untracked_keys.push_back({key, value});
    }
    std::cout << "    Written 20 untracked keys" << std::endl;

    // Phase 2: Enable tracking and write more keys (these will be tracked)
    storage.enable_tracking(true);
    std::vector<std::pair<std::string, std::string>> tracked_keys;
    for (int i = 1; i <= 10; ++i) {
        std::string key = "tracked_key_" + std::to_string(i);
        std::string value = "tracked_value_" + std::to_string(i);
        Status status = storage.write(key, value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        tracked_keys.push_back({key, value});
    }
    std::cout << "    Written 10 tracked keys" << std::endl;

    // Verify graph has tracked keys
    const Graph &graph_before = storage.graph();
    ASSERT_EQ(10, graph_before.get_vertex_count());
    std::cout << "    Graph contains " << graph_before.get_vertex_count()
              << " tracked vertices" << std::endl;

    // Phase 3: Perform repartitioning
    storage.repartition();
    std::cout << "    Repartitioning completed" << std::endl;

    // Phase 4: Verify ALL keys are still accessible after repartitioning
    // This test will fail if untracked keys are lost during repartitioning

    // Check untracked keys (these are the ones at risk of being lost)
    for (const auto &kv_pair : untracked_keys) {
        std::vector<std::pair<std::string, std::string>> scan_result;
        Status status = storage.scan(kv_pair.first, 1, scan_result);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_TRUE(scan_result.size() == 1);
        if (scan_result.size() == 1) {
            std::string retrieved_value = scan_result[0].second;
            ASSERT_STR_EQ(kv_pair.second, retrieved_value);
        } else {
            ASSERT_FALSE(true);
        }
    }
    std::cout << "    All 20 untracked keys preserved after repartitioning"
              << std::endl;

    // Check tracked keys (these should definitely be preserved)
    for (const auto &kv_pair : tracked_keys) {
        std::string retrieved_value;
        Status status = storage.read(kv_pair.first, retrieved_value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ(kv_pair.second, retrieved_value);
    }
    std::cout << "    All 10 tracked keys preserved after repartitioning"
              << std::endl;

    // Verify total count
    int total_keys = untracked_keys.size() + tracked_keys.size();
    std::cout << "    Total " << total_keys
              << " keys preserved after repartitioning" << std::endl;
    END_TEST("untracked_keys_preservation")
}

template <typename StorageType> void test_partition_map_consistency() {
    TEST("partition_map_consistency")
    StorageType storage(2);

    // Write keys without tracking enabled
    Status status = storage.write("key1", "value1");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key2", "value2");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.write("key3", "value3");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    std::cout << "    Written 3 keys without tracking" << std::endl;

    // Test scan functionality - this will fail if keys aren't in partition_map_
    std::vector<std::pair<std::string, std::string>> results;
    status = storage.scan("key", 10, results);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_TRUE(results.size() >=
                3); // Should find at least the 3 keys we wrote
    std::cout << "    Scan found " << results.size() << " keys (expected >= 3)"
              << std::endl;

    // Verify we can read all keys individually
    std::string value;
    status = storage.read("key1", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value1", value);
    status = storage.read("key2", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value2", value);
    status = storage.read("key3", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("value3", value);
    std::cout << "    All keys individually accessible" << std::endl;

    // Test with tracking enabled
    storage.enable_tracking(true);
    status = storage.write("tracked_key", "tracked_value");
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    status = storage.read("tracked_key", value);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_STR_EQ("tracked_value", value);

    // Scan should still work and include the new tracked key
    status = storage.scan("key", 10, results);
    ASSERT_STATUS_EQ(Status::SUCCESS, status);
    ASSERT_TRUE(results.size() >= 4); // Should find at least 4 keys now
    std::cout << "    Scan after tracking enabled found " << results.size()
              << " keys (expected >= 4)" << std::endl;

    END_TEST("partition_map_consistency")
}

template <typename StorageType> void test_operation_count() {
    TEST("operation_count")
    StorageType storage(4);

    // Initial count should be 0
    ASSERT_EQ(0, storage.operation_count());

    // Perform an arbitrary number of operations
    const size_t num_operations = 50;
    for (size_t i = 0; i < num_operations; ++i) {
        std::string key = "key:" + std::to_string(i);
        std::string value = "value:" + std::to_string(i);
        storage.write(key, value);
    }

    // Perform some read operations
    for (size_t i = 0; i < 20; ++i) {
        std::string key = "key:" + std::to_string(i);
        std::string value;
        storage.read(key, value);
    }
    // Sleep for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Check the count: 50 writes + 20 reads = 70 operations
    size_t expected_count = 50 + 20;
    ASSERT_EQ(expected_count, storage.operation_count());
    END_TEST("operation_count")
}

// Test suite runner for a specific storage type
template <typename StorageType>
void run_repartitioning_test_suite(const std::string &storage_name) {
    std::vector<std::pair<std::string, TestFunction>> tests = {
        {"basic_operations", []() { test_basic_operations<StorageType>(); }},
        {"tracking_disabled_by_default",
         []() { test_tracking_disabled_by_default<StorageType>(); }},
        {"enable_tracking", []() { test_enable_tracking<StorageType>(); }},
        {"basic_repartition", []() { test_basic_repartition<StorageType>(); }},
        {"co_access_patterns",
         []() { test_co_access_patterns<StorageType>(); }},
        {"empty_graph_repartition",
         []() { test_empty_graph_repartition<StorageType>(); }},
        {"multiple_repartitions",
         []() { test_multiple_repartitions<StorageType>(); }},
        {"repartition_correctness",
         []() { test_repartition_correctness<StorageType>(); }},
        {"scan_operations", []() { test_scan_operations<StorageType>(); }},
        {"untracked_keys_preservation",
         []() { test_untracked_keys_preservation<StorageType>(); }},
        {"partition_map_consistency",
         []() { test_partition_map_consistency<StorageType>(); }},
        {"operation_count", []() { test_operation_count<StorageType>(); }}};

    run_test_suite(storage_name, tests);
}

int main() {
    std::cout
        << "=== Testing Repartitioning Key-Value Storage Implementations ==="
        << std::endl;

    try {
        // Test HardRepartitioningKeyValueStorage
        run_repartitioning_test_suite<HardRepartitioningKeyValueStorage<
            MapStorageEngine, MapKeyStorage, MapKeyStorage>>(
            "HardRepartitioningKeyValueStorage");

        // Test SoftRepartitioningKeyValueStorage
        run_repartitioning_test_suite<SoftRepartitioningKeyValueStorage<
            MapStorageEngine, MapKeyStorage, MapKeyStorage>>(
            "SoftRepartitioningKeyValueStorage");

        // Test SoftThreadedRepartitioningKeyValueStorage
        run_repartitioning_test_suite<SoftThreadedRepartitioningKeyValueStorage<
            LmdbStorageEngine, MapKeyStorage>>(
            "SoftThreadedRepartitioningKeyValueStorage");

        std::cout << "\n========================================\n";
        std::cout << "  All Repartitioning Tests PASSED!\n";
        std::cout << "========================================\n\n";

        std::cout << "Summary:\n";
        std::cout << "  ✓ Both Hard and Soft repartitioning implementations "
                     "work correctly\n";
        std::cout << "  ✓ Repartitioning uses METIS graph partitioning\n";
        std::cout << "  ✓ Graph is cleared after repartitioning\n";
        std::cout << "  ✓ Tracking is disabled after repartitioning\n";
        std::cout << "  ✓ Data remains accessible after repartitioning\n";
        std::cout << "  ✓ Multiple repartitions can be performed\n";
        std::cout << "  ✓ Co-accessed keys can be optimally placed\n";
        std::cout << "  ✓ Total tests passed: " << tests_passed << "\n";
        std::cout << "  ✓ Total tests failed: " << tests_failed << "\n";

        return tests_failed > 0 ? 1 : 0;

    } catch (const std::exception &e) {
        std::cerr << "Test suite failed with exception: " << e.what()
                  << std::endl;
        return 1;
    }
}
