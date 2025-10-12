#include "MapKeyStorage.h"
#include "TkrzwHashKeyStorage.h"
#include "TkrzwTreeKeyStorage.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>

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

// Helper function to perform a scan using lower_bound and iterator incrementation
template<typename StorageType, typename ValueType>
std::vector<std::pair<std::string, ValueType>> scan_storage(
    StorageType& storage, 
    const std::string& start_key, 
    size_t limit) 
{
    std::vector<std::pair<std::string, ValueType>> results;
    auto it = storage.lower_bound(start_key);
    
    size_t count = 0;
    while (!it.is_end() && count < limit) {
        results.push_back({it.get_key(), it.get_value()});
        ++it;
        count++;
    }
    
    return results;
}

// Generic test functions that work with any KeyStorage implementation
template<typename StorageType, typename ValueType>
void test_basic_put_get() {
    TEST("basic_put_get")
        StorageType storage;
        
        // Put some values
        storage.put("key1", static_cast<ValueType>(100));
        storage.put("key2", static_cast<ValueType>(200));
        storage.put("key3", static_cast<ValueType>(300));
        
        // Get them back
        ValueType value;
        ASSERT_TRUE(storage.get("key1", value));
        ASSERT_EQ(static_cast<ValueType>(100), value);
        
        ASSERT_TRUE(storage.get("key2", value));
        ASSERT_EQ(static_cast<ValueType>(200), value);
        
        ASSERT_TRUE(storage.get("key3", value));
        ASSERT_EQ(static_cast<ValueType>(300), value);
    END_TEST("basic_put_get")
}

template<typename StorageType, typename ValueType>
void test_get_nonexistent_key() {
    TEST("get_nonexistent_key")
        StorageType storage;
        
        ValueType value;
        ASSERT_FALSE(storage.get("nonexistent", value));
    END_TEST("get_nonexistent_key")
}

template<typename StorageType, typename ValueType>
void test_overwrite_value() {
    TEST("overwrite_value")
        StorageType storage;
        
        storage.put("key", static_cast<ValueType>(100));
        ValueType value;
        ASSERT_TRUE(storage.get("key", value));
        ASSERT_EQ(static_cast<ValueType>(100), value);
        
        storage.put("key", static_cast<ValueType>(200));
        ASSERT_TRUE(storage.get("key", value));
        ASSERT_EQ(static_cast<ValueType>(200), value);
    END_TEST("overwrite_value")
}

template<typename StorageType, typename ValueType>
void test_empty_key() {
    TEST("empty_key")
        StorageType storage;
        
        storage.put("", static_cast<ValueType>(999));
        ValueType value;
        ASSERT_TRUE(storage.get("", value));
        ASSERT_EQ(static_cast<ValueType>(999), value);
    END_TEST("empty_key")
}

template<typename StorageType, typename ValueType>
void test_lower_bound_basic() {
    TEST("lower_bound_basic")
        StorageType storage;
        
        storage.put("user:1001", static_cast<ValueType>(100));
        storage.put("user:1002", static_cast<ValueType>(200));
        storage.put("user:1003", static_cast<ValueType>(300));
        storage.put("product:2001", static_cast<ValueType>(400));
        
        // Lower bound on "user:" should find first user key
        auto it = storage.lower_bound("user:");
        ASSERT_FALSE(it.is_end());
        ASSERT_STR_EQ("user:1001", it.get_key());
        ASSERT_EQ(static_cast<ValueType>(100), it.get_value());
    END_TEST("lower_bound_basic")
}

template<typename StorageType, typename ValueType>
void test_lower_bound_exact_match() {
    TEST("lower_bound_exact_match")
        StorageType storage;
        
        storage.put("exact", static_cast<ValueType>(100));
        storage.put("exactly", static_cast<ValueType>(200));
        storage.put("exact_match", static_cast<ValueType>(300));
        
        auto it = storage.lower_bound("exact");
        ASSERT_FALSE(it.is_end());
        ASSERT_STR_EQ("exact", it.get_key());
        ASSERT_EQ(static_cast<ValueType>(100), it.get_value());
    END_TEST("lower_bound_exact_match")
}

template<typename StorageType, typename ValueType>
void test_lower_bound_no_match() {
    TEST("lower_bound_no_match")
        StorageType storage;
        
        storage.put("apple", static_cast<ValueType>(10));
        storage.put("banana", static_cast<ValueType>(20));
        
        // Lower bound on "zzz" should return end
        auto it = storage.lower_bound("zzz");
        ASSERT_TRUE(it.is_end());
    END_TEST("lower_bound_no_match")
}

