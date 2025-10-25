#include "DoneOperation.h"
#include "../../../utils/test_assertions.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_constructor() {
    TEST("constructor")
    DoneOperation done_op;

    // Test that it's properly constructed
    ASSERT_TRUE(done_op.type() == Type::DONE);
    ASSERT_TRUE(done_op.status() == Status::PENDING);
    END_TEST("constructor")
}

void test_type_verification() {
    TEST("type_verification")
    DoneOperation done_op;

    // Verify the operation type is DONE
    ASSERT_TRUE(done_op.type() == Type::DONE);
    END_TEST("type_verification")
}

void test_status_inheritance() {
    TEST("status_inheritance")
    DoneOperation done_op;

    // Test initial status
    ASSERT_TRUE(done_op.status() == Status::PENDING);

    // Test status modification
    done_op.status(Status::SUCCESS);
    ASSERT_TRUE(done_op.status() == Status::SUCCESS);

    done_op.status(Status::ERROR);
    ASSERT_TRUE(done_op.status() == Status::ERROR);

    done_op.status(Status::NOT_FOUND);
    ASSERT_TRUE(done_op.status() == Status::NOT_FOUND);
    END_TEST("status_inheritance")
}

void test_wait() {
    TEST("test_wait")
    DoneOperation done_op;

    // Thread 1: Will wait on the barrier
    std::thread t1([&done_op]() { done_op.wait(); });
    done_op.wait();
    t1.join();
    END_TEST("test_wait")
}

int main() {
    std::vector<std::pair<std::string, TestFunction>> tests = {
        {"constructor", test_constructor},
        {"type_verification", test_type_verification},
        {"status_inheritance", test_status_inheritance}};

    run_test_suite("DoneOperation class", tests);

    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;

    if (tests_failed == 0) {
        std::cout << "All DoneOperation tests completed successfully!"
                  << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
