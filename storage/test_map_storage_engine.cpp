#include "MapStorageEngine.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) \
    std::cout << "Running test: " << name << "..." << std::endl; \
    try {

#define END_TEST(name) \
        std::cout << "  ✓ " << name << " PASSED" << std::endl; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "  ✗ " << name << " FAILED: " << e.what() << std::endl; \
        tests_failed++; \
    } catch (...) { \
        std::cout << "  ✗ " << name << " FAILED: Unknown exception" << std::endl; \
        tests_failed++; \
    }

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        throw std::runtime_error(std::string("Expected '") + std::to_string(expected) + \
                                "' but got '" + std::to_string(actual) + "'"); \
    }

#define ASSERT_STR_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        throw std::runtime_error(std::string("Expected '") + (expected) + \
                                "' but got '" + (actual) + "'"); \
    }

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error("Condition failed: " #condition); \
    }

#define ASSERT_FALSE(condition) \
    if (condition) { \
        throw std::runtime_error("Condition should be false: " #condition); \
    }

void test_basic_write_read() {
    TEST("basic_write_read")
        MapStorageEngine engine;
        
        // Write some values
        engine.write("key1", "value1");
        engine.write("key2", "value2");
        engine.write("key3", "value3");
        
        // Read them back
        ASSERT_STR_EQ("value1", engine.read("key1"));
        ASSERT_STR_EQ("value2", engine.read("key2"));
        ASSERT_STR_EQ("value3", engine.read("key3"));
    END_TEST("basic_write_read")
}

void test_read_nonexistent_key() {
    TEST("read_nonexistent_key")
        MapStorageEngine engine;
        
        // Reading a non-existent key should return empty string
        ASSERT_STR_EQ("", engine.read("nonexistent"));
    END_TEST("read_nonexistent_key")
}

void test_overwrite_value() {
    TEST("overwrite_value")
        MapStorageEngine engine;
        
        engine.write("key", "original");
        ASSERT_STR_EQ("original", engine.read("key"));
        
        engine.write("key", "updated");
        ASSERT_STR_EQ("updated", engine.read("key"));
    END_TEST("overwrite_value")
}

void test_empty_key_value() {
    TEST("empty_key_value")
        MapStorageEngine engine;
        
        // Test empty key
        engine.write("", "empty_key_value");
        ASSERT_STR_EQ("empty_key_value", engine.read(""));
        
        // Test empty value
        engine.write("empty_value_key", "");
        ASSERT_STR_EQ("", engine.read("empty_value_key"));
    END_TEST("empty_key_value")
}

void test_scan_basic() {
    TEST("scan_basic")
        MapStorageEngine engine;
        
        engine.write("user:1001", "Alice");
        engine.write("user:1002", "Bob");
        engine.write("user:1003", "Charlie");
        engine.write("product:2001", "Laptop");
        
        auto results = engine.scan("user:", 10);
        ASSERT_EQ(3, results.size());
        ASSERT_STR_EQ("user:1001", results[0]);
        ASSERT_STR_EQ("user:1002", results[1]);
        ASSERT_STR_EQ("user:1003", results[2]);
    END_TEST("scan_basic")
}

void test_scan_with_limit() {
    TEST("scan_with_limit")
        MapStorageEngine engine;
        
        engine.write("item:001", "A");
        engine.write("item:002", "B");
        engine.write("item:003", "C");
        engine.write("item:004", "D");
        engine.write("item:005", "E");
        
        auto results = engine.scan("item:", 3);
        ASSERT_EQ(3, results.size());
        ASSERT_STR_EQ("item:001", results[0]);
        ASSERT_STR_EQ("item:002", results[1]);
        ASSERT_STR_EQ("item:003", results[2]);
    END_TEST("scan_with_limit")
}

void test_scan_no_matches() {
    TEST("scan_no_matches")
        MapStorageEngine engine;
        
        engine.write("apple", "fruit");
        engine.write("banana", "fruit");
        
        auto results = engine.scan("orange", 10);
        ASSERT_EQ(0, results.size());
    END_TEST("scan_no_matches")
}

void test_scan_empty_prefix() {
    TEST("scan_empty_prefix")
        MapStorageEngine engine;
        
        engine.write("a", "1");
        engine.write("b", "2");
        engine.write("c", "3");
        
        // Empty prefix should match all keys
        auto results = engine.scan("", 10);
        ASSERT_EQ(3, results.size());
    END_TEST("scan_empty_prefix")
}

void test_scan_exact_match() {
    TEST("scan_exact_match")
        MapStorageEngine engine;
        
        engine.write("exact", "value");
        engine.write("exactly", "value2");
        engine.write("exact_match", "value3");
        
        auto results = engine.scan("exact", 10);
        ASSERT_EQ(3, results.size());
        ASSERT_STR_EQ("exact", results[0]);
    END_TEST("scan_exact_match")
}

void test_scan_sorted_order() {
    TEST("scan_sorted_order")
        MapStorageEngine engine;
        
        // Insert in random order
        engine.write("z", "last");
        engine.write("a", "first");
        engine.write("m", "middle");
        
        auto results = engine.scan("", 10);
        ASSERT_EQ(3, results.size());
        ASSERT_STR_EQ("a", results[0]);
        ASSERT_STR_EQ("m", results[1]);
        ASSERT_STR_EQ("z", results[2]);
    END_TEST("scan_sorted_order")
}

void test_large_dataset() {
    TEST("large_dataset")
        MapStorageEngine engine;
        
        // Write 1000 entries
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key:" + std::to_string(i);
            std::string value = "value:" + std::to_string(i);
            engine.write(key, value);
        }
        
        // Read some back
        ASSERT_STR_EQ("value:0", engine.read("key:0"));
        ASSERT_STR_EQ("value:500", engine.read("key:500"));
        ASSERT_STR_EQ("value:999", engine.read("key:999"));
        
        // Scan with limit
        auto results = engine.scan("key:", 100);
        ASSERT_EQ(100, results.size());
    END_TEST("large_dataset")
}

