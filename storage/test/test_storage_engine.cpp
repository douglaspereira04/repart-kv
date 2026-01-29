#include "../MapStorageEngine.h"
#include "../TkrzwHashStorageEngine.h"
#include "../TkrzwTreeStorageEngine.h"
#include "../LmdbStorageEngine.h"
#include "../TbbStorageEngine.h"
#include "../../utils/test_assertions.h"
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <memory>
#include <chrono>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

// Generic test functions that work with any StorageEngine
template <typename EngineType> void test_basic_write_read() {
    TEST("basic_write_read")
    EngineType engine;

    // Write some values
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("key1", "value1"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("key2", "value2"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("key3", "value3"));

    // Read them back
    std::string read_value;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("key1", read_value));
    ASSERT_STR_EQ("value1", read_value);
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("key2", read_value));
    ASSERT_STR_EQ("value2", read_value);
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("key3", read_value));
    ASSERT_STR_EQ("value3", read_value);
    END_TEST("basic_write_read")
}

template <typename EngineType> void test_read_nonexistent_key() {
    TEST("read_nonexistent_key")
    EngineType engine;
    std::string read_value;

    // Reading a non-existent key should return empty string
    ASSERT_STATUS_EQ(Status::NOT_FOUND, engine.read("nonexistent", read_value));
    END_TEST("read_nonexistent_key")
}

template <typename EngineType> void test_overwrite_value() {
    TEST("overwrite_value")
    EngineType engine;

    std::string read_value;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("key", "original"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("key", read_value));
    ASSERT_STR_EQ("original", read_value);

    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("key", "updated"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("key", read_value));
    ASSERT_STR_EQ("updated", read_value);
    END_TEST("overwrite_value")
}

