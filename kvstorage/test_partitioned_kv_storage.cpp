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
        storage.write("key1", "value1");
        storage.write("key2", "value2");
        storage.write("key3", "value3");
        
        // Read them back
        ASSERT_STR_EQ("value1", storage.read("key1"));
        ASSERT_STR_EQ("value2", storage.read("key2"));
        ASSERT_STR_EQ("value3", storage.read("key3"));
    END_TEST("basic_write_read")
}

template<typename StorageType>
void test_read_nonexistent_key() {
    TEST("read_nonexistent_key")
        StorageType storage(4);
        
        // Reading a non-existent key should return empty string
        ASSERT_STR_EQ("", storage.read("nonexistent"));
    END_TEST("read_nonexistent_key")
}

template<typename StorageType>
void test_overwrite_value() {
    TEST("overwrite_value")
        StorageType storage(4);
        
        storage.write("key", "original");
        ASSERT_STR_EQ("original", storage.read("key"));
        
        storage.write("key", "updated");
        ASSERT_STR_EQ("updated", storage.read("key"));
    END_TEST("overwrite_value")
}

template<typename StorageType>
void test_empty_key_value() {
    TEST("empty_key_value")
        StorageType storage(4);
        
        // Test empty key
        storage.write("", "empty_key_value");
        ASSERT_STR_EQ("empty_key_value", storage.read(""));
        
        // Test empty value
        storage.write("empty_value_key", "");
        ASSERT_STR_EQ("", storage.read("empty_value_key"));
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
            storage.write(key, value);
        }
        
        // Read them all back
        for (int i = 0; i < 20; ++i) {
            std::string key = "key:" + std::to_string(i);
            std::string expected_value = "value:" + std::to_string(i);
            ASSERT_STR_EQ(expected_value, storage.read(key));
        }
    END_TEST("multiple_partitions")
}

template<typename StorageType>
void test_single_partition() {
    TEST("single_partition")
        StorageType storage(1);  // Only one partition
        
        storage.write("key1", "value1");
        storage.write("key2", "value2");
        storage.write("key3", "value3");
        
        ASSERT_STR_EQ("value1", storage.read("key1"));
        ASSERT_STR_EQ("value2", storage.read("key2"));
        ASSERT_STR_EQ("value3", storage.read("key3"));
    END_TEST("single_partition")
}

template<typename StorageType>
void test_many_partitions() {
    TEST("many_partitions")
        StorageType storage(16);  // Many partitions
        
        for (int i = 0; i < 100; ++i) {
            std::string key = "item:" + std::to_string(i);
            std::string value = "data:" + std::to_string(i);
            storage.write(key, value);
        }
        
        for (int i = 0; i < 100; ++i) {
            std::string key = "item:" + std::to_string(i);
            std::string expected_value = "data:" + std::to_string(i);
            ASSERT_STR_EQ(expected_value, storage.read(key));
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
            storage.write(key, value);
        }
        
        // Read some back
        ASSERT_STR_EQ("value:0", storage.read("key:0"));
        ASSERT_STR_EQ("value:500", storage.read("key:500"));
        ASSERT_STR_EQ("value:999", storage.read("key:999"));
    END_TEST("large_dataset")
}

template<typename StorageType>
void test_special_characters() {
    TEST("special_characters")
        StorageType storage(4);
        
        storage.write("key:with:colons", "value1");
        storage.write("key/with/slashes", "value2");
        storage.write("key-with-dashes", "value3");
        storage.write("key_with_underscores", "value4");
        storage.write("key.with.dots", "value5");
        
        ASSERT_STR_EQ("value1", storage.read("key:with:colons"));
        ASSERT_STR_EQ("value2", storage.read("key/with/slashes"));
        ASSERT_STR_EQ("value3", storage.read("key-with-dashes"));
        ASSERT_STR_EQ("value4", storage.read("key_with_underscores"));
        ASSERT_STR_EQ("value5", storage.read("key.with.dots"));
    END_TEST("special_characters")
}

template<typename StorageType>
void test_repeated_operations() {
    TEST("repeated_operations")
        StorageType storage(4);
        
        // Write, read, overwrite, read again
        storage.write("test_key", "initial");
        ASSERT_STR_EQ("initial", storage.read("test_key"));
        
        storage.write("test_key", "second");
        ASSERT_STR_EQ("second", storage.read("test_key"));
        
        storage.write("test_key", "third");
        ASSERT_STR_EQ("third", storage.read("test_key"));
    END_TEST("repeated_operations")
}

template<typename StorageType>
void test_scan_basic() {
    TEST("scan_basic")
        StorageType storage(4);
        
        storage.write("user:1001", "Alice");
        storage.write("user:1002", "Bob");
        storage.write("user:1003", "Charlie");
        storage.write("product:2001", "Laptop");
        
        auto results = storage.scan("user:", 10);
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
        storage.write("item:002", "B");
        storage.write("item:003", "C");
        storage.write("item:004", "D");
        storage.write("item:005", "E");
        
        auto results = storage.scan("item:", 3);
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
        
        storage.write("apple", "fruit");
        storage.write("banana", "fruit");
        
        auto results = storage.scan("orange", 10);
        ASSERT_EQ(0, results.size());
    END_TEST("scan_no_matches")
}

template<typename StorageType>
void test_scan_empty_prefix() {
    TEST("scan_empty_prefix")
        StorageType storage(4);
        
        storage.write("a", "1");
        storage.write("b", "2");
        storage.write("c", "3");
        
        // Empty prefix should match all keys
        auto results = storage.scan("", 10);
        ASSERT_EQ(3, results.size());
    END_TEST("scan_empty_prefix")
}

template<typename StorageType>
void test_mixed_operations() {
    TEST("mixed_operations")
        StorageType storage(4);
        
        // Mix of writes, reads, and overwrites
        storage.write("a", "1");
        storage.write("b", "2");
        ASSERT_STR_EQ("1", storage.read("a"));
        
        storage.write("c", "3");
        storage.write("a", "1_updated");
        ASSERT_STR_EQ("1_updated", storage.read("a"));
        ASSERT_STR_EQ("2", storage.read("b"));
        ASSERT_STR_EQ("3", storage.read("c"));
        
        storage.write("d", "4");
        auto results = storage.scan("", 10);
        ASSERT_EQ(4, results.size());
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