void test_special_characters() {
    TEST("special_characters")
        MapStorageEngine engine;
        
        engine.write("key:with:colons", "value1");
        engine.write("key/with/slashes", "value2");
        engine.write("key-with-dashes", "value3");
        engine.write("key_with_underscores", "value4");
        engine.write("key.with.dots", "value5");
        
        ASSERT_STR_EQ("value1", engine.read("key:with:colons"));
        ASSERT_STR_EQ("value2", engine.read("key/with/slashes"));
        ASSERT_STR_EQ("value3", engine.read("key-with-dashes"));
        ASSERT_STR_EQ("value4", engine.read("key_with_underscores"));
        ASSERT_STR_EQ("value5", engine.read("key.with.dots"));
    END_TEST("special_characters")
}

void test_manual_locking() {
    TEST("manual_locking")
        MapStorageEngine engine;
        
        // Test exclusive lock
        engine.lock();
        engine.write("locked_key", "locked_value");
        engine.unlock();
        
        // Test shared lock
        engine.lock_shared();
        std::string value = engine.read("locked_key");
        engine.unlock_shared();
        
        ASSERT_STR_EQ("locked_value", value);
    END_TEST("manual_locking")
}

void test_concurrent_writes() {
    TEST("concurrent_writes")
        MapStorageEngine engine;
        
        const int num_threads = 4;
        const int writes_per_thread = 100;
        
        auto writer = [&engine](int thread_id) {
            for (int i = 0; i < writes_per_thread; ++i) {
                std::string key = "thread:" + std::to_string(thread_id) + ":key:" + std::to_string(i);
                std::string value = "value:" + std::to_string(i);
                
                engine.lock();
                engine.write(key, value);
                engine.unlock();
            }
        };
        
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(writer, i);
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Verify all writes succeeded
        for (int thread_id = 0; thread_id < num_threads; ++thread_id) {
            for (int i = 0; i < writes_per_thread; ++i) {
                std::string key = "thread:" + std::to_string(thread_id) + ":key:" + std::to_string(i);
                std::string expected_value = "value:" + std::to_string(i);
                
                engine.lock_shared();
                std::string actual_value = engine.read(key);
                engine.unlock_shared();
                
                ASSERT_STR_EQ(expected_value, actual_value);
            }
        }
    END_TEST("concurrent_writes")
}

void test_concurrent_reads_writes() {
    TEST("concurrent_reads_writes")
        MapStorageEngine engine;
        
        // Pre-populate with data
        for (int i = 0; i < 50; ++i) {
            engine.write("key:" + std::to_string(i), "value:" + std::to_string(i));
        }
        
        auto writer = [&engine]() {
            for (int i = 50; i < 100; ++i) {
                engine.lock();
                engine.write("key:" + std::to_string(i), "value:" + std::to_string(i));
                engine.unlock();
            }
        };
        
        auto reader = [&engine]() {
            for (int i = 0; i < 50; ++i) {
                engine.lock_shared();
                std::string value = engine.read("key:" + std::to_string(i));
                engine.unlock_shared();
                // Just ensure we can read without crashes
            }
        };
        
        std::thread t1(writer);
        std::thread t2(reader);
        std::thread t3(reader);
        
        t1.join();
        t2.join();
        t3.join();
        
        // Verify final state
        engine.lock_shared();
        auto results = engine.scan("key:", 100);
        engine.unlock_shared();
        
        ASSERT_EQ(100, results.size());
    END_TEST("concurrent_reads_writes")
}

void test_scan_after_updates() {
    TEST("scan_after_updates")
        MapStorageEngine engine;
        
        engine.write("prefix:a", "1");
        engine.write("prefix:b", "2");
        engine.write("prefix:c", "3");
        
        auto results1 = engine.scan("prefix:", 10);
        ASSERT_EQ(3, results1.size());
        
        // Add more keys
        engine.write("prefix:d", "4");
        engine.write("prefix:e", "5");
        
        auto results2 = engine.scan("prefix:", 10);
        ASSERT_EQ(5, results2.size());
        
        // Update existing key
        engine.write("prefix:a", "updated");
        ASSERT_STR_EQ("updated", engine.read("prefix:a"));
        
        // Scan should still return same number
        auto results3 = engine.scan("prefix:", 10);
        ASSERT_EQ(5, results3.size());
    END_TEST("scan_after_updates")
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  MapStorageEngine Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Basic functionality tests
    test_basic_write_read();
    test_read_nonexistent_key();
    test_overwrite_value();
    test_empty_key_value();
    
    // Scan tests
    test_scan_basic();
    test_scan_with_limit();
    test_scan_no_matches();
    test_scan_empty_prefix();
    test_scan_exact_match();
    test_scan_sorted_order();
    test_scan_after_updates();
    
    // Edge cases and special scenarios
    test_large_dataset();
    test_special_characters();
    
    // Locking and concurrency tests
    test_manual_locking();
    test_concurrent_writes();
    test_concurrent_reads_writes();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << tests_failed << std::endl;
    std::cout << "Total tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "✓ All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}