template <typename EngineType> void test_scan_basic() {
    TEST("scan_basic")
    EngineType engine;

    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("user:1001", "Alice"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("user:1002", "Bob"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("user:1003", "Charlie"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("product:2001", "Laptop"));

    std::vector<std::pair<std::string, std::string>> results;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("user:", 3, results));
    ASSERT_EQ(3, results.size());
    ASSERT_STR_EQ("user:1001", results[0].first);
    ASSERT_STR_EQ("Alice", results[0].second);
    ASSERT_STR_EQ("user:1002", results[1].first);
    ASSERT_STR_EQ("Bob", results[1].second);
    ASSERT_STR_EQ("user:1003", results[2].first);
    ASSERT_STR_EQ("Charlie", results[2].second);
    END_TEST("scan_basic")
}

template <typename EngineType> void test_scan_with_limit() {
    TEST("scan_with_limit")
    EngineType engine;

    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("item:001", "A"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("item:002", "B"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("item:003", "C"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("item:004", "D"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("item:005", "E"));

    std::vector<std::pair<std::string, std::string>> results;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("item:", 3, results));
    ASSERT_EQ(3, results.size());
    ASSERT_STR_EQ("item:001", results[0].first);
    ASSERT_STR_EQ("A", results[0].second);
    ASSERT_STR_EQ("item:002", results[1].first);
    ASSERT_STR_EQ("B", results[1].second);
    ASSERT_STR_EQ("item:003", results[2].first);
    ASSERT_STR_EQ("C", results[2].second);
    END_TEST("scan_with_limit")
}

template <typename EngineType> void test_scan_empty_prefix() {
    TEST("scan_empty_prefix")
    EngineType engine;

    std::vector<std::pair<std::string, std::string>> results;
    ASSERT_STATUS_EQ(Status::NOT_FOUND, engine.scan("", 10, results));
    ASSERT_EQ(0, results.size());

    engine.write("key1", "value1");
    engine.write("key2", "value2");
    engine.write("key3", "value3");

    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("", 10, results));
    ASSERT_EQ(3, results.size());
    ASSERT_STR_EQ("key1", results[0].first);
    ASSERT_STR_EQ("value1", results[0].second);
    ASSERT_STR_EQ("key2", results[1].first);
    ASSERT_STR_EQ("value2", results[1].second);
    ASSERT_STR_EQ("key3", results[2].first);
    ASSERT_STR_EQ("value3", results[2].second);
    END_TEST("scan_empty_prefix")
}

template <typename EngineType> void test_scan_no_matches() {
    TEST("scan_no_matches")
    EngineType engine;

    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("apple", "fruit"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("banana", "fruit"));

    std::vector<std::pair<std::string, std::string>> results;
    ASSERT_STATUS_EQ(Status::NOT_FOUND, engine.scan("orange", 10, results));
    ASSERT_EQ(0, results.size());
    END_TEST("scan_no_matches")
}

template <typename EngineType> void test_scan_exact_match() {
    TEST("scan_exact_match")
    EngineType engine;

    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("exact", "value"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("exactly", "value2"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("exact_match", "value3"));

    std::vector<std::pair<std::string, std::string>> results;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("exact", 10, results));
    ASSERT_EQ(3, results.size());
    ASSERT_STR_EQ("exact", results[0].first);
    ASSERT_STR_EQ("value", results[0].second);
    END_TEST("scan_exact_match")
}

template <typename EngineType> void test_scan_sorted_order() {
    TEST("scan_sorted_order")
    EngineType engine;

    // Insert in random order
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("z", "last"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("a", "first"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("m", "middle"));

    std::vector<std::pair<std::string, std::string>> results;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("a", 10, results));
    ASSERT_EQ(3, results.size());
    ASSERT_STR_EQ("a", results[0].first);
    ASSERT_STR_EQ("first", results[0].second);
    ASSERT_STR_EQ("m", results[1].first);
    ASSERT_STR_EQ("middle", results[1].second);
    ASSERT_STR_EQ("z", results[2].first);
    ASSERT_STR_EQ("last", results[2].second);
    END_TEST("scan_sorted_order")
}

template <typename EngineType> void test_large_dataset() {
    TEST("large_dataset")
    EngineType engine;

    // Write 1000 entries
    for (int i = 0; i < 1000; ++i) {
        std::string key = "key:" + std::to_string(i);
        std::string value = "value:" + std::to_string(i);
        engine.write(key, value);
    }

    // Read some back
    std::string read_value;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("key:0", read_value));
    ASSERT_STR_EQ("value:0", read_value);
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("key:500", read_value));
    ASSERT_STR_EQ("value:500", read_value);
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("key:999", read_value));
    ASSERT_STR_EQ("value:999", read_value);

    // Scan with limit
    std::vector<std::pair<std::string, std::string>> results;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("key:", 100, results));
    ASSERT_EQ(100, results.size());
    END_TEST("large_dataset")
}

template <typename EngineType> void test_special_characters() {
    TEST("special_characters")
    EngineType engine;

    ASSERT_STATUS_EQ(Status::SUCCESS,
                     engine.write("key:with:colons", "value1"));
    ASSERT_STATUS_EQ(Status::SUCCESS,
                     engine.write("key/with/slashes", "value2"));
    ASSERT_STATUS_EQ(Status::SUCCESS,
                     engine.write("key-with-dashes", "value3"));
    ASSERT_STATUS_EQ(Status::SUCCESS,
                     engine.write("key_with_underscores", "value4"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("key.with.dots", "value5"));

    std::string read_value;
    ASSERT_STATUS_EQ(Status::SUCCESS,
                     engine.read("key:with:colons", read_value));
    ASSERT_STR_EQ("value1", read_value);
    ASSERT_STATUS_EQ(Status::SUCCESS,
                     engine.read("key/with/slashes", read_value));
    ASSERT_STR_EQ("value2", read_value);
    ASSERT_STATUS_EQ(Status::SUCCESS,
                     engine.read("key-with-dashes", read_value));
    ASSERT_STR_EQ("value3", read_value);
    ASSERT_STATUS_EQ(Status::SUCCESS,
                     engine.read("key_with_underscores", read_value));
    ASSERT_STR_EQ("value4", read_value);
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("key.with.dots", read_value));
    ASSERT_STR_EQ("value5", read_value);
    END_TEST("special_characters")
}

