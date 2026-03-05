#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <semaphore>
#include <memory>
#include "keystorage/AbslBtreeKeyStorage.h"
#include "workload/Workload.h"
#if __has_include("request/request_generator.h")
#include "request/request_generator.h"
#elif __has_include("../../loadgen/src/request/request_generator.h")
#include "../../loadgen/src/request/request_generator.h"
#else
#error "loadgen request_generator.h is required"
#endif
#if __has_include("types/types.h")
#include "types/types.h"
#elif __has_include("../../loadgen/src/types/types.h")
#include "../../loadgen/src/types/types.h"
#endif
#include "kvstorage/HardRepartitioningKeyValueStorage.h"
#include "kvstorage/LockStrippingKeyValueStorage.h"
#include "kvstorage/SoftRepartitioningKeyValueStorage.h"
#include "kvstorage/threaded/SoftThreadedRepartitioningKeyValueStorage.h"
#include "kvstorage/threaded/HardThreadedRepartitioningKeyValueStorage.h"
#include "storage/TkrzwTreeStorageEngine.h"
#include "storage/TkrzwHashStorageEngine.h"
#include "storage/LmdbStorageEngine.h"
#include "storage/LevelDBStorageEngine.h"
#include "storage/MapStorageEngine.h"
#include "storage/TbbStorageEngine.h"
#include "keystorage/TkrzwTreeKeyStorage.h"
#include "keystorage/TkrzwHashKeyStorage.h"
#include "keystorage/LmdbKeyStorage.h"
#include "keystorage/UnorderedDenseKeyStorage.h"
#include "repart_kv_api.h"
#include <cassert>
#include <barrier>
#include <random>
#include <boost/lockfree/spsc_queue.hpp>

const size_t QUEUE_CAPACITY = 10000000;
// Global parameters
size_t PARTITION_COUNT = 4;
size_t TEST_WORKERS = 1;
std::string STORAGE_TYPE = "soft";         // Default to soft repartitioning
std::string STORAGE_ENGINE = "tkrzw_tree"; // Default to TkrzwTreeStorageEngine
std::vector<std::string> STORAGE_PATHS = {
    "/tmp"}; // Default paths for embedded database files
std::string LOADGEN_CONFIG_FILE;
std::string WORKLOAD_NAME;
// Repartitioning parameters
std::chrono::milliseconds TRACKING_DURATION(
    100); // Duration to track key accesses before repartitioning
std::chrono::milliseconds
    REPARTITION_INTERVAL(100); // Interval between repartitioning cycles
std::chrono::nanoseconds THINKING_TIME(0); // Thinking time delay (ns)
long THINKING_SEED = 0;                    // Thinking seed

std::chrono::high_resolution_clock::time_point **START_TIMES = nullptr;
std::chrono::high_resolution_clock::time_point **END_TIMES = nullptr;
size_t *TIMES_INDEX = nullptr;

/**
 * @brief Operation with start timestamp for open-loop producer-consumer model
 */
struct QueuedOperation {
    workload::Operation op;
    std::chrono::high_resolution_clock::time_point start_time;
};

/**
 * @brief Thread-safe blocking queue for producer-consumer
 */
template <typename T, size_t C> class BlockingSPSCQueue {
    boost::lockfree::spsc_queue<T> q_;
    std::counting_semaphore<C> available_sem_;
    bool done_ = false;

public:
    BlockingSPSCQueue() : q_(C), available_sem_(0) {}
    void wait() { available_sem_.acquire(); }
    void push(T item) {
        q_.push(std::move(item));
        available_sem_.release();
    }
    T try_pop() {
        T value = q_.front();
        q_.pop();
        return value;
    }
    void set_done() {
        done_ = true;
        available_sem_.release();
    }

    bool done() const { return done_; }

    size_t size() const { return q_.read_available(); }
};

/**
 * @brief Write operation latency data (start,end pairs) to CSV after experiment
 * ends. Pairs are sorted by start time; times are relative to experiment start.
 * @param metrics_file Base metrics filename (same naming, .csv replaced with
 * _latency.csv)
 * @param experiment_start Reference time point for relative timestamps
 * @param test_workers Number of workers
 */
