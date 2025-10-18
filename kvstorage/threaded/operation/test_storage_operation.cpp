#include "Operation.h"
#include "ReadOperation.h"
#include "../../../storage/MapStorageEngine.h"
#include "../../../utils/test_assertions.h"
#include <iostream>
#include <string>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_operation_with_storage() {
    TEST("operation_with_storage")
        std::string key = "test_key";
        MapStorageEngine storage(0);
        
        Operation<MapStorageEngine> operation(&key, Type::READ, &storage);
        
        // Test that operation has access to storage
        ASSERT_TRUE(&operation.storage() == &storage);
        
        // Test that operation type is correct
        ASSERT_TRUE(operation.type() == Type::READ);
        ASSERT_STR_EQ("test_key", operation.key());
    END_TEST("operation_with_storage")
}

void test_operation_without_storage() {
    TEST("operation_without_storage")
        std::string key = "test_key";
        
        Operation<MapStorageEngine> operation(&key, Type::WRITE);
        
        // Test that operation type is correct
        ASSERT_TRUE(operation.type() == Type::WRITE);
        ASSERT_STR_EQ("test_key", operation.key());
        
        // Note: storage() method would cause undefined behavior if called
        // since storage_ is nullptr, so we don't test it here
    END_TEST("operation_without_storage")
}

void test_readoperation_with_storage() {
    TEST("readoperation_with_storage")
        std::string key = "read_key";
        std::string value = "read_value";
        MapStorageEngine storage(0);
        
        ReadOperation<MapStorageEngine> read_op(key, value, storage);
        
        // Test that ReadOperation inherits storage access
        ASSERT_TRUE(&read_op.storage() == &storage);
        
        // Test that ReadOperation type is correct
        ASSERT_TRUE(read_op.type() == Type::READ);
        ASSERT_STR_EQ("read_key", read_op.key());
        
        // Test that value access still works
        ASSERT_STR_EQ("read_value", read_op.value());
    END_TEST("readoperation_with_storage")
}

void test_readoperation_without_storage() {
    TEST("readoperation_without_storage")
        std::string key = "read_key";
        std::string value = "read_value";
        
        ReadOperation<MapStorageEngine> read_op(key, value);
        
        // Test that ReadOperation type is correct
        ASSERT_TRUE(read_op.type() == Type::READ);
        ASSERT_STR_EQ("read_key", read_op.key());
        
        // Test that value access works
        ASSERT_STR_EQ("read_value", read_op.value());
        
        // Note: storage() method would cause undefined behavior if called
        // since storage_ is nullptr, so we don't test it here
    END_TEST("readoperation_without_storage")
}

int main() {
    std::cout << "Starting Operation with Storage tests..." << std::endl << std::endl;
    
    test_operation_with_storage();
    test_operation_without_storage();
    test_readoperation_with_storage();
    test_readoperation_without_storage();
    
    std::cout << std::endl << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "All Operation with Storage tests completed successfully!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
