#include "WriteOperation.h"
#include "../../../storage/MapStorageEngine.h"
#include "../../../utils/test_assertions.h"
#include <iostream>
#include <string>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_write_operation_constructor() {
    TEST("write_operation_constructor")
        std::string key = "test_key";
        std::string value = "test_value";
        
        WriteOperation<MapStorageEngine> write_op(key, value);
        
        // Test that it inherits from Operation correctly
        ASSERT_TRUE(write_op.type() == Type::WRITE);
        ASSERT_STR_EQ("test_key", write_op.key());
    END_TEST("write_operation_constructor")
}

void test_write_operation_value_access() {
    TEST("write_operation_value_access")
        std::string key = "user_123";
        std::string value = "user_data";
        
        WriteOperation<MapStorageEngine> write_op(key, value);
        
        // Test value method
        std::string& value_ref = write_op.value();
        ASSERT_STR_EQ("user_data", value_ref);
        
        // Test that we can modify the value through the reference
        value_ref = "modified_data";
        ASSERT_STR_EQ("modified_data", value_ref);
        
        // Test that the original value is also modified
        ASSERT_STR_EQ("modified_data", value);
    END_TEST("write_operation_value_access")
}

void test_write_operation_with_storage() {
    TEST("write_operation_with_storage")
        std::string key = "write_key";
        std::string value = "write_value";
        MapStorageEngine storage(0);
        
        WriteOperation<MapStorageEngine> write_op(key, value, storage);
        
        // Test that WriteOperation inherits storage access
        ASSERT_TRUE(&write_op.storage() == &storage);
        
        // Test that WriteOperation type is correct
        ASSERT_TRUE(write_op.type() == Type::WRITE);
        ASSERT_STR_EQ("write_key", write_op.key());
        
        // Test that value access works
        ASSERT_STR_EQ("write_value", write_op.value());
    END_TEST("write_operation_with_storage")
}

void test_write_operation_without_storage() {
    TEST("write_operation_without_storage")
        std::string key = "write_key";
        std::string value = "write_value";
        
        WriteOperation<MapStorageEngine> write_op(key, value);
        
        // Test that WriteOperation type is correct
        ASSERT_TRUE(write_op.type() == Type::WRITE);
        ASSERT_STR_EQ("write_key", write_op.key());
        
        // Test that value access works
        ASSERT_STR_EQ("write_value", write_op.value());
        
        // Note: storage() method would cause undefined behavior if called
        // since storage_ is nullptr, so we don't test it here
    END_TEST("write_operation_without_storage")
}

void test_write_operation_inheritance() {
    TEST("write_operation_inheritance")
        std::string key = "inheritance_test";
        std::string value = "inheritance_value";
        
        WriteOperation<MapStorageEngine> write_op(key, value);
        
        // Test that WriteOperation can be used as Operation
        ASSERT_TRUE(write_op.type() == Type::WRITE);
        ASSERT_STR_EQ("inheritance_test", write_op.key());
        
        // Test that WriteOperation has additional functionality
        ASSERT_STR_EQ("inheritance_value", write_op.value());
    END_TEST("write_operation_inheritance")
}

void test_write_operation_value_modification() {
    TEST("write_operation_value_modification")
        std::string key = "modify_test";
        std::string value = "original_value";
        
        WriteOperation<MapStorageEngine> write_op(key, value);
        
        // Test initial value
        ASSERT_STR_EQ("original_value", write_op.value());
        ASSERT_STR_EQ("original_value", value);
        
        // Modify through WriteOperation
        write_op.value() = "new_value";
        
        // Test that both references show the change
        ASSERT_STR_EQ("new_value", write_op.value());
        ASSERT_STR_EQ("new_value", value);
        
        // Modify original value directly
        value = "direct_change";
        
        // Test that WriteOperation sees the change
        ASSERT_STR_EQ("direct_change", write_op.value());
    END_TEST("write_operation_value_modification")
}

void test_write_operation_different_storage_types() {
    TEST("write_operation_different_storage_types")
        std::string key = "storage_type_test";
        std::string value = "storage_value";
        MapStorageEngine storage(0);
        
        // Test with MapStorageEngine
        WriteOperation<MapStorageEngine> write_op1(key, value, storage);
        ASSERT_TRUE(write_op1.type() == Type::WRITE);
        ASSERT_STR_EQ("storage_type_test", write_op1.key());
        ASSERT_STR_EQ("storage_value", write_op1.value());
        ASSERT_TRUE(&write_op1.storage() == &storage);
        
        // Test without storage (should still work)
        WriteOperation<MapStorageEngine> write_op2(key, value);
        ASSERT_TRUE(write_op2.type() == Type::WRITE);
        ASSERT_STR_EQ("storage_type_test", write_op2.key());
        ASSERT_STR_EQ("storage_value", write_op2.value());
    END_TEST("write_operation_different_storage_types")
}

int main() {
    std::cout << "Starting WriteOperation class tests..." << std::endl << std::endl;
    
    test_write_operation_constructor();
    test_write_operation_value_access();
    test_write_operation_with_storage();
    test_write_operation_without_storage();
    test_write_operation_inheritance();
    test_write_operation_value_modification();
    test_write_operation_different_storage_types();
    
    std::cout << std::endl << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "All WriteOperation tests completed successfully!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