void output_latency_csv(
    const std::string &metrics_file,
    std::chrono::high_resolution_clock::time_point experiment_start,
    size_t test_workers) {
    if (START_TIMES == nullptr || END_TIMES == nullptr ||
        TIMES_INDEX == nullptr) {
        return;
    }

    std::string latency_file = metrics_file;
    const size_t csv_pos = latency_file.rfind(".csv");
    if (csv_pos != std::string::npos) {
        latency_file = latency_file.substr(0, csv_pos) + "__latency.csv";
    } else {
        latency_file += "__latency.csv";
    }

    std::ofstream file(latency_file);
    if (!file.is_open()) {
        std::cerr << "Warning: Failed to open latency file: " << latency_file
                  << std::endl;
        return;
    }

    file << "start_ns,end_ns" << std::endl;

    // K-way merge: each worker's array is sorted by start time; use counters
    // to merge by repeatedly picking the smallest start among current heads
    std::vector<size_t> worker_idx(test_workers, 0);
    size_t total = 0;
    for (size_t w = 0; w < test_workers; ++w) {
        total += TIMES_INDEX[w];
    }

    for (size_t written = 0; written < total; ++written) {
        size_t best_worker = test_workers;
        std::chrono::high_resolution_clock::time_point best_start{};

        for (size_t w = 0; w < test_workers; ++w) {
            if (worker_idx[w] < TIMES_INDEX[w]) {
                auto s = START_TIMES[w][worker_idx[w]];
                if (best_worker == test_workers || s < best_start) {
                    best_worker = w;
                    best_start = s;
                }
            }
        }

        if (best_worker == test_workers) {
            break;
        }

        auto start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            START_TIMES[best_worker][worker_idx[best_worker]] -
                            experiment_start)
                            .count();
        auto end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          END_TIMES[best_worker][worker_idx[best_worker]] -
                          experiment_start)
                          .count();
        file << start_ns << "," << end_ns << "\n";
        worker_idx[best_worker]++;
    }
    file.flush();
    file.close();

    std::cout << "Latency data saved to: " << latency_file << std::endl;
}

/**
 * @brief Format a number with thousand separators (dots)
 * @param value The number to format
 * @return Formatted string with thousand separators
 */
std::string format_with_separators(size_t value) {
    std::string str = std::to_string(value);
    std::string result;

    // Add thousand separators from right to left
    for (size_t i = 0; i < str.length(); ++i) {
        if (i > 0 && (str.length() - i) % 3 == 0) {
            result += ".";
        }
        result += str[i];
    }

    return result;
}

/**
 * @brief Format a double with thousand separators (dots) and decimal places
 * @param value The double value to format
 * @param precision Number of decimal places (default: 2)
 * @return Formatted string with thousand separators and comma decimal separator
 */
std::string format_with_separators(double value, int precision = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    std::string str = oss.str();

    // Find the decimal point
    size_t decimal_pos = str.find('.');
    if (decimal_pos == std::string::npos) {
        decimal_pos = str.length();
    }

    std::string result;
    std::string integer_part = str.substr(0, decimal_pos);
    std::string decimal_part = str.substr(decimal_pos);

    // Add thousand separators to integer part
    for (size_t i = 0; i < integer_part.length(); ++i) {
        if (i > 0 && (integer_part.length() - i) % 3 == 0) {
            result += ".";
        }
        result += integer_part[i];
    }

    // Replace decimal point with comma
    if (decimal_part.length() > 0) {
        decimal_part[0] = ',';
    }

    return result + decimal_part;
}

/**
 * @brief Format a long long with thousand separators (dots)
 * @param value The long long value to format
 * @return Formatted string with thousand separators
 */
std::string format_with_separators(long long value) {
    return format_with_separators(static_cast<size_t>(value));
}

/**
 * @brief Format a std::chrono::milliseconds::rep with thousand separators
 * (dots)
 * @param value The milliseconds count to format
 * @return Formatted string with thousand separators
 */
template <typename Rep> std::string format_with_separators(Rep value) {
    return format_with_separators(static_cast<size_t>(value));
}

// Simple approach: try to construct with different signatures and use SFINAE
template <typename T>
auto try_construct_repartitioning(T *, size_t partition_count,
                                  const std::vector<std::string> &paths)
    -> decltype(T(partition_count, std::hash<std::string>(), TRACKING_DURATION,
                  REPARTITION_INTERVAL, paths)) {
    return T(partition_count, std::hash<std::string>(), TRACKING_DURATION,
             REPARTITION_INTERVAL, paths);
}

template <typename T>
auto try_construct_partitioned(T *, size_t partition_count,
                               const std::vector<std::string> &paths)
    -> decltype(T(partition_count, std::hash<std::string>(), paths)) {
    return T(partition_count, std::hash<std::string>(), paths);
}

template <typename T>
auto try_construct_storage_engine(T *, size_t level,
                                  const std::vector<std::string> &paths)
    -> decltype(T(level, paths.empty() ? "/tmp" : paths[0])) {
    return T(level, paths.empty() ? "/tmp" : paths[0]);
}

template <typename T> auto try_construct_default(T *) -> decltype(T()) {
    return T();
}

/**
 * @brief Get current memory usage in KB
 * @return Memory usage in KB, or 0 if unable to determine
 */
size_t get_memory_usage_kb() {
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            // Extract the number from the line
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                return std::stoull(line.substr(pos));
            }
        }
    }
    return 0;
}

/**
 * @brief Get current disk usage of the current directory in KB
 * @return Disk usage in KB
 */
size_t get_disk_usage_kb() {
    size_t total_size = 0;
    try {
        for (const auto &entry :
             std::filesystem::recursive_directory_iterator(".")) {
            if (entry.is_regular_file()) {
                total_size += entry.file_size();
            }
        }
    } catch (const std::exception &e) {
        // Silently ignore errors (e.g., permission denied)
    }
    return total_size / 1024; // Convert bytes to KB
}

