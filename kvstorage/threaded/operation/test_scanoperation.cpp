#include "ScanOperation.h"
#include "../../../storage/MapStorageEngine.h"
#include "../../../utils/test_assertions.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;


void test_scan_operation_constructor() {
    TEST("scan_operation_constructor")
        std::string key = "test_key";
        std::vector<std::pair<std::string, std::string>> results(5);
        
        ScanOperation<MapStorageEngine> scan_op(key, results, 3);  // 3 partitions
        
        // Test that it inherits from Operation correctly
        ASSERT_TRUE(scan_op.type() == Type::SCAN);
        ASSERT_STR_EQ("test_key", scan_op.key());
        
        // Test values access
        ASSERT_EQ(5, scan_op.values().size());
    END_TEST("scan_operation_constructor")
}

void test_scan_operation_barrier() {
    TEST("scan_operation_barrier")
        // Create storage engine and populate with key-value pairs
        MapStorageEngine storage(0);
        storage.write("key1", "value1");
        storage.write("key2", "value2");
        storage.write("key3", "value3");
        storage.write("key4", "value4");
        storage.write("key5", "value5");
        
        // Create scan operation with single key and results vector
        std::string scan_key = "key2";  // Start scanning from key2
        std::vector<std::pair<std::string, std::string>> results;
        results.resize(3);  // Limit scan to 3 results
        
        ScanOperation<MapStorageEngine> scan_op(scan_key, results, 2);  // 2 partitions
        
        // Test that operation is properly constructed
        ASSERT_TRUE(scan_op.type() == Type::SCAN);
        ASSERT_TRUE(scan_op.key() == scan_key);
        ASSERT_TRUE(&scan_op.key() == &scan_key);
        ASSERT_STR_EQ("key2", scan_op.key());

        std::thread t1([&]() {
            // First wait - determine if this is the coordinator
            bool is_coord = scan_op.is_coordinator();

            if (is_coord) {
                // This thread is the coordinator                
                // Perform range scan starting from scan.key(), limited to scan.values().size()
                size_t limit = scan_op.values().size();
                auto results = storage.scan(scan_op.key(), limit);
                for (size_t i = 0; i < limit; i++) {
                    scan_op.values()[i] = results[i];
                }
            }
            // Second wait - all threads synchronize again
            if (scan_op.is_coordinator()){
                // Only one thread notify the operation
                scan_op.notify();
            }
        });
        
        std::thread t2([&]() {
            // First wait - determine if this is the coordinator

            bool is_coord = scan_op.is_coordinator();

            if (is_coord) {
                // This thread is the coordinator                
                // Perform range scan starting from scan.key(), limited to scan.values().size()
                size_t limit = scan_op.values().size();
                auto results = storage.scan(scan_op.key(), limit);
                for (size_t i = 0; i < limit; i++) {
                    scan_op.values()[i] = results[i];
                }
            }
            // Second wait - all threads synchronize again
            if (scan_op.is_coordinator()){
                // Only one thread notify the operation
                scan_op.notify();
            }
        });

        // Wait for the operation to complete
        scan_op.wait();

        // Check that scan results are populated correctly
        // Results should contain key2, key3, key4 based on our scan
        ASSERT_STR_EQ("key2", results[0].first);
        ASSERT_STR_EQ("value2", results[0].second);
        ASSERT_STR_EQ("key3", results[1].first);
        ASSERT_STR_EQ("value3", results[1].second);
        ASSERT_STR_EQ("key4", results[2].first);
        ASSERT_STR_EQ("value4", results[2].second);
        
        // Wait for both threads to complete
        t1.join();
        t2.join();
        
        std::cout << "  ✓ Scan operation completed successfully" << std::endl;
    END_TEST("scan_operation_single_key")
}

int main() {
    std::cout << "Starting ScanOperation class tests..." << std::endl << std::endl;
    
    test_scan_operation_constructor();
    test_scan_operation_barrier();
    
    std::cout << std::endl << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "All ScanOperation tests completed successfully!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
