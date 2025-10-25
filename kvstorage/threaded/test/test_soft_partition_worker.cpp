#include "../SoftPartitionWorker.h"
#include "../operation/ReadOperation.h"
#include "../operation/WriteOperation.h"
#include "../../../storage/MapStorageEngine.h"
#include "../../../utils/test_assertions.h"
#include <string>
#include <vector>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

template <size_t Q> using Worker = SoftPartitionWorker<MapStorageEngine, Q>;

void test_stop_signal() {
    TEST("stop_signal")
    MapStorageEngine engine;
    Worker<4> worker(engine);

    // The stop signal should be done on destructor
    // If the test finishes, pass
    ASSERT_TRUE(true);
    END_TEST("stop_signal")
}

void test_single_read_operation() {
    TEST("single_read_operation")
    MapStorageEngine engine;
    std::string key = "k1";
    engine.write(key, "v1");
    Worker<8> worker(engine);

    std::string value;
    ReadOperation read_operation(key, value);
    worker.enqueue(&read_operation);
    read_operation.wait();

    ASSERT_STATUS_EQ(Status::SUCCESS, read_operation.status());
    ASSERT_STR_EQ("v1", value);
    END_TEST("single_read_operation")
}

void test_single_write_operation() {
    TEST("single_write_operation")
    MapStorageEngine engine;
    Worker<8> worker(engine);

    std::string key = "k1";
    std::string value = "v1";
    WriteOperation *write_operation = new WriteOperation(key, value);
    worker.enqueue(write_operation);

    std::string read_value = "";
    ReadOperation read_operation(key, read_value);
    worker.enqueue(&read_operation);
    read_operation.wait();

    ASSERT_STATUS_EQ(Status::SUCCESS, read_operation.status());
    ASSERT_STR_EQ("v1", read_value);
    END_TEST("single_write_operation")
}

void test_single_scan_operation() {
    TEST("single_scan_operation")
    MapStorageEngine engine;
    Worker<8> worker(engine);

    std::string key = "k1";
    engine.write(key, "v1");
    key = "k2";
    engine.write(key, "v2");
    key = "k3";
    engine.write(key, "v3");

    key = "k1";
    std::vector<std::pair<std::string, std::string>> values = {
        {"", ""}, {"", ""}, {"", ""}};
    ScanOperation scan_operation(key, values, 1);
    worker.enqueue(&scan_operation);

    scan_operation.sync();

    ASSERT_STATUS_EQ(Status::SUCCESS, scan_operation.status());
    ASSERT_STR_EQ("k1", values[0].first);
    ASSERT_STR_EQ("v1", values[0].second);
    ASSERT_STR_EQ("k2", values[1].first);
    ASSERT_STR_EQ("v2", values[1].second);
    ASSERT_STR_EQ("k3", values[2].first);
    ASSERT_STR_EQ("v3", values[2].second);
    END_TEST("single_scan_operation")
}

void test_sync_multiple_workers() {
    TEST("sync_multiple_workers")
    MapStorageEngine engine;
    const size_t worker_count = 10;
    std::vector<Worker<8> *> workers;
    for (size_t i = 0; i < worker_count; ++i) {
        workers.push_back(new Worker<8>(engine));
    }

    SyncOperation *sync_operation = new SyncOperation(worker_count);
    for (size_t i = 0; i < worker_count; ++i) {
        workers[i]->enqueue(sync_operation);
    }

    for (size_t i = 0; i < worker_count; ++i) {
        delete workers[i];
    }
    END_TEST("sync_multiple_workers")
}

int main() {
    std::vector<std::pair<std::string, TestFunction>> tests = {
        {"stop_signal", test_stop_signal},
        {"single_read_operation", test_single_read_operation},
        {"single_write_operation", test_single_write_operation},
        {"single_scan_operation", test_single_scan_operation},
        {"sync_multiple_workers", test_sync_multiple_workers}};

    run_test_suite("SoftPartitionWorker", tests);

    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;

    return tests_failed == 0 ? 0 : 1;
}