/**
 * @brief Metrics logging loop that runs in a separate thread
 * @param executed_counts Array of counters (one per worker)
 * @param running Atomic flag to control the loop
 * @param output_file Path to the output CSV file
 * @param storage Reference to the storage instance for tracking status
 * @param start_barrier Barrier to wait for before starting the metrics logging
 */
template <typename StorageType>
void metrics_loop(const std::vector<size_t> &executed_counts,
                  const std::atomic<bool> &running,
                  const std::string &output_file, StorageType &storage,
                  std::barrier<> &start_barrier) {
    std::ofstream file(output_file);
    if (!file.is_open()) {
        std::cerr << "Warning: Failed to open metrics file: " << output_file
                  << std::endl;
        exit(1);
        return;
    }

    start_barrier.arrive_and_wait();
    std::chrono::high_resolution_clock::time_point start_time =
        std::chrono::high_resolution_clock::now();

    // Write CSV header
    file << "elapsed_time_ms,executed_count,memory_kb,disk_kb,Tracking,"
            "Repartitioning"
         << std::endl;

    // Track previous tracking state to detect transitions
    bool prev_tracking_enabled = false;

    while (running) {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time);

        // Sum all worker counts (no synchronization needed - approximate values
        // are fine)
        size_t count = 0;
        for (size_t worker_count : executed_counts) {
            count += worker_count;
        }

        size_t memory_kb = get_memory_usage_kb();
        size_t disk_kb = get_disk_usage_kb();

        // Get current tracking and repartitioning status
        bool current_tracking_enabled = false;
        bool current_repartitioning = false;

        // Check if storage has repartitioning methods
        // (RepartitioningKeyValueStorage)
        if constexpr (requires {
                          storage.enable_tracking();
                          storage.is_repartitioning();
                      }) {
            current_tracking_enabled = storage.enable_tracking();
            current_repartitioning = storage.is_repartitioning();
        }
        // For StorageEngine types, tracking and repartitioning are not
        // applicable

        // Determine repartitioning status: "o" if repartitioning flag is set OR
        // tracking was disabled
        char repartitioning_status = 'x';
        if (current_repartitioning ||
            (prev_tracking_enabled && !current_tracking_enabled)) {
            repartitioning_status = 'o';
        }

        // Write metrics to CSV
        file << format_with_separators(elapsed.count()) << ","
             << format_with_separators(count) << ","
             << format_with_separators(memory_kb) << ","
             << format_with_separators(disk_kb) << ","
             << (current_tracking_enabled ? 'o' : 'x') << ","
             << repartitioning_status << std::endl;

        // Update previous tracking state
        prev_tracking_enabled = current_tracking_enabled;

        // Flush to ensure data is written
        file.flush();

        // Sleep for 100 milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    file.close();
}

template <typename StorageType>
void execute_operation(const workload::Operation &op, StorageType &storage) {
    switch (op.type) {
        case workload::OperationType::READ: {
            std::string value;
            Status status = storage.read(op.key, value);
            if (status != Status::SUCCESS) {
                std::cerr << "Error: Failed to read key: " << op.key
                          << std::endl;
                exit(1);
            }
            break;
        }
        case workload::OperationType::WRITE: {
            const std::string *value_ptr =
                op.value.empty() ? &workload::DEFAULT_VALUE : &op.value;
            Status status = storage.write(op.key, *value_ptr);
            if (status != Status::SUCCESS) {
                std::cerr << "Error: Failed to write key: " << op.key
                          << std::endl;
                exit(1);
            }
            break;
        }
        case workload::OperationType::SCAN: {
            std::vector<std::pair<std::string, std::string>> results;
            Status status = storage.scan(op.key, op.limit, results);
            if (status != Status::SUCCESS) {
                std::cerr << "Error: Failed to scan key: " << op.key
                          << std::endl;
                exit(1);
            }
            break;
        }
    }
}

static workload::Operation
make_operation_from_request(loadgen::types::Type type, long key,
                            const std::string &value, long scan_size) {
    std::string key_str = std::to_string(key);
    switch (type) {
        case loadgen::types::Type::READ: {
            return workload::Operation(workload::OperationType::READ, key_str);
        }
        case loadgen::types::Type::WRITE: {
            workload::Operation operation(workload::OperationType::WRITE,
                                          key_str);
            operation.value = value;
            return operation;
        }
        case loadgen::types::Type::SCAN: {
            size_t limit = scan_size < 0 ? 0 : static_cast<size_t>(scan_size);
            return workload::Operation(workload::OperationType::SCAN, key_str,
                                       limit);
        }
        default:
            return workload::Operation(workload::OperationType::READ, key_str);
    }
}

/**
 * @brief Producer thread: "thinks" (exponential sleep), then pushes operation
 *        with start timestamp to queue. Open-loop: request rate independent of
 *        service time.
 */
