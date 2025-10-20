#include "Operation.h"
#include "ReadOperation.h"
#include "../../../storage/MapStorageEngine.h"
#include "../../../utils/test_assertions.h"
#include <iostream>
#include <string>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_basic_operation() {
    TEST("basic_operation")
        std::string key = "test_key";
        
        Operation operation(&key, Type::WRITE);
        
        // Test that operation type is correct
        ASSERT_TRUE(operation.type() == Type::WRITE);
        ASSERT_STR_EQ("test_key", operation.key());

    END_TEST("basic_operation")
}

void test_readoperation() {
    TEST("readoperation")
        std::string key = "read_key";
        std::string value = "read_value";
        
        ReadOperation read_op(key, value);
        
        // Test that ReadOperation type is correct
        ASSERT_TRUE(read_op.type() == Type::READ);
        ASSERT_STR_EQ("read_key", read_op.key());
        
        // Test that value access works
        ASSERT_STR_EQ("read_value", read_op.value());
    END_TEST("readoperation")
}

int main() {
    std::cout << "Starting Operation with Storage tests..." << std::endl << std::endl;
    
    test_basic_operation();
    test_readoperation();
    
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
