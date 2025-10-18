#include "Request.h"
#include "../../utils/test_assertions.h"
#include <iostream>
#include <string>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_type_enum() {
    TEST("type_enum")
        // Test enum values
        ASSERT_EQ(0, static_cast<int>(Type::READ));
        ASSERT_EQ(1, static_cast<int>(Type::WRITE));
        ASSERT_EQ(2, static_cast<int>(Type::SCAN));
    END_TEST("type_enum")
}

void test_read_request() {
    TEST("read_request")
        std::string key_data = "user_123";
        std::string* key_ptr = &key_data;
        
        Request request(key_ptr, Type::READ);
        
        // Test type access
        ASSERT_TRUE(request.type() == Type::READ);
        
        // Test key access
        ASSERT_STR_EQ("user_123", request.key());
    END_TEST("read_request")
}

void test_write_request() {
    TEST("write_request")
        std::string key_data = "product_456";
        std::string* key_ptr = &key_data;
        
        Request request(key_ptr, Type::WRITE);
        
        // Test type access
        ASSERT_TRUE(request.type() == Type::WRITE);
        
        // Test key access
        ASSERT_STR_EQ("product_456", request.key());
    END_TEST("write_request")
}

void test_scan_request() {
    TEST("scan_request")
        std::string key_data = "scan_prefix";
        std::string* key_ptr = &key_data;
        
        Request request(key_ptr, Type::SCAN);
        
        // Test type access
        ASSERT_TRUE(request.type() == Type::SCAN);
        
        // Test key access
        ASSERT_STR_EQ("scan_prefix", request.key());
    END_TEST("scan_request")
}

void test_key_array_access() {
    TEST("key_array_access")
        // Create an array of strings
        std::string keys[] = {"key1", "key2", "key3", "key4"};
        std::string* key_ptr = keys;
        
        Request request(key_ptr, Type::READ);
        
        // Test array access
        ASSERT_STR_EQ("key1", request.key(0));
        ASSERT_STR_EQ("key2", request.key(1));
        ASSERT_STR_EQ("key3", request.key(2));
        ASSERT_STR_EQ("key4", request.key(3));
    END_TEST("key_array_access")
}

void test_constructor_parameters() {
    TEST("constructor_parameters")
        // Test with different key values and types
        std::string key1 = "test_key_1";
        std::string key2 = "test_key_2";
        std::string key3 = "test_key_3";
        
        Request read_req(&key1, Type::READ);
        Request write_req(&key2, Type::WRITE);
        Request scan_req(&key3, Type::SCAN);
        
        // Verify each request has correct type and key
        ASSERT_TRUE(read_req.type() == Type::READ);
        ASSERT_STR_EQ("test_key_1", read_req.key());
        
        ASSERT_TRUE(write_req.type() == Type::WRITE);
        ASSERT_STR_EQ("test_key_2", write_req.key());
        
        ASSERT_TRUE(scan_req.type() == Type::SCAN);
        ASSERT_STR_EQ("test_key_3", scan_req.key());
    END_TEST("constructor_parameters")
}

void test_const_correctness() {
    TEST("const_correctness")
        std::string key_data = "const_test_key";
        std::string* key_ptr = &key_data;
        
        const Request request(key_ptr, Type::READ);
        
        // Test that const methods work
        Type type = request.type();
        std::string key = request.key();
        
        ASSERT_TRUE(type == Type::READ);
        ASSERT_STR_EQ("const_test_key", key);
    END_TEST("const_correctness")
}

void test_pointer_relationship() {
    TEST("pointer_relationship")
        std::string key_data = "pointer_test";
        std::string* key_ptr = &key_data;
        
        Request request(key_ptr, Type::WRITE);
        
        // Test that the request holds a reference to the original data
        ASSERT_TRUE(&request.key() == &key_data);
        
        // Modify original data and verify request sees the change
        key_data = "modified_key";
        ASSERT_STR_EQ("modified_key", request.key());
    END_TEST("pointer_relationship")
}

int main() {
    std::cout << "Starting Request class tests..." << std::endl << std::endl;
    
    test_type_enum();
    test_read_request();
    test_write_request();
    test_scan_request();
    test_key_array_access();
    test_constructor_parameters();
    test_const_correctness();
    test_pointer_relationship();
    
    std::cout << std::endl << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "All Request tests completed successfully!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