void producer_function(
    size_t pair_id, workload::RequestGenerator &generator,
    BlockingSPSCQueue<QueuedOperation, QUEUE_CAPACITY> &queue,
    std::barrier<> &start_barrier) {
    loadgen::types::Type type;
    long key;
    std::string value;
    long scan_size;

    if (generator.current_phase() ==
        workload::RequestGenerator::Phase::LOADING) {
        generator.skip_current_phase();
    }

    assert(generator.current_phase() ==
           workload::RequestGenerator::Phase::OPERATIONS);
    std::vector<workload::Operation> operations;
    workload::RequestGenerator::Phase phase =
        generator.next(type, key, value, scan_size);
    while (phase == workload::RequestGenerator::Phase::OPERATIONS) {
        operations.push_back(
            make_operation_from_request(type, key, value, scan_size));
        phase = generator.next(type, key, value, scan_size);
    }

    start_barrier.arrive_and_wait();

    std::mt19937 rng(
        static_cast<std::mt19937::result_type>(pair_id + THINKING_SEED));

    if (THINKING_TIME.count() > 0) {
        auto interval_begin = std::chrono::high_resolution_clock::now();
        std::exponential_distribution<double> thinking_dist(
            1.0 / static_cast<double>(THINKING_TIME.count()));
        for (const auto &operation : operations) {
            double delay_ns = thinking_dist(rng);
            auto target = std::chrono::duration<double, std::nano>(delay_ns);
            auto curr_time = std::chrono::high_resolution_clock::now();
            while (curr_time - interval_begin < target) {
            }
            queue.push(QueuedOperation{operation, curr_time});
            interval_begin = curr_time;
        }
    } else {
        for (const auto &operation : operations) {
            auto start_time = std::chrono::high_resolution_clock::now();
            queue.push(QueuedOperation{operation, start_time});
        }
    }
    queue.set_done();
}

/**
 * @brief Consumer thread: pops from queue, executes operation, records end
 * time, saves (start, end) for latency.
 */
template <typename StorageType> void
consumer_function(size_t pair_id,
                  BlockingSPSCQueue<QueuedOperation, QUEUE_CAPACITY> &queue,
                  StorageType &storage, std::vector<size_t> &executed_counts,
                  std::barrier<> &start_barrier) {
    start_barrier.arrive_and_wait();

    while (true) {
        queue.wait();
        if (queue.size() == 0 && queue.done()) {
            break;
        }
        QueuedOperation item = queue.try_pop();
        execute_operation(item.op, storage);
        auto op_end = std::chrono::high_resolution_clock::now();
        executed_counts[pair_id]++;
        START_TIMES[pair_id][TIMES_INDEX[pair_id]] = item.start_time;
        END_TIMES[pair_id][TIMES_INDEX[pair_id]] = op_end;
        TIMES_INDEX[pair_id]++;
    }
}