template<typename StorageType, typename ValueType>
void test_lower_bound_empty_prefix() {
    TEST("lower_bound_empty_prefix")
        StorageType storage;
        
        storage.put("a", static_cast<ValueType>(1));
        storage.put("b", static_cast<ValueType>(2));
        storage.put("c", static_cast<ValueType>(3));
        
        // Empty prefix should start from beginning
        auto it = storage.lower_bound("");
        ASSERT_FALSE(it.is_end());
        ASSERT_STR_EQ("a", it.get_key());
    END_TEST("lower_bound_empty_prefix")
}

template<typename StorageType, typename ValueType>
void test_iterator_incrementation() {
    TEST("iterator_incrementation")
        StorageType storage;
        
        storage.put("a", static_cast<ValueType>(1));
        storage.put("b", static_cast<ValueType>(2));
        storage.put("c", static_cast<ValueType>(3));
        
        auto it = storage.lower_bound("a");
        ASSERT_FALSE(it.is_end());
        ASSERT_STR_EQ("a", it.get_key());
        ASSERT_EQ(static_cast<ValueType>(1), it.get_value());
        
        ++it;
        ASSERT_FALSE(it.is_end());
        ASSERT_STR_EQ("b", it.get_key());
        ASSERT_EQ(static_cast<ValueType>(2), it.get_value());
        
        ++it;
        ASSERT_FALSE(it.is_end());
        ASSERT_STR_EQ("c", it.get_key());
        ASSERT_EQ(static_cast<ValueType>(3), it.get_value());
        
        ++it;
        ASSERT_TRUE(it.is_end());
    END_TEST("iterator_incrementation")
}

template<typename StorageType, typename ValueType>
void test_scan_basic() {
    TEST("scan_basic")
        StorageType storage;
        
        storage.put("user:1001", static_cast<ValueType>(100));
        storage.put("user:1002", static_cast<ValueType>(200));
        storage.put("user:1003", static_cast<ValueType>(300));
        storage.put("product:2001", static_cast<ValueType>(400));
        
        auto results = scan_storage<StorageType, ValueType>(storage, "user:", 10);
        ASSERT_EQ(3, results.size());
        ASSERT_STR_EQ("user:1001", results[0].first);
        ASSERT_EQ(static_cast<ValueType>(100), results[0].second);
        ASSERT_STR_EQ("user:1002", results[1].first);
        ASSERT_EQ(static_cast<ValueType>(200), results[1].second);
        ASSERT_STR_EQ("user:1003", results[2].first);
        ASSERT_EQ(static_cast<ValueType>(300), results[2].second);
    END_TEST("scan_basic")
}

template<typename StorageType, typename ValueType>
void test_scan_with_limit() {
    TEST("scan_with_limit")
        StorageType storage;
        
        storage.put("item:001", static_cast<ValueType>(1));
        storage.put("item:002", static_cast<ValueType>(2));
        storage.put("item:003", static_cast<ValueType>(3));
        storage.put("item:004", static_cast<ValueType>(4));
        storage.put("item:005", static_cast<ValueType>(5));
        
        auto results = scan_storage<StorageType, ValueType>(storage, "item:", 3);
        ASSERT_EQ(3, results.size());
        ASSERT_STR_EQ("item:001", results[0].first);
        ASSERT_STR_EQ("item:002", results[1].first);
        ASSERT_STR_EQ("item:003", results[2].first);
    END_TEST("scan_with_limit")
}

template<typename StorageType, typename ValueType>
void test_scan_no_matches() {
    TEST("scan_no_matches")
        StorageType storage;
        
        storage.put("apple", static_cast<ValueType>(10));
        storage.put("banana", static_cast<ValueType>(20));
        
        auto results = scan_storage<StorageType, ValueType>(storage, "orange", 10);
        ASSERT_EQ(0, results.size());
    END_TEST("scan_no_matches")
}

template<typename StorageType, typename ValueType>
void test_scan_empty_prefix() {
    TEST("scan_empty_prefix")
        StorageType storage;
        
        storage.put("a", static_cast<ValueType>(1));
        storage.put("b", static_cast<ValueType>(2));
        storage.put("c", static_cast<ValueType>(3));
        
        auto results = scan_storage<StorageType, ValueType>(storage, "", 10);
        ASSERT_EQ(3, results.size());
    END_TEST("scan_empty_prefix")
}