template <typename EngineType> void test_manual_locking() {
    TEST("manual_locking")
    EngineType engine;

    // Test exclusive lock
    engine.lock();
    engine.write("locked_key", "locked_value");
    engine.unlock();

    // Test shared lock
    engine.lock_shared();
    std::string read_value;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("locked_key", read_value));
    ASSERT_STR_EQ("locked_value", read_value);
    engine.unlock_shared();

    END_TEST("manual_locking")
}

template <typename EngineType> void test_concurrent_writes() {
    TEST("concurrent_writes")
    EngineType engine;

    const int num_threads = 4;
    const int writes_per_thread = 100;

    auto writer = [&engine](int thread_id) {
        for (int i = 0; i < writes_per_thread; ++i) {
            std::string key = "thread:" + std::to_string(thread_id) +
                              ":key:" + std::to_string(i);
            std::string value = "value:" + std::to_string(i);

            engine.lock();
            ASSERT_STATUS_EQ(Status::SUCCESS, engine.write(key, value));
            engine.unlock();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(writer, i);
    }

    for (auto &t : threads) {
        t.join();
    }

    // Verify all writes succeeded
    for (int thread_id = 0; thread_id < num_threads; ++thread_id) {
        for (int i = 0; i < writes_per_thread; ++i) {
            std::string key = "thread:" + std::to_string(thread_id) +
                              ":key:" + std::to_string(i);
            std::string expected_value = "value:" + std::to_string(i);

            engine.lock_shared();
            std::string actual_value;
            ASSERT_STATUS_EQ(Status::SUCCESS, engine.read(key, actual_value));
            engine.unlock_shared();

            ASSERT_STR_EQ(expected_value, actual_value);
        }
    }
    END_TEST("concurrent_writes")
}

template <typename EngineType> void test_concurrent_reads_writes() {
    TEST("concurrent_reads_writes")
    EngineType engine;

    // Pre-populate with data
    for (int i = 0; i < 50; ++i) {
        ASSERT_STATUS_EQ(Status::SUCCESS,
                         engine.write("key:" + std::to_string(i),
                                      "value:" + std::to_string(i)));
    }

    auto writer = [&engine]() {
        for (int i = 50; i < 100; ++i) {
            engine.lock();
            ASSERT_STATUS_EQ(Status::SUCCESS,
                             engine.write("key:" + std::to_string(i),
                                          "value:" + std::to_string(i)));
            engine.unlock();
        }
    };

    auto reader = [&engine]() {
        for (int i = 0; i < 50; ++i) {
            engine.lock_shared();
            std::string value;
            ASSERT_STATUS_EQ(Status::SUCCESS,
                             engine.read("key:" + std::to_string(i), value));
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
    std::vector<std::pair<std::string, std::string>> results(100);
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("key:", 100, results));
    engine.unlock_shared();

    ASSERT_EQ(100, results.size());
    END_TEST("concurrent_reads_writes")
}

template <typename EngineType> void test_scan_after_updates() {
    TEST("scan_after_updates")
    EngineType engine;

    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("prefix:a", "1"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("prefix:b", "2"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("prefix:c", "3"));

    std::vector<std::pair<std::string, std::string>> results1;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("prefix:", 10, results1));
    ASSERT_EQ(3, results1.size());

    // Add more keys
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("prefix:d", "4"));
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("prefix:e", "5"));

    std::vector<std::pair<std::string, std::string>> results2;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("prefix:", 10, results2));
    ASSERT_EQ(5, results2.size());

    // Update existing key
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.write("prefix:a", "updated"));
    std::string read_value;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.read("prefix:a", read_value));
    ASSERT_STR_EQ("updated", read_value);

    // Scan should still return same number
    std::vector<std::pair<std::string, std::string>> results3;
    ASSERT_STATUS_EQ(Status::SUCCESS, engine.scan("prefix:", 10, results3));
    ASSERT_EQ(5, results3.size());
    END_TEST("scan_after_updates")
}

