#include "HardRepartitioningKeyValueStorage.h"
#include "SoftRepartitioningKeyValueStorage.h"
#include "../storage/MapStorageEngine.h"
#include "../keystorage/MapKeyStorage.h"
#include "../utils/test_assertions.h"
#include <iostream>
#include <vector>
#include <string>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

// Generic test functions that work with any PartitionedKeyValueStorage implementation
template<typename StorageType>
void test_basic_write_read() {
    TEST("basic_write_read")
        StorageType storage(4);  // Create with 4 partitions
        
        // Write some values
        Status status = storage.write("key1", "value1");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("key2", "value2");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("key3", "value3");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        
        // Read them back
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
    END_TEST("basic_write_read")
}

template<typename StorageType>
void test_read_nonexistent_key() {
    TEST("read_nonexistent_key")
        StorageType storage(4);
        
        // Reading a non-existent key should return empty string
        std::string value;
        Status status = storage.read("nonexistent", value);
        ASSERT_STATUS_EQ(Status::NOT_FOUND, status);
    END_TEST("read_nonexistent_key")
}

template<typename StorageType>
void test_overwrite_value() {
    TEST("overwrite_value")
        StorageType storage(4);
        std::string value;
        
        Status status = storage.write("key", "original");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.read("key", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("original", value);
        
        status = storage.write("key", "updated");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.read("key", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("updated", value);
    END_TEST("overwrite_value")
}

template<typename StorageType>
void test_empty_key_value() {
    TEST("empty_key_value")
        StorageType storage(4);
        std::string value;
        // Test empty key
        Status status = storage.write("", "empty_key_value");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.read("", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("empty_key_value", value);
        
        // Test empty value
        status = storage.write("empty_value_key", "");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.read("empty_value_key", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("", value);
    END_TEST("empty_key_value")
}

template<typename StorageType>
void test_multiple_partitions() {
    TEST("multiple_partitions")
        StorageType storage(4);
        
        // Write keys that should hash to different partitions
        for (int i = 0; i < 20; ++i) {
            std::string key = "key:" + std::to_string(i);
            std::string value = "value:" + std::to_string(i);
            Status status = storage.write(key, value);
            ASSERT_STATUS_EQ(Status::SUCCESS, status);
        }
        
        // Read them all back
        for (int i = 0; i < 20; ++i) {
            std::string key = "key:" + std::to_string(i);
            std::string expected_value = "value:" + std::to_string(i);
            std::string value;
            Status status = storage.read(key, value);
            ASSERT_STATUS_EQ(Status::SUCCESS, status);
            ASSERT_STR_EQ(expected_value, value);
        }
    END_TEST("multiple_partitions")
}

template<typename StorageType>
void test_single_partition() {
    TEST("single_partition")
        StorageType storage(1);  // Only one partition
        
        Status status = storage.write("key1", "value1");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("key2", "value2");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("key3", "value3");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        
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
    END_TEST("single_partition")
}

template<typename StorageType>
void test_many_partitions() {
    TEST("many_partitions")
        StorageType storage(16);  // Many partitions
        
        for (int i = 0; i < 100; ++i) {
            std::string key = "item:" + std::to_string(i);
            std::string value = "data:" + std::to_string(i);
            Status status = storage.write(key, value);
            ASSERT_STATUS_EQ(Status::SUCCESS, status);
        }
        
        for (int i = 0; i < 100; ++i) {
            std::string key = "item:" + std::to_string(i);
            std::string expected_value = "data:" + std::to_string(i);
            std::string value;
            Status status = storage.read(key, value);
            ASSERT_STATUS_EQ(Status::SUCCESS, status);
            ASSERT_STR_EQ(expected_value, value);
        }
    END_TEST("many_partitions")
}

template<typename StorageType>
void test_large_dataset() {
    TEST("large_dataset")
        StorageType storage(8);
        
        // Write 1000 entries
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key:" + std::to_string(i);
            std::string value = "value:" + std::to_string(i);
            Status status = storage.write(key, value);
            ASSERT_STATUS_EQ(Status::SUCCESS, status);
        }
        
        // Read some back
        std::string value;
        Status status = storage.read("key:0", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value:0", value);
        status = storage.read("key:500", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value:500", value);
        status = storage.read("key:999", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value:999", value);
        
    END_TEST("large_dataset")
}

template<typename StorageType>
void test_special_characters() {
    TEST("special_characters")
        StorageType storage(4);
        
        storage.write("key:with:colons", "value1");
        Status status = storage.write("key/with/slashes", "value2");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("key-with-dashes", "value3");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("key_with_underscores", "value4");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("key.with.dots", "value5");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        
        std::string value;
        status = storage.read("key:with:colons", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value1", value);
        status = storage.read("key/with/slashes", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value2", value);
        status = storage.read("key-with-dashes", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value3", value);
        status = storage.read("key_with_underscores", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value4", value);
        status = storage.read("key.with.dots", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("value5", value);
    END_TEST("special_characters")
}

template<typename StorageType>
void test_repeated_operations() {
    TEST("repeated_operations")
        StorageType storage(4);
        std::string value;
        // Write, read, overwrite, read again
        Status status = storage.write("test_key", "initial");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.read("test_key", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("initial", value);
        
        status = storage.write("test_key", "second");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.read("test_key", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("second", value);
        
        status = storage.write("test_key", "third");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.read("test_key", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("third", value);
    END_TEST("repeated_operations")
}

template<typename StorageType>
void test_scan_basic() {
    TEST("scan_basic")
        StorageType storage(4);
        
        storage.write("user:1001", "Alice");
        Status status = storage.write("user:1002", "Bob");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("user:1003", "Charlie");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("product:2001", "Laptop");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        
        std::vector<std::pair<std::string, std::string>> results;
        status = storage.scan("user:", 10, results);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_EQ(3, results.size());
        
        // Results should be sorted
        ASSERT_STR_EQ("user:1001", results[0].first);
        ASSERT_STR_EQ("Alice", results[0].second);
        ASSERT_STR_EQ("user:1002", results[1].first);
        ASSERT_STR_EQ("Bob", results[1].second);
        ASSERT_STR_EQ("user:1003", results[2].first);
        ASSERT_STR_EQ("Charlie", results[2].second);
    END_TEST("scan_basic")
}

template<typename StorageType>
void test_scan_with_limit() {
    TEST("scan_with_limit")
        StorageType storage(4);
        
        storage.write("item:001", "A");
        Status status = storage.write("item:002", "B");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("item:003", "C");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("item:004", "D");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("item:005", "E");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        
        std::vector<std::pair<std::string, std::string>> results;
        status = storage.scan("item:", 3, results);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_EQ(3, results.size());
        ASSERT_STR_EQ("item:001", results[0].first);
        ASSERT_STR_EQ("A", results[0].second);
        ASSERT_STR_EQ("item:002", results[1].first);
        ASSERT_STR_EQ("B", results[1].second);
        ASSERT_STR_EQ("item:003", results[2].first);
        ASSERT_STR_EQ("C", results[2].second);
    END_TEST("scan_with_limit")
}

template<typename StorageType>
void test_scan_no_matches() {
    TEST("scan_no_matches")
        StorageType storage(4);
        
        Status status = storage.write("apple", "fruit");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("banana", "fruit");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        
        std::vector<std::pair<std::string, std::string>> results;
        status = storage.scan("orange", 10, results);
        ASSERT_STATUS_EQ(Status::NOT_FOUND, status);
    END_TEST("scan_no_matches")
}

template<typename StorageType>
void test_scan_empty_prefix() {
    TEST("scan_empty_prefix")
        StorageType storage(4);

        Status status = storage.write("a", "1");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("b", "2");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("c", "3");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        
        // Empty prefix should match all keys
        std::vector<std::pair<std::string, std::string>> results;
        status = storage.scan("", 10, results);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_EQ(3, results.size());
    END_TEST("scan_empty_prefix")
}

template<typename StorageType>
void test_mixed_operations() {
    TEST("mixed_operations")
        StorageType storage(4);
        
        // Mix of writes, reads, and overwrites
        Status status = storage.write("a", "1");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("b", "2");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        std::string value;
        status = storage.read("a", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("1", value);
        
        status = storage.write("c", "3");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.write("a", "1_updated");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        status = storage.read("a", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("1_updated", value);
        status = storage.read("b", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("2", value);
        status = storage.read("c", value);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_STR_EQ("3", value);
        
        status = storage.write("d", "4");
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        std::vector<std::pair<std::string, std::string>> results;
        status = storage.scan("", 10, results);
        ASSERT_STATUS_EQ(Status::SUCCESS, status);
        ASSERT_EQ(4, results.size());
        ASSERT_STR_EQ("a", results[0].first);
        ASSERT_STR_EQ("1_updated", results[0].second);
        ASSERT_STR_EQ("b", results[1].first);
        ASSERT_STR_EQ("2", results[1].second);
        ASSERT_STR_EQ("c", results[2].first);
        ASSERT_STR_EQ("3", results[2].second);
        ASSERT_STR_EQ("d", results[3].first);
        ASSERT_STR_EQ("4", results[3].second);
    END_TEST("mixed_operations")
}

// Helper function to run all tests for a given storage type
template<typename StorageType>
void run_test_suite(const std::string& storage_name) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Testing: " << storage_name << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Basic functionality tests
    test_basic_write_read<StorageType>();
    test_read_nonexistent_key<StorageType>();
    test_overwrite_value<StorageType>();
    test_empty_key_value<StorageType>();
    
    // Partition-specific tests
    test_single_partition<StorageType>();
    test_multiple_partitions<StorageType>();
    test_many_partitions<StorageType>();
    
    // Scan tests
    test_scan_basic<StorageType>();
    test_scan_with_limit<StorageType>();
    test_scan_no_matches<StorageType>();
    test_scan_empty_prefix<StorageType>();
    
    // Edge cases and special scenarios
    test_large_dataset<StorageType>();
    test_special_characters<StorageType>();
    test_repeated_operations<StorageType>();
    test_mixed_operations<StorageType>();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Generic PartitionedKeyValueStorage Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Test RepartitioningKeyValueStorage with MapStorageEngine
    using TestStorage = SoftRepartitioningKeyValueStorage<
        MapStorageEngine,
        MapKeyStorage,
        MapKeyStorage
    >;
    
    run_test_suite<TestStorage>("RepartitioningKeyValueStorage<MapStorageEngine>");
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Overall Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << tests_failed << std::endl;
    std::cout << "Total tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "✓ All tests passed for partitioned key-value storage!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}