template<typename StorageType, typename ValueType>
void test_scan_sorted_order() {
    TEST("scan_sorted_order")
        StorageType storage;
        
        // Insert in random order
        storage.put("z", static_cast<ValueType>(26));
        storage.put("a", static_cast<ValueType>(1));
        storage.put("m", static_cast<ValueType>(13));
        
        auto results = scan_storage<StorageType, ValueType>(storage, "", 10);
        ASSERT_EQ(3, results.size());
        ASSERT_STR_EQ("a", results[0].first);
        ASSERT_EQ(static_cast<ValueType>(1), results[0].second);
        ASSERT_STR_EQ("m", results[1].first);
        ASSERT_EQ(static_cast<ValueType>(13), results[1].second);
        ASSERT_STR_EQ("z", results[2].first);
        ASSERT_EQ(static_cast<ValueType>(26), results[2].second);
    END_TEST("scan_sorted_order")
}

template<typename StorageType, typename ValueType>
void test_scan_partial_prefix() {
    TEST("scan_partial_prefix")
        StorageType storage;
        
        storage.put("user:1001", static_cast<ValueType>(100));
        storage.put("user:1002", static_cast<ValueType>(200));
        storage.put("user:1003", static_cast<ValueType>(300));
        
        // Scan starting from middle
        auto results = scan_storage<StorageType, ValueType>(storage, "user:1002", 10);
        ASSERT_EQ(2, results.size());
        ASSERT_STR_EQ("user:1002", results[0].first);
        ASSERT_STR_EQ("user:1003", results[1].first);
    END_TEST("scan_partial_prefix")
}

template<typename StorageType, typename ValueType>
void test_large_dataset() {
    TEST("large_dataset")
        StorageType storage;
        
        // Put 1000 entries
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key:" + std::to_string(i);
            storage.put(key, static_cast<ValueType>(i));
        }
        
        // Get some back
        ValueType value;
        ASSERT_TRUE(storage.get("key:0", value));
        ASSERT_EQ(static_cast<ValueType>(0), value);
        
        ASSERT_TRUE(storage.get("key:500", value));
        ASSERT_EQ(static_cast<ValueType>(500), value);
        
        ASSERT_TRUE(storage.get("key:999", value));
        ASSERT_EQ(static_cast<ValueType>(999), value);
        
        // Scan with limit
        auto results = scan_storage<StorageType, ValueType>(storage, "key:", 100);
        ASSERT_EQ(100, results.size());
    END_TEST("large_dataset")
}

template<typename StorageType, typename ValueType>
void test_special_characters() {
    TEST("special_characters")
        StorageType storage;
        
        storage.put("key:with:colons", static_cast<ValueType>(1));
        storage.put("key/with/slashes", static_cast<ValueType>(2));
        storage.put("key-with-dashes", static_cast<ValueType>(3));
        storage.put("key_with_underscores", static_cast<ValueType>(4));
        storage.put("key.with.dots", static_cast<ValueType>(5));
        
        ValueType value;
        ASSERT_TRUE(storage.get("key:with:colons", value));
        ASSERT_EQ(static_cast<ValueType>(1), value);
        ASSERT_TRUE(storage.get("key/with/slashes", value));
        ASSERT_EQ(static_cast<ValueType>(2), value);
        ASSERT_TRUE(storage.get("key-with-dashes", value));
        ASSERT_EQ(static_cast<ValueType>(3), value);
        ASSERT_TRUE(storage.get("key_with_underscores", value));
        ASSERT_EQ(static_cast<ValueType>(4), value);
        ASSERT_TRUE(storage.get("key.with.dots", value));
        ASSERT_EQ(static_cast<ValueType>(5), value);
    END_TEST("special_characters")
}

template<typename StorageType, typename ValueType>
void test_scan_after_updates() {
    TEST("scan_after_updates")
        StorageType storage;
        
        storage.put("prefix:a", static_cast<ValueType>(1));
        storage.put("prefix:b", static_cast<ValueType>(2));
        storage.put("prefix:c", static_cast<ValueType>(3));
        
        auto results1 = scan_storage<StorageType, ValueType>(storage, "prefix:", 10);
        ASSERT_EQ(3, results1.size());
        
        // Add more keys
        storage.put("prefix:d", static_cast<ValueType>(4));
        storage.put("prefix:e", static_cast<ValueType>(5));
        
        auto results2 = scan_storage<StorageType, ValueType>(storage, "prefix:", 10);
        ASSERT_EQ(5, results2.size());
        
        // Update existing key
        storage.put("prefix:a", static_cast<ValueType>(999));
        ValueType value;
        ASSERT_TRUE(storage.get("prefix:a", value));
        ASSERT_EQ(static_cast<ValueType>(999), value);
        
        // Scan should still return same number with updated value
        auto results3 = scan_storage<StorageType, ValueType>(storage, "prefix:", 10);
        ASSERT_EQ(5, results3.size());
        ASSERT_EQ(static_cast<ValueType>(999), results3[0].second);
    END_TEST("scan_after_updates")
}