// Template function to run workload with any RepartitioningKeyValueStorage
// implementation
template <typename StorageType> void run_workload_with_storage(
    std::vector<std::unique_ptr<workload::RequestGenerator>> &generators,
    size_t partition_count, size_t test_workers,
    const std::string &storage_type_name) {
    // Create storage instance
    std::cout << "\n=== Initializing Storage ===" << std::endl;

    StorageType storage = [&]() -> StorageType {
        // Try RepartitioningKeyValueStorage constructor first
        if constexpr (requires {
                          try_construct_repartitioning(
                              static_cast<StorageType *>(nullptr),
                              partition_count, STORAGE_PATHS);
                      }) {
            std::cout << "Created " << storage_type_name << " with "
                      << partition_count << " partitions (Tkrzw)" << std::endl;
            std::cout << "Tracking duration: " << TRACKING_DURATION.count()
                      << "ms, Repartition interval: "
                      << REPARTITION_INTERVAL.count() << "ms" << std::endl;
            return try_construct_repartitioning(
                static_cast<StorageType *>(nullptr), partition_count,
                STORAGE_PATHS);
        }
        // Try PartitionedKeyValueStorage constructor (e.g. LockStripping)
        else if constexpr (requires {
                               try_construct_partitioned(
                                   static_cast<StorageType *>(nullptr),
                                   partition_count, STORAGE_PATHS);
                           }) {
            std::cout << "Created " << storage_type_name << " with "
                      << partition_count << " partitions (PartitionedStorage)"
                      << std::endl;
            return try_construct_partitioned(
                static_cast<StorageType *>(nullptr), partition_count,
                STORAGE_PATHS);
        }
        // Try StorageEngine constructor
        else if constexpr (requires {
                               try_construct_storage_engine(
                                   static_cast<StorageType *>(nullptr), 0,
                                   STORAGE_PATHS);
                           }) {
            std::cout << "Created " << storage_type_name << " (StorageEngine)"
                      << std::endl;
            return try_construct_storage_engine(
                static_cast<StorageType *>(nullptr), 0, STORAGE_PATHS);
        }
        // Fallback to default constructor
        else {
            std::cout << "Created " << storage_type_name
                      << " (other type - using default constructor)"
                      << std::endl;
            return try_construct_default(static_cast<StorageType *>(nullptr));
        }
    }();

    // Setup metrics tracking
    std::vector<size_t> executed_counts(test_workers,
                                        0); // One counter per worker
    std::atomic<bool> metrics_running(true);

    std::string workload_filename =
        WORKLOAD_NAME.empty() ? "loadgen" : WORKLOAD_NAME;

    // Create metrics filename:
    // workload__testworkers__storagetype__partitions__storageengine__paths.csv
    std::string metrics_file =
        workload_filename + "__" + std::to_string(test_workers) + "__" +
        STORAGE_TYPE + "__" + std::to_string(partition_count) + "__" +
        STORAGE_ENGINE + "__" + std::to_string(STORAGE_PATHS.size()) + "__" +
        std::to_string(REPARTITION_INTERVAL.count()) + "__" +
        std::to_string(THINKING_TIME.count()) + ".csv";

    // Execute operations
    std::cout << "\n=== Executing Workload ===" << std::endl;

    // Execute preload
    std::cout << "Preload: executing preload operations... " << std::flush;
    size_t preload = 0;

    auto &primary_generator = generators[0];

    loadgen::types::Type type;
    long key;
    std::string value;
    long scan_size;
    workload::RequestGenerator::Phase phase =
        primary_generator->next(type, key, value, scan_size);
    while (phase == workload::RequestGenerator::Phase::LOADING) {
        auto operation =
            make_operation_from_request(type, key, value, scan_size);
        execute_operation(operation, storage);
        preload++;
        phase = primary_generator->next(type, key, value, scan_size);
    }

    assert(primary_generator->current_phase() ==
           workload::RequestGenerator::Phase::OPERATIONS);
    std::cout << " [DONE]" << std::endl;

    // Open-loop: N producer-consumer pairs, each with its own queue
    std::vector<BlockingSPSCQueue<QueuedOperation, QUEUE_CAPACITY>> queues(
        test_workers);
    std::barrier start_barrier(2 * test_workers + 2);

    // Start metrics logging thread
    std::thread metrics_thread(metrics_loop<StorageType>,
                               std::ref(executed_counts),
                               std::ref(metrics_running), metrics_file,
                               std::ref(storage), std::ref(start_barrier));

    // Create producer and consumer threads
    std::vector<std::thread> producer_threads;
    std::vector<std::thread> consumer_threads;
    producer_threads.reserve(test_workers);
    consumer_threads.reserve(test_workers);

    for (size_t i = 0; i < test_workers; ++i) {
        producer_threads.emplace_back(
            producer_function, i, std::ref(*generators[i]), std::ref(queues[i]),
            std::ref(start_barrier));
        consumer_threads.emplace_back(consumer_function<StorageType>, i,
                                      std::ref(queues[i]), std::ref(storage),
                                      std::ref(executed_counts),
                                      std::ref(start_barrier));
    }

    std::cout << "Loading workload into memory... " << std::flush;
    start_barrier.arrive_and_wait();
    std::cout << " [DONE]" << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "Executing workload (open-loop)... " << std::flush;
    for (auto &thread : producer_threads) {
        thread.join();
    }
    for (auto &thread : consumer_threads) {
        thread.join();
    }
    std::cout << " [DONE]" << std::endl;

    // Stop metrics logging
    metrics_running = false;
    metrics_thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Calculate and display results
    size_t total_operations = 0;
    for (size_t count : executed_counts) {
        total_operations += count;
    }

    double operations_per_second =
        static_cast<double>(total_operations) / (duration.count() / 1000.0);

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Storage type: " << storage_type_name << std::endl;
    if (preload > 0) {
        std::cout << "Preload operations: " << format_with_separators(preload)
                  << std::endl;
    }
    std::cout << "Total operations executed (excluding preload): "
              << format_with_separators(total_operations) << std::endl;
    std::cout << "Duration: " << format_with_separators(duration.count())
              << " ms" << std::endl;
    std::cout << "Operations per second: "
              << format_with_separators(operations_per_second, 2) << std::endl;
    std::cout << "Metrics saved to: " << metrics_file << std::endl;

    output_latency_csv(metrics_file, start_time, test_workers);

    // Display per-worker statistics
    std::cout << "\nPer-worker statistics:" << std::endl;
    for (size_t i = 0; i < executed_counts.size(); ++i) {
        std::cout << "  Worker " << i << ": "
                  << format_with_separators(executed_counts[i]) << " operations"
                  << std::endl;
    }
}

/**
 * @brief Helper function to execute workload with storage engine configuration
 */
