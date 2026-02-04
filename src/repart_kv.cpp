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
#include "keystorage/AbslBtreeKeyStorage.h"
#include "workload/Workload.h"
#include "kvstorage/HardRepartitioningKeyValueStorage.h"
#include "kvstorage/SoftRepartitioningKeyValueStorage.h"
#include "kvstorage/threaded/SoftThreadedRepartitioningKeyValueStorage.h"
#include "kvstorage/threaded/HardThreadedRepartitioningKeyValueStorage.h"
#include "storage/TkrzwTreeStorageEngine.h"
#include "storage/TkrzwHashStorageEngine.h"
#include "storage/LmdbStorageEngine.h"
#include "storage/MapStorageEngine.h"
#include "storage/TbbStorageEngine.h"
#include "keystorage/TkrzwTreeKeyStorage.h"
#include "keystorage/TkrzwHashKeyStorage.h"
#include "keystorage/LmdbKeyStorage.h"
#include "keystorage/UnorderedDenseKeyStorage.h"
#include "repart_kv_api.h"

// Global parameters
size_t PARTITION_COUNT = 4;
size_t TEST_WORKERS = 1;
size_t WARMUP_OPERATIONS = 0;              // Default to no warmup
std::string STORAGE_TYPE = "soft";         // Default to soft repartitioning
std::string STORAGE_ENGINE = "tkrzw_tree"; // Default to TkrzwTreeStorageEngine
std::vector<std::string> STORAGE_PATHS = {
    "/tmp"}; // Default paths for embedded database files
std::vector<std::string> WORKLOAD_FILES; // Workload file paths
// Repartitioning parameters
std::chrono::milliseconds TRACKING_DURATION(
    1000); // Duration to track key accesses before repartitioning
std::chrono::milliseconds
    REPARTITION_INTERVAL(1000); // Interval between repartitioning cycles

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
 * @param start_time Start time of the execution for calculating elapsed time
 * @param storage Reference to the storage instance for tracking status
 * @param warmup_operations Number of operations to wait for before starting
 * metrics logging
 */
template <typename StorageType>
void metrics_loop(const std::vector<size_t> &executed_counts,
                  const std::atomic<bool> &running,
                  const std::string &output_file,
                  std::chrono::high_resolution_clock::time_point start_time,
                  StorageType &storage) {
    std::ofstream file(output_file);
    if (!file.is_open()) {
        std::cerr << "Warning: Failed to open metrics file: " << output_file
                  << std::endl;
        return;
    }

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

        // Sleep for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
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
            }
            break;
        }
        case workload::OperationType::WRITE: {
            Status status = storage.write(op.key, workload::DEFAULT_VALUE);
            if (status != Status::SUCCESS) {
                std::cerr << "Error: Failed to write key: " << op.key
                          << std::endl;
            }
            break;
        }
        case workload::OperationType::SCAN: {
            std::vector<std::pair<std::string, std::string>> results;
            Status status = storage.scan(op.key, op.limit, results);
            if (status != Status::SUCCESS) {
                std::cerr << "Error: Failed to scan key: " << op.key
                          << std::endl;
            }
            break;
        }
    }
}

/**
 * @brief Worker function that executes a subset of operations
 * @param worker_id ID of this worker (0-indexed)
 * @param operations Reference to the operations vector for this worker
 * @param storage Reference to the storage instance
 * @param executed_counts Array of counters (one per worker, no synchronization
 * needed)
 */
template <typename StorageType>
void worker_function(size_t worker_id,
                     const std::vector<workload::Operation> &operations,
                     StorageType &storage,
                     std::vector<size_t> &executed_counts) {

    for (const auto &op : operations) {
        execute_operation(op, storage);
        executed_counts[worker_id]++;
    }
}