template<typename StorageType, typename ValueType>
void test_numeric_value_ranges() {
    TEST("numeric_value_ranges")
        StorageType storage;
        
        // Test with various numeric values
        storage.put("min", static_cast<ValueType>(0));
        storage.put("small", static_cast<ValueType>(1));
        storage.put("medium", static_cast<ValueType>(1000));
        storage.put("large", static_cast<ValueType>(1000000));
        
        ValueType value;
        ASSERT_TRUE(storage.get("min", value));
        ASSERT_EQ(static_cast<ValueType>(0), value);
        
        ASSERT_TRUE(storage.get("small", value));
        ASSERT_EQ(static_cast<ValueType>(1), value);
        
        ASSERT_TRUE(storage.get("medium", value));
        ASSERT_EQ(static_cast<ValueType>(1000), value);
        
        ASSERT_TRUE(storage.get("large", value));
        ASSERT_EQ(static_cast<ValueType>(1000000), value);
    END_TEST("numeric_value_ranges")
}

// Helper function to run all tests for a given storage type and value type
template<typename StorageType, typename ValueType>
void run_test_suite(const std::string& storage_name, const std::string& value_type_name) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Testing: " << storage_name << "<" << value_type_name << ">" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Basic functionality tests
    test_basic_put_get<StorageType, ValueType>();
    test_get_nonexistent_key<StorageType, ValueType>();
    test_overwrite_value<StorageType, ValueType>();
    test_empty_key<StorageType, ValueType>();
    
    // Lower bound tests
    test_lower_bound_basic<StorageType, ValueType>();
    test_lower_bound_exact_match<StorageType, ValueType>();
    test_lower_bound_no_match<StorageType, ValueType>();
    test_lower_bound_empty_prefix<StorageType, ValueType>();
    
    // Iterator tests
    test_iterator_incrementation<StorageType, ValueType>();
    
    // Scan tests (using lower_bound + iterator)
    test_scan_basic<StorageType, ValueType>();
    test_scan_with_limit<StorageType, ValueType>();
    test_scan_no_matches<StorageType, ValueType>();
    test_scan_empty_prefix<StorageType, ValueType>();
    test_scan_sorted_order<StorageType, ValueType>();
    test_scan_partial_prefix<StorageType, ValueType>();
    test_scan_after_updates<StorageType, ValueType>();
    
    // Edge cases and special scenarios
    test_large_dataset<StorageType, ValueType>();
    test_special_characters<StorageType, ValueType>();
    test_numeric_value_ranges<StorageType, ValueType>();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Generic KeyStorage Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Test all storage implementations with int
    std::cout << "\n=== Testing with int ===" << std::endl;
    run_test_suite<MapKeyStorage<int>, int>("MapKeyStorage", "int");
    run_test_suite<TkrzwHashKeyStorage<int>, int>("TkrzwHashKeyStorage", "int");
    run_test_suite<TkrzwTreeKeyStorage<int>, int>("TkrzwTreeKeyStorage", "int");
    
    // Test all storage implementations with long
    std::cout << "\n=== Testing with long ===" << std::endl;
    run_test_suite<MapKeyStorage<long>, long>("MapKeyStorage", "long");
    run_test_suite<TkrzwHashKeyStorage<long>, long>("TkrzwHashKeyStorage", "long");
    run_test_suite<TkrzwTreeKeyStorage<long>, long>("TkrzwTreeKeyStorage", "long");
    
    // Test all storage implementations with uint64_t
    std::cout << "\n=== Testing with uint64_t ===" << std::endl;
    run_test_suite<MapKeyStorage<uint64_t>, uint64_t>("MapKeyStorage", "uint64_t");
    run_test_suite<TkrzwHashKeyStorage<uint64_t>, uint64_t>("TkrzwHashKeyStorage", "uint64_t");
    run_test_suite<TkrzwTreeKeyStorage<uint64_t>, uint64_t>("TkrzwTreeKeyStorage", "uint64_t");
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Overall Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << tests_failed << std::endl;
    std::cout << "Total tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "✓ All tests passed for all key storage implementations!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}