template <typename EngineType> void test_operation_count() {
    TEST("operation_count")
    EngineType engine;

    // Initial count should be 0
    ASSERT_EQ(0, engine.operation_count());

    // Perform an arbitrary number of operations
    const size_t num_operations = 50;
    for (size_t i = 0; i < num_operations; ++i) {
        std::string key = "key:" + std::to_string(i);
        std::string value = "value:" + std::to_string(i);
        engine.write(key, value);
    }

    // Perform some read operations
    for (size_t i = 0; i < 20; ++i) {
        std::string key = "key:" + std::to_string(i);
        std::string value;
        engine.read(key, value);
    }

    // Perform some scan operations
    for (size_t i = 0; i < 5; ++i) {
        std::vector<std::pair<std::string, std::string>> results;
        engine.scan("key:", 10, results);
    }

    // Sleep for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Check the count: 50 writes + 20 reads + 5 scans = 75 operations
    size_t expected_count = 50 + 20 + 5;
    ASSERT_EQ(expected_count, engine.operation_count());
    END_TEST("operation_count")
}

// Helper function to run all tests for a given engine type
template <typename EngineType>
void run_storage_engine_test_suite(const std::string &engine_name) {
    std::vector<std::pair<std::string, TestFunction>> tests = {
        {"basic_write_read", []() { test_basic_write_read<EngineType>(); }},
        {"read_nonexistent_key",
         []() { test_read_nonexistent_key<EngineType>(); }},
        {"overwrite_value", []() { test_overwrite_value<EngineType>(); }},
        {"scan_basic", []() { test_scan_basic<EngineType>(); }},
        {"scan_with_limit", []() { test_scan_with_limit<EngineType>(); }},
        {"scan_no_matches", []() { test_scan_no_matches<EngineType>(); }},
        {"scan_exact_match", []() { test_scan_exact_match<EngineType>(); }},
        {"scan_sorted_order", []() { test_scan_sorted_order<EngineType>(); }},
        {"scan_after_updates", []() { test_scan_after_updates<EngineType>(); }},
        {"scan_empty_prefix", []() { test_scan_empty_prefix<EngineType>(); }},
        {"large_dataset", []() { test_large_dataset<EngineType>(); }},
        {"special_characters", []() { test_special_characters<EngineType>(); }},
        {"manual_locking", []() { test_manual_locking<EngineType>(); }},
        {"concurrent_writes", []() { test_concurrent_writes<EngineType>(); }},
        {"concurrent_reads_writes",
         []() { test_concurrent_reads_writes<EngineType>(); }},
        {"operation_count", []() { test_operation_count<EngineType>(); }}};

    run_test_suite(engine_name, tests);
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Generic StorageEngine Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    // Test all storage engine implementations
    run_storage_engine_test_suite<MapStorageEngine>("MapStorageEngine");
    run_storage_engine_test_suite<TkrzwHashStorageEngine>(
        "TkrzwHashStorageEngine");
    run_storage_engine_test_suite<TkrzwTreeStorageEngine>(
        "TkrzwTreeStorageEngine");
    run_storage_engine_test_suite<LmdbStorageEngine>("LmdbStorageEngine");
    run_storage_engine_test_suite<TbbStorageEngine>("TbbStorageEngine");

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Overall Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << tests_failed << std::endl;
    std::cout << "Total tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << std::endl;

    if (tests_failed == 0) {
        std::cout << "✓ All tests passed for all storage engines!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}
