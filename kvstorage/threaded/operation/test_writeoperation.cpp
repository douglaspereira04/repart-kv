#include "WriteOperation.h"
#include "../../../storage/MapStorageEngine.h"
#include "../../../utils/test_assertions.h"
#include <iostream>
#include <string>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_writeoperation() {
    TEST("writeoperation")
    std::string key = "write_key";
    std::string value = "write_value";

    WriteOperation write_op(key, value);

    // Test that WriteOperation type is correct
    ASSERT_TRUE(write_op.type() == Type::WRITE);
    ASSERT_STR_EQ("write_key", write_op.key());
    END_TEST("writeoperation")
}

void test_write_operation_value_access() {
    TEST("write_operation_value_access")
    WriteOperation *write_op_ptr;
    {
        std::string key = "user_123";
        std::string value = "user_data";

        WriteOperation *write_op = new WriteOperation(key, value);
        write_op_ptr = write_op;
        ASSERT_TRUE(write_op_ptr->key() == key);
        ASSERT_TRUE(write_op_ptr->value() == value);
        ASSERT_FALSE(&write_op_ptr->key() == &key);
        ASSERT_FALSE(&write_op_ptr->value() == &value);
    }

    delete write_op_ptr;
    END_TEST("write_operation_value_access")
}

void test_write_operation() {
    TEST("write_operation")
    std::string key = "write_key";
    std::string value = "write_value";

    WriteOperation write_op(key, value);

    // Test that WriteOperation type is correct
    ASSERT_TRUE(write_op.type() == Type::WRITE);
    ASSERT_STR_EQ("write_key", write_op.key());

    // Test that value access works
    ASSERT_STR_EQ("write_value", write_op.value());

    // Note: storage() method would cause undefined behavior if called
    // since storage_ is nullptr, so we don't test it here
    END_TEST("write_operation")
}

void test_write_operation_inheritance() {
    TEST("write_operation_inheritance")
    std::string key = "inheritance_test";
    std::string value = "inheritance_value";

    WriteOperation write_op(key, value);

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

    WriteOperation write_op(key, value);

    // Test initial value
    ASSERT_STR_EQ("original_value", write_op.value());
    ASSERT_STR_EQ("original_value", value);

    // Modify value should not change the WriteOperation value
    value = "new_value";
    key = "new_key";
    ASSERT_STR_EQ("original_value", write_op.value());
    ASSERT_STR_EQ("modify_test", write_op.key());
    ASSERT_FALSE(&write_op.key() == &key);
    ASSERT_FALSE(&write_op.value() == &value);
    END_TEST("write_operation_value_modification")
}

int main() {
    std::cout << "Starting WriteOperation class tests..." << std::endl
              << std::endl;

    test_writeoperation();
    test_write_operation_value_access();
    test_write_operation();
    test_write_operation_inheritance();
    test_write_operation_value_modification();

    std::cout << std::endl << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;

    if (tests_failed == 0) {
        std::cout << "All WriteOperation tests completed successfully!"
                  << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