// Template function to run workload with any RepartitioningKeyValueStorage
// implementation
template <typename StorageType> void run_workload_with_storage(
    const std::vector<std::vector<workload::Operation>> &all_worker_operations,
    size_t partition_count, size_t test_workers,
    const std::string &storage_type_name, size_t warmup_operations) {
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

    // Extract filename from the first workload_file path for the metrics file
    std::string workload_filename = WORKLOAD_FILES[0];
    size_t last_slash = workload_filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        workload_filename = workload_filename.substr(last_slash + 1);
    }
    // Remove extension if present
    size_t last_dot = workload_filename.find_last_of(".");
    if (last_dot != std::string::npos) {
        workload_filename = workload_filename.substr(0, last_dot);
    }

    // Create metrics filename:
    // workload__testworkers__storagetype__partitions__storageengine__paths.csv
    std::string metrics_file =
        workload_filename + "__" + std::to_string(test_workers) + "__" +
        STORAGE_TYPE + "__" + std::to_string(partition_count) + "__" +
        STORAGE_ENGINE + "__" + std::to_string(STORAGE_PATHS.size()) + "__" +
        std::to_string(TRACKING_DURATION.count()) + ".csv";

    // Execute operations
    std::cout << "\n=== Executing Workload ===" << std::endl;

    // Handle warmup for each worker
    if (warmup_operations > 0) {
        std::cout << "Warming up: executing "
                  << format_with_separators(warmup_operations)
                  << " operations per worker before starting experiment..."
                  << std::endl;

        for (size_t i = 0; i < test_workers; ++i) {
            const auto &ops = all_worker_operations[i];
            size_t actual_warmup = std::min(warmup_operations, ops.size());
            for (size_t j = 0; j < actual_warmup; ++j) {
                execute_operation(ops[j], storage);
            }
        }

        std::cout << "Warmup complete." << std::endl;
    }

    // Prepare experiment operations (after warmup) for each worker
    std::vector<std::vector<workload::Operation>>
        experiment_operations_per_worker;
    experiment_operations_per_worker.reserve(test_workers);
    for (size_t i = 0; i < test_workers; ++i) {
        const auto &ops = all_worker_operations[i];
        if (warmup_operations < ops.size()) {
            experiment_operations_per_worker.emplace_back(
                ops.begin() + warmup_operations, ops.end());
        } else {
            experiment_operations_per_worker.emplace_back();
        }
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Start metrics logging thread
    std::thread metrics_thread(
        metrics_loop<StorageType>, std::ref(executed_counts),
        std::ref(metrics_running), metrics_file, start_time, std::ref(storage));

    // Create and start worker threads
    std::vector<std::thread> worker_threads;
    worker_threads.reserve(test_workers);

    for (size_t i = 0; i < test_workers; ++i) {
        worker_threads.emplace_back(
            worker_function<StorageType>, i,
            std::ref(experiment_operations_per_worker[i]), std::ref(storage),
            std::ref(executed_counts));
    }

    // Wait for all worker threads to complete
    for (auto &thread : worker_threads) {
        thread.join();
    }

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
    if (warmup_operations > 0) {
        std::cout << "Warmup operations: "
                  << format_with_separators(warmup_operations) << std::endl;
    }
    std::cout << "Total operations executed (excluding warmup): "
              << format_with_separators(total_operations) << std::endl;
    std::cout << "Duration: " << format_with_separators(duration.count())
              << " ms" << std::endl;
    std::cout << "Operations per second: "
              << format_with_separators(operations_per_second, 2) << std::endl;
    std::cout << "Metrics saved to: " << metrics_file << std::endl;

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
    const std::vector<std::vector<workload::Operation>> &operations) {
    size_t warmup_operations = WARMUP_OPERATIONS;
    if (STORAGE_ENGINE == "tkrzw_tree") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<TkrzwTreeStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "soft") {
            using StorageType =
                SoftRepartitioningKeyValueStorage<TkrzwTreeStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType = SoftThreadedRepartitioningKeyValueStorage<
                TkrzwTreeStorageEngine, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                TkrzwTreeStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = TkrzwTreeStorageEngine;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "TkrzwTreeStorageEngine", warmup_operations);
        }
    } else if (STORAGE_ENGINE == "tkrzw_hash") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<TkrzwHashStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "soft") {
            using StorageType =
                SoftRepartitioningKeyValueStorage<TkrzwHashStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType = SoftThreadedRepartitioningKeyValueStorage<
                TkrzwHashStorageEngine, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                TkrzwHashStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = TkrzwHashStorageEngine;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "TkrzwHashStorageEngine", warmup_operations);
        }
    } else if (STORAGE_ENGINE == "lmdb") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<LmdbStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "soft") {
            using StorageType = SoftRepartitioningKeyValueStorage<
                LmdbStorageEngine, AbslBtreeKeyStorage, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType =
                SoftThreadedRepartitioningKeyValueStorage<LmdbStorageEngine,
                                                          AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                LmdbStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = LmdbStorageEngine;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS, "LmdbStorageEngine",
                warmup_operations);
        }
    } else if (STORAGE_ENGINE == "map") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<MapStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "soft") {
            using StorageType = SoftRepartitioningKeyValueStorage<
                MapStorageEngine, AbslBtreeKeyStorage, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType =
                SoftThreadedRepartitioningKeyValueStorage<MapStorageEngine,
                                                          AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                MapStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = MapStorageEngine;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS, "MapStorageEngine",
                warmup_operations);
        }
    } else if (STORAGE_ENGINE == "tbb") {
        if (STORAGE_TYPE == "hard") {
            using StorageType =
                HardRepartitioningKeyValueStorage<TbbStorageEngine,
                                                  AbslBtreeKeyStorage,
                                                  UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "soft") {
            using StorageType = SoftRepartitioningKeyValueStorage<
                TbbStorageEngine, AbslBtreeKeyStorage, AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "threaded") {
            using StorageType =
                SoftThreadedRepartitioningKeyValueStorage<TbbStorageEngine,
                                                          AbslBtreeKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "SoftThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "hard_threaded") {
            using StorageType = HardThreadedRepartitioningKeyValueStorage<
                TbbStorageEngine, AbslBtreeKeyStorage,
                UnorderedDenseKeyStorage>;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS,
                "HardThreadedRepartitioningKeyValueStorage", warmup_operations);
        } else if (STORAGE_TYPE == "engine") {
            using StorageType = TbbStorageEngine;
            run_workload_with_storage<StorageType>(
                operations, PARTITION_COUNT, TEST_WORKERS, "TbbStorageEngine",
                warmup_operations);
        }
    }
}

