#include "SyncOperation.h"
#include "../../../utils/test_assertions.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_constructor() {
    TEST("constructor")
    SyncOperation sync_op(3);

    // Test that it's properly constructed
    ASSERT_TRUE(sync_op.type() == Type::SYNC);
    ASSERT_TRUE(sync_op.status() == Status::PENDING);
    END_TEST("constructor")
}

void test_type_verification() {
    TEST("type_verification")
    SyncOperation sync_op(2);

    // Verify the operation type is SYNC
    ASSERT_TRUE(sync_op.type() == Type::SYNC);
    END_TEST("type_verification")
}

void test_status_inheritance() {
    TEST("status_inheritance")
    SyncOperation sync_op(2);

    // Test initial status
    ASSERT_TRUE(sync_op.status() == Status::PENDING);

    // Test status modification
    sync_op.status(Status::SUCCESS);
    ASSERT_TRUE(sync_op.status() == Status::SUCCESS);

    sync_op.status(Status::ERROR);
    ASSERT_TRUE(sync_op.status() == Status::ERROR);

    sync_op.status(Status::NOT_FOUND);
    ASSERT_TRUE(sync_op.status() == Status::NOT_FOUND);
    END_TEST("status_inheritance")
}

void test_sync() {
    TEST("test_sync")
    SyncOperation sync_op(2);

    std::thread t1([&sync_op]() { sync_op.sync(); });
    sync_op.sync();
    t1.join();
    END_TEST("test_sync")
}

void test_barrier_destroy() {
    TEST("barrier_destroy")
    SyncOperation sync_op(2);
    std::thread t1([&sync_op]() { sync_op.sync(); });
    sync_op.sync();
    t1.join();

    END_TEST("barrier_destroy")
}

void test_large_partition_count() {
    TEST("large_partition_count")
    const int partition_count = 100;
    SyncOperation sync_op(partition_count);
    std::atomic<int> completion_count{0};

    std::vector<std::thread> threads;

    // Create many threads that will all wait on the barrier
    for (int i = 0; i < partition_count; ++i) {
        threads.emplace_back([&sync_op, &completion_count]() {
            sync_op.sync();
            completion_count++;
        });
    }

    // Wait for all threads to complete
    for (auto &t : threads) {
        t.join();
    }

    ASSERT_EQ(partition_count, completion_count.load());
    END_TEST("large_partition_count")
}

int main() {
    std::cout << "Starting SyncOperation class tests..." << std::endl
              << std::endl;

    test_constructor();
    test_type_verification();
    test_status_inheritance();
    test_sync();
    test_barrier_destroy();
    test_large_partition_count();

    std::cout << std::endl << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;

    if (tests_failed == 0) {
        std::cout << "All SyncOperation tests completed successfully!"
                  << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
