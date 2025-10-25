#include "ReadOperation.h"
#include "../../../storage/MapStorageEngine.h"
#include "../../../utils/test_assertions.h"
#include <iostream>
#include <string>
#include <vector>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_read_operation_constructor() {
    TEST("read_operation_constructor")
    std::string key = "test_key";
    std::string value = "test_value";

    ReadOperation read_op(key, value);

    // Test that it inherits from Operation correctly
    ASSERT_TRUE(read_op.type() == Type::READ);
    ASSERT_STR_EQ("test_key", read_op.key());
    END_TEST("read_operation_constructor")
}

void test_read_operation_value_access() {
    TEST("read_operation_value_access")
    std::string key = "user_123";
    std::string value = "user_data";

    ReadOperation read_op(key, value);

    // Test value method
    std::string &value_ref = read_op.value();
    ASSERT_STR_EQ("user_data", value_ref);

    // Test that we can modify the value through the reference
    value_ref = "modified_data";
    ASSERT_STR_EQ("modified_data", value);
    END_TEST("read_operation_value_access")
}

void test_read_operation_notify() {
    TEST("read_operation_notify")
    std::string key = "notify_test";
    std::string value = "notify_value";

    ReadOperation read_op(key, value);

    // Test notify method (should not block since we're not calling wait)
    read_op.notify();

    // Verify the operation still works after notify
    ASSERT_STR_EQ("notify_value", read_op.value());
    END_TEST("read_operation_notify")
}

void test_read_operation_inheritance() {
    TEST("read_operation_inheritance")
    std::string key = "inheritance_test";
    std::string value = "inheritance_value";

    ReadOperation read_op(key, value);

    // Test that ReadOperation can be used as Operation
    ASSERT_TRUE(read_op.type() == Type::READ);
    ASSERT_STR_EQ("inheritance_test", read_op.key());

    // Test that ReadOperation has additional functionality
    ASSERT_STR_EQ("inheritance_value", read_op.value());
    END_TEST("read_operation_inheritance")
}

int main() {
    std::vector<std::pair<std::string, TestFunction>> tests = {
        {"read_operation_constructor", test_read_operation_constructor},
        {"read_operation_value_access", test_read_operation_value_access},
        {"read_operation_notify", test_read_operation_notify},
        {"read_operation_inheritance", test_read_operation_inheritance}};

    run_test_suite("ReadOperation class", tests);

    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;

    if (tests_failed == 0) {
        std::cout << "All ReadOperation tests completed successfully!"
                  << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