/**
 * @brief Print usage information
 */
void print_usage(const char *program_name) {
    std::cout << "Usage: " << program_name
              << " [workload_files] [partition_count] [test_workers] "
                 "[storage_type] [storage_engine] [warmup_operations] "
                 "[storage_paths] "
                 "[repartition_interval_ms]"
              << std::endl;
    std::cout << "\nArguments:" << std::endl;
    std::cout << "  workload_files   Comma-separated paths to workload files "
                 "(one per worker)"
              << std::endl;
    std::cout << "  partition_count  Number of partitions (default: 4)"
              << std::endl;
    std::cout << "  test_workers     Number of worker threads (default: 1)"
              << std::endl;
    std::cout << "  storage_type     Storage implementation: 'hard', 'soft', "
                 "'threaded', 'hard_threaded', or 'engine' (default: soft)"
              << std::endl;
    std::cout << "  storage_engine   Storage engine backend: 'tkrzw_tree', "
                 "'tkrzw_hash', "
                 "'lmdb', 'map', or 'tbb' (default: tkrzw_tree)"
              << std::endl;
    std::cout << "  warmup_operations Number of operations to execute before "
                 "starting the experiment timer (default: 0)"
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
    std::cout << "\nStorage Engines:" << std::endl;
    std::cout
        << "  tkrzw_tree      TkrzwTreeStorageEngine (sorted key-value storage)"
        << std::endl;
    std::cout << "  tkrzw_hash      TkrzwHashStorageEngine (hash-based storage)"
              << std::endl;
    std::cout << "  lmdb            LmdbStorageEngine (LMDB-based storage)"
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

    std::string workload_files_str = argv[1];
    std::stringstream ss_wf(workload_files_str);
    std::string wf;
    while (std::getline(ss_wf, wf, ',')) {
        // Trim whitespace
        wf.erase(0, wf.find_first_not_of(" \t"));
        wf.erase(wf.find_last_not_of(" \t") + 1);
        if (!wf.empty()) {
            WORKLOAD_FILES.push_back(wf);
        }
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

    // Validate that the number of workload files matches the number of workers
    if (WORKLOAD_FILES.size() != TEST_WORKERS) {
        std::cerr << "Error: Number of workload files ("
                  << WORKLOAD_FILES.size()
                  << ") does not match number of workers (" << TEST_WORKERS
                  << ")" << std::endl;
        return 1;
    }

    if (argc >= 5) {
        STORAGE_TYPE = argv[4];
        if (STORAGE_TYPE != "hard" && STORAGE_TYPE != "soft" &&
            STORAGE_TYPE != "threaded" && STORAGE_TYPE != "hard_threaded" &&
            STORAGE_TYPE != "engine") {
            std::cerr << "Error: storage_type must be 'hard', 'soft', "
                         "'threaded', 'hard_threaded', or 'engine', got: "
                      << STORAGE_TYPE << std::endl;
            return 1;
        }
    }

    if (argc >= 6) {
        STORAGE_ENGINE = argv[5];
        if (STORAGE_ENGINE != "tkrzw_tree" && STORAGE_ENGINE != "tkrzw_hash" &&
            STORAGE_ENGINE != "lmdb" && STORAGE_ENGINE != "map" &&
            STORAGE_ENGINE != "tbb") {
            std::cerr
                << "Error: storage_engine must be 'tkrzw_tree', 'tkrzw_hash', "
                   "'lmdb', 'map', or 'tbb', got: "
                << STORAGE_ENGINE << std::endl;
            return 1;
        }
    }

    if (argc >= 7) {
        try {
            WARMUP_OPERATIONS = std::stoull(argv[6]);
        } catch (const std::exception &e) {
            std::cerr << "Error: Invalid warmup_operations: " << argv[6]
                      << std::endl;
            return 1;
        }
    }

    if (argc >= 8) {
        std::string paths_str = argv[7];
        STORAGE_PATHS.clear();
        // Parse comma-separated paths
        std::stringstream ss(paths_str);
        std::string path;
        while (std::getline(ss, path, ',')) {
            // Trim whitespace
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
            TRACKING_DURATION = std::chrono::milliseconds(interval_ms);
            REPARTITION_INTERVAL = std::chrono::milliseconds(interval_ms);
        } catch (const std::exception &e) {
            std::cerr << "Error: Invalid repartition_interval_ms: " << argv[8]
                      << std::endl;
            return 1;
        }
    }

    std::cout << "=== Repart-KV Workload Executor ===" << std::endl;
    std::cout << "Workload files: ";
    for (size_t i = 0; i < WORKLOAD_FILES.size(); ++i) {
        std::cout << WORKLOAD_FILES[i];
        if (i < WORKLOAD_FILES.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << std::endl;
    std::cout << "Partition count: " << PARTITION_COUNT << std::endl;
    std::cout << "Test workers: " << TEST_WORKERS << std::endl;
    std::cout << "Storage type: " << STORAGE_TYPE << std::endl;
    std::cout << "Storage engine: " << STORAGE_ENGINE << std::endl;
    std::cout << "Warmup operations: " << WARMUP_OPERATIONS << std::endl;
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

    // Read workloads
    std::vector<std::vector<workload::Operation>> all_worker_operations;
    all_worker_operations.reserve(TEST_WORKERS);
    size_t total_loaded_ops = 0;

    for (const auto &file : WORKLOAD_FILES) {
        try {
            auto ops = workload::read_workload(file);
            total_loaded_ops += ops.size();
            all_worker_operations.push_back(std::move(ops));
        } catch (const std::exception &e) {
            std::cerr << "Error loading workload file " << file << ": "
                      << e.what() << std::endl;
            return 1;
        }
    }

    std::cout << "Loaded " << format_with_separators(total_loaded_ops)
              << " operations from " << WORKLOAD_FILES.size()
              << " workload files" << std::endl;

    // Print summary of operations
    size_t read_count = 0, write_count = 0, scan_count = 0;
    for (const auto &worker_ops : all_worker_operations) {
        for (const auto &op : worker_ops) {
            switch (op.type) {
                case workload::OperationType::READ:
                    read_count++;
                    break;
                case workload::OperationType::WRITE:
                    write_count++;
                    break;
                case workload::OperationType::SCAN:
                    scan_count++;
                    break;
            }
        }
    }

    std::cout << "\nOperation summary:" << std::endl;
    std::cout << "  READ:  " << format_with_separators(read_count) << std::endl;
    std::cout << "  WRITE: " << format_with_separators(write_count)
              << std::endl;
    std::cout << "  SCAN:  " << format_with_separators(scan_count) << std::endl;

    // Execute workload with the selected storage type and engine
    execute_with_storage_config(all_worker_operations);

    return 0;
}