void execute_with_storage_config(
    std::vector<std::unique_ptr<workload::RequestGenerator>> &generators) {
    if (STORAGE_ENGINE == "tkrzw_tree") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<TkrzwTreeStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "soft") {
            using StorageType =
                SoftRepartitioningKeyValueStorage<TkrzwTreeStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType = SoftThreadedRepartitioningKeyValueStorage<
                TkrzwTreeStorageEngine, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                TkrzwTreeStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = TkrzwTreeStorageEngine;
            run_workload_with_storage<StorageType>(generators, PARTITION_COUNT,
                                                   TEST_WORKERS,
                                                   "TkrzwTreeStorageEngine");
        } else if (STORAGE_TYPE == "lock_stripping") {
            using StorageType =
                LockStrippingKeyValueStorage<TkrzwTreeStorageEngine>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "LockStrippingKeyValueStorage<TkrzwTreeStorageEngine>");
        }
    } else if (STORAGE_ENGINE == "tkrzw_hash") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<TkrzwHashStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "soft") {
            using StorageType =
                SoftRepartitioningKeyValueStorage<TkrzwHashStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType = SoftThreadedRepartitioningKeyValueStorage<
                TkrzwHashStorageEngine, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                TkrzwHashStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = TkrzwHashStorageEngine;
            run_workload_with_storage<StorageType>(generators, PARTITION_COUNT,
                                                   TEST_WORKERS,
                                                   "TkrzwHashStorageEngine");
        } else if (STORAGE_TYPE == "lock_stripping") {
            using StorageType =
                LockStrippingKeyValueStorage<TkrzwHashStorageEngine>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "LockStrippingKeyValueStorage<TkrzwHashStorageEngine>");
        }
    } else if (STORAGE_ENGINE == "lmdb") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<LmdbStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "soft") {
            using StorageType = SoftRepartitioningKeyValueStorage<
                LmdbStorageEngine, AbslBtreeKeyStorage, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType =
                SoftThreadedRepartitioningKeyValueStorage<LmdbStorageEngine,
                                                          AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                LmdbStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = LmdbStorageEngine;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS, "LmdbStorageEngine");
        } else if (STORAGE_TYPE == "lock_stripping") {
            using StorageType = LockStrippingKeyValueStorage<LmdbStorageEngine>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "LockStrippingKeyValueStorage<LmdbStorageEngine>");
        }
    } else if (STORAGE_ENGINE == "leveldb") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<LevelDBStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "soft") {
            using StorageType = SoftRepartitioningKeyValueStorage<
                LevelDBStorageEngine, AbslBtreeKeyStorage, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType =
                SoftThreadedRepartitioningKeyValueStorage<LevelDBStorageEngine,
                                                          AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                LevelDBStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = LevelDBStorageEngine;
            run_workload_with_storage<StorageType>(generators, PARTITION_COUNT,
                                                   TEST_WORKERS,
                                                   "LevelDBStorageEngine");
        } else if (STORAGE_TYPE == "lock_stripping") {
            using StorageType =
                LockStrippingKeyValueStorage<LevelDBStorageEngine>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "LockStrippingKeyValueStorage<LevelDBStorageEngine>");
        }
    } else if (STORAGE_ENGINE == "map") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<MapStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "soft") {
            using StorageType = SoftRepartitioningKeyValueStorage<
                MapStorageEngine, AbslBtreeKeyStorage, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType =
                SoftThreadedRepartitioningKeyValueStorage<MapStorageEngine,
                                                          AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                MapStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = MapStorageEngine;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS, "MapStorageEngine");
        } else if (STORAGE_TYPE == "lock_stripping") {
            using StorageType = LockStrippingKeyValueStorage<MapStorageEngine>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "LockStrippingKeyValueStorage<MapStorageEngine>");
        }
    } else if (STORAGE_ENGINE == "tbb") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<TbbStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "soft") {
            using StorageType = SoftRepartitioningKeyValueStorage<
                TbbStorageEngine, AbslBtreeKeyStorage, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType =
                SoftThreadedRepartitioningKeyValueStorage<TbbStorageEngine,
                                                          AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                TbbStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage");
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = TbbStorageEngine;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS, "TbbStorageEngine");
        } else if (STORAGE_TYPE == "lock_stripping") {
            using StorageType = LockStrippingKeyValueStorage<TbbStorageEngine>;
            run_workload_with_storage<StorageType>(
                generators, PARTITION_COUNT, TEST_WORKERS,
                "LockStrippingKeyValueStorage<TbbStorageEngine>");
        }
    }
}

/**
 * @brief Print usage information
 */
void print_usage(const char *program_name) {
    std::cout << "Usage: " << program_name
              << " <loadgen_config.toml> [partition_count] [test_workers] "
                 "[storage_type] [storage_engine] "
                 "[thinking_time_ns] [storage_paths] "
                 "[repartition_interval_ms]"
              << std::endl;
    std::cout << "\nArguments:" << std::endl;
    std::cout << "  loadgen_config   Path to a LoadGen TOML configuration file "
                 "(used for all workers)"
              << std::endl;
    std::cout << "  partition_count  Number of partitions (default: 4)"
              << std::endl;
    std::cout << "  test_workers     Number of worker threads (default: 1)"
              << std::endl;
    std::cout << "  storage_type     Storage implementation: 'hard', 'soft', "
                 "'threaded', 'hard_threaded', 'engine', or 'lock_stripping' "
                 "(default: soft)"
              << std::endl;
    std::cout << "  storage_engine   Storage engine backend: 'tkrzw_tree', "
                 "'tkrzw_hash', "
                 "'lmdb', 'leveldb', 'map', or 'tbb' (default: tkrzw_tree)"
              << std::endl;
    std::cout
        << "  thinking_time_ns Thinking time delay in nanoseconds (default: 0)"
        << std::endl;
    std::cout
        << "  storage_paths    Comma-separated paths for embedded database "
           "files "
           "(default: /tmp). Multiple paths distribute partitions across them."
        << std::endl;
    std::cout
        << "  repartition_interval_ms  Interval in milliseconds between "
           "repartitioning cycles and tracking duration (default: 1000). "
           "Sets both TRACKING_DURATION and REPARTITION_INTERVAL to this value."
        << std::endl;
    std::cout << "\nStorage Types:" << std::endl;
    std::cout << "  hard            HardRepartitioningKeyValueStorage (creates "
                 "new storage engines)"
              << std::endl;
    std::cout << "  soft            SoftRepartitioningKeyValueStorage (uses "
                 "single storage with partition locks)"
              << std::endl;
    std::cout << "  threaded        SoftThreadedRepartitioningKeyValueStorage "
                 "(threaded soft repartitioning)"
              << std::endl;
    std::cout << "  hard_threaded   HardThreadedRepartitioningKeyValueStorage "
                 "(hard repartitioning with worker threads)"
              << std::endl;
    std::cout
        << "  engine          Direct StorageEngine usage (no repartitioning)"
        << std::endl;
    std::cout
        << "  lock_stripping  LockStrippingKeyValueStorage (partitioned with "
           "lock striping, no repartitioning)"
        << std::endl;
    std::cout << "\nStorage Engines:" << std::endl;
    std::cout
        << "  tkrzw_tree      TkrzwTreeStorageEngine (sorted key-value storage)"
        << std::endl;
    std::cout << "  tkrzw_hash      TkrzwHashStorageEngine (hash-based storage)"
              << std::endl;
    std::cout << "  lmdb            LmdbStorageEngine (LMDB-based storage)"
              << std::endl;
    std::cout
        << "  leveldb         LevelDBStorageEngine (LevelDB LSM-tree storage)"
        << std::endl;
    std::cout
        << "  map             MapStorageEngine (in-memory std::map storage)"
        << std::endl;
    std::cout << "  tbb             TbbStorageEngine (in-memory TBB "
                 "concurrent_hash_map)"
              << std::endl;
    std::cout << "\nWorkload file format:" << std::endl;
    std::cout << "  0,<key>         : READ operation" << std::endl;
    std::cout << "  1,<key>         : WRITE operation (uses 1KB default value)"
              << std::endl;
    std::cout << "  2,<key>,<limit> : SCAN operation" << std::endl;
}

int run_repart_kv(int argc, char *argv[]) {
    // Parse command-line arguments
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    LOADGEN_CONFIG_FILE = argv[1];
    if (!std::filesystem::exists(LOADGEN_CONFIG_FILE)) {
        std::cerr << "Error: LoadGen config file does not exist: "
                  << LOADGEN_CONFIG_FILE << std::endl;
        return 1;
    }

    if (argc >= 3) {
        try {
            PARTITION_COUNT = std::stoull(argv[2]);
            if (PARTITION_COUNT == 0) {
                std::cerr << "Error: partition_count must be greater than 0"
                          << std::endl;
                return 1;
            }
        } catch (const std::exception &e) {
            std::cerr << "Error: Invalid partition_count: " << argv[2]
                      << std::endl;
            return 1;
        }
    }

    if (argc >= 4) {
        try {
            TEST_WORKERS = std::stoull(argv[3]);
            if (TEST_WORKERS == 0) {
                std::cerr << "Error: test_workers must be greater than 0"
                          << std::endl;
                return 1;
            }
        } catch (const std::exception &e) {
            std::cerr << "Error: Invalid test_workers: " << argv[3]
                      << std::endl;
            return 1;
        }
    }

    if (argc >= 5) {
        STORAGE_TYPE = argv[4];
        if (STORAGE_TYPE != "hard" && STORAGE_TYPE != "soft" &&
            STORAGE_TYPE != "threaded" && STORAGE_TYPE != "hard_threaded" &&
            STORAGE_TYPE != "engine" && STORAGE_TYPE != "lock_stripping") {
            std::cerr << "Error: storage_type must be 'hard', 'soft', "
                         "'threaded', 'hard_threaded', 'engine', or "
                         "'lock_stripping', got: "
                      << STORAGE_TYPE << std::endl;
            return 1;
        }
    }

    if (argc >= 6) {
        STORAGE_ENGINE = argv[5];
        if (STORAGE_ENGINE != "tkrzw_tree" && STORAGE_ENGINE != "tkrzw_hash" &&
            STORAGE_ENGINE != "lmdb" && STORAGE_ENGINE != "leveldb" &&
            STORAGE_ENGINE != "map" && STORAGE_ENGINE != "tbb") {
            std::cerr
                << "Error: storage_engine must be 'tkrzw_tree', 'tkrzw_hash', "
                   "'lmdb', 'leveldb', 'map', or 'tbb', got: "
                << STORAGE_ENGINE << std::endl;
            return 1;
        }
    }

    if (argc >= 7) {
        try {
            int64_t thinking_ns = std::stoll(argv[6]);
            THINKING_TIME =
                std::chrono::nanoseconds(thinking_ns >= 0 ? thinking_ns : 0);
        } catch (const std::exception &e) {
            std::cerr << "Error: Invalid thinking_time_ns: " << argv[6]
                      << std::endl;
            return 1;
        }
    }

    if (argc >= 8) {
        std::string paths_str = argv[7];
        STORAGE_PATHS.clear();
        std::stringstream ss(paths_str);
        std::string path;
        while (std::getline(ss, path, ',')) {
            path.erase(0, path.find_first_not_of(" \t"));
            path.erase(path.find_last_not_of(" \t") + 1);
            if (!path.empty()) {
                STORAGE_PATHS.push_back(path);
            }
        }
        if (STORAGE_PATHS.empty()) {
            STORAGE_PATHS = {"/tmp"};
        }
    }

    if (argc >= 9) {
        try {
            int64_t interval_ms = std::stoll(argv[8]);
            REPARTITION_INTERVAL = std::chrono::milliseconds(interval_ms);
            if (interval_ms == 0) {
                TRACKING_DURATION = std::chrono::milliseconds(interval_ms);
            }
        } catch (const std::exception &e) {
            std::cerr << "Error: Invalid repartition_interval_ms: " << argv[8]
                      << std::endl;
            return 1;
        }
    }

    std::filesystem::path config_path(LOADGEN_CONFIG_FILE);
    WORKLOAD_NAME = config_path.stem().string();
    if (WORKLOAD_NAME.empty()) {
        WORKLOAD_NAME = config_path.filename().string();
    }

    std::cout << "=== Repart-KV Workload Executor ===" << std::endl;
    std::cout << "LoadGen config: " << LOADGEN_CONFIG_FILE << std::endl;
    std::cout << "Partition count: " << PARTITION_COUNT << std::endl;
    std::cout << "Test workers: " << TEST_WORKERS << std::endl;
    std::cout << "Storage type: " << STORAGE_TYPE << std::endl;
    std::cout << "Storage engine: " << STORAGE_ENGINE << std::endl;
    std::cout << "Thinking time: " << THINKING_TIME.count() << "ns"
              << std::endl;
    std::cout << "Storage paths: ";
    for (size_t i = 0; i < STORAGE_PATHS.size(); ++i) {
        std::cout << STORAGE_PATHS[i];
        if (i < STORAGE_PATHS.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << std::endl;
    std::cout << "Tracking duration: " << TRACKING_DURATION.count() << "ms"
              << std::endl;
    std::cout << "Repartition interval: " << REPARTITION_INTERVAL.count()
              << "ms" << std::endl;
    std::cout << std::endl;

    std::vector<std::unique_ptr<workload::RequestGenerator>> generators;
    generators.reserve(TEST_WORKERS);
    for (size_t worker = 0; worker < TEST_WORKERS; ++worker) {
        auto generator = std::make_unique<workload::RequestGenerator>(
            LOADGEN_CONFIG_FILE, false);
        auto &config = generator->config();
        config.operation_seed += static_cast<long>(worker);
        config.key_seed += static_cast<long>(worker);
        config.scan_seed += static_cast<long>(worker);
        config.n_operations = config.n_operations / TEST_WORKERS;
        generator->initialize();
        generators.push_back(std::move(generator));
    }

    size_t n_operations = generators[0]->config().n_operations * TEST_WORKERS;
    START_TIMES =
        new std::chrono::high_resolution_clock::time_point *[TEST_WORKERS];
    END_TIMES =
        new std::chrono::high_resolution_clock::time_point *[TEST_WORKERS];
    TIMES_INDEX = new size_t[TEST_WORKERS];

    for (size_t worker = 0; worker < TEST_WORKERS; ++worker) {
        START_TIMES[worker] =
            new std::chrono::high_resolution_clock::time_point[n_operations];
        END_TIMES[worker] =
            new std::chrono::high_resolution_clock::time_point[n_operations];
        TIMES_INDEX[worker] = 0;
    }
    THINKING_SEED = generators[0]->config().operation_seed;

    if (generators.empty()) {
        std::cerr << "Error: No request generators created" << std::endl;
        return 1;
    }

    const auto &summary_config = generators.front()->config();
    std::cout << "Configured workload summary:" << std::endl;
    std::cout << "  Load records: " << summary_config.n_records << std::endl;
    std::cout << "  Operation phase: "
              << summary_config.n_operations * TEST_WORKERS << " operations"
              << std::endl;
    std::cout << "  Data distribution: " << summary_config.data_distribution
              << std::endl;
    std::cout << "  Operation mix weights: " << "READ="
              << summary_config.read_proportion << ", "
              << "UPDATE=" << summary_config.update_proportion << ", "
              << "INSERT=" << summary_config.insert_proportion << ", "
              << "SCAN=" << summary_config.scan_proportion << std::endl;
    std::cout << "  Scan length distribution: "
              << summary_config.scan_length_distribution << " ["
              << summary_config.min_scan_length << "-"
              << summary_config.max_scan_length << "]" << std::endl;

    execute_with_storage_config(generators);

    return 0;
}
