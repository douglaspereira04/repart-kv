#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <fstream>
#include <filesystem>
#include "workload/Workload.h"
#include "kvstorage/SoftRepartitioningKeyValueStorage.h"
#include "storage/TkrzwHashStorageEngine.h"
#include "storage/TkrzwTreeStorageEngine.h"
#include "keystorage/TkrzwHashKeyStorage.h"
#include "keystorage/TkrzwTreeKeyStorage.h"

// Global parameters
size_t PARTITION_COUNT = 4;
size_t TEST_WORKERS = 1;

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
        for (const auto& entry : std::filesystem::recursive_directory_iterator(".")) {
            if (entry.is_regular_file()) {
                total_size += entry.file_size();
            }
        }
    } catch (const std::exception& e) {
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
 */
void metrics_loop(const std::vector<size_t>& executed_counts, 
                  const std::atomic<bool>& running,
                  const std::string& output_file,
                  std::chrono::high_resolution_clock::time_point start_time,
                  SoftRepartitioningKeyValueStorage<TkrzwTreeStorageEngine, TkrzwTreeKeyStorage, TkrzwHashKeyStorage>& storage) {
    std::ofstream file(output_file);
    if (!file.is_open()) {
        std::cerr << "Warning: Failed to open metrics file: " << output_file << std::endl;
        return;
    }
    
    // Write CSV header
    file << "elapsed_time_ms,executed_count,memory_kb,disk_kb,Tracking,Repartitioning" << std::endl;
    
    // Track previous tracking state to detect transitions
    bool prev_tracking_enabled = false;
    
    while (running) {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
        
        // Sum all worker counts (no synchronization needed - approximate values are fine)
        size_t count = 0;
        for (size_t worker_count : executed_counts) {
            count += worker_count;
        }
        
        size_t memory_kb = get_memory_usage_kb();
        size_t disk_kb = get_disk_usage_kb();
        
        // Get current tracking and repartitioning status
        bool current_tracking_enabled = storage.enable_tracking();
        bool current_repartitioning = storage.is_repartitioning();
        
        // Determine repartitioning status: "o" if repartitioning flag is set OR tracking was disabled
        char repartitioning_status = 'x';
        if (current_repartitioning || (prev_tracking_enabled && !current_tracking_enabled)) {
            repartitioning_status = 'o';
        }
        
        // Write metrics to CSV
        file << elapsed.count() << "," 
             << count << "," 
             << memory_kb << "," 
             << disk_kb << ","
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

/**
 * @brief Worker function that executes a subset of operations
 * @param worker_id ID of this worker (0-indexed)
 * @param operations Reference to the full operations vector
 * @param storage Reference to the storage instance
 * @param executed_counts Array of counters (one per worker, no synchronization needed)
 */
void worker_function(
    size_t worker_id,
    const std::vector<Operation>& operations,
    SoftRepartitioningKeyValueStorage<TkrzwTreeStorageEngine, TkrzwTreeKeyStorage, TkrzwHashKeyStorage>& storage,
    std::vector<size_t>& executed_counts) {
    
    // Execute operations in a strided pattern
    // Worker 0: operations[0], operations[0 + TEST_WORKERS], operations[0 + 2*TEST_WORKERS], ...
    // Worker 1: operations[1], operations[1 + TEST_WORKERS], operations[1 + 2*TEST_WORKERS], ...
    // etc.
    for (size_t i = worker_id; i < operations.size(); i += TEST_WORKERS) {
        const auto& op = operations[i];
        
        switch (op.type) {
            case OperationType::READ: {
                std::string value = storage.read(op.key);
                executed_counts[worker_id]++;
                break;
            }
            case OperationType::WRITE: {
                storage.write(op.key, op.value);
                executed_counts[worker_id]++;
                break;
            }
            case OperationType::SCAN: {
                auto results = storage.scan(op.key, op.limit);
                executed_counts[worker_id]++;
                break;
            }
        }
    }
}

/**
 * @brief Print usage information
 */
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <workload_file> [partition_count] [test_workers]" << std::endl;
    std::cout << "\nArguments:" << std::endl;
    std::cout << "  workload_file    Path to the workload file" << std::endl;
    std::cout << "  partition_count  Number of partitions (default: 4)" << std::endl;
    std::cout << "  test_workers     Number of worker threads (default: 1)" << std::endl;
    std::cout << "\nWorkload file format:" << std::endl;
    std::cout << "  0,<key>         : READ operation" << std::endl;
    std::cout << "  1,<key>         : WRITE operation (uses 1KB default value)" << std::endl;
    std::cout << "  2,<key>,<limit> : SCAN operation" << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string workload_file = argv[1];
    
    if (argc >= 3) {
        try {
            PARTITION_COUNT = std::stoull(argv[2]);
            if (PARTITION_COUNT == 0) {
                std::cerr << "Error: partition_count must be greater than 0" << std::endl;
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: Invalid partition_count: " << argv[2] << std::endl;
            return 1;
        }
    }
    
    if (argc >= 4) {
        try {
            TEST_WORKERS = std::stoull(argv[3]);
            if (TEST_WORKERS == 0) {
                std::cerr << "Error: test_workers must be greater than 0" << std::endl;
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: Invalid test_workers: " << argv[3] << std::endl;
            return 1;
        }
    }
    
    std::cout << "=== Repart-KV Workload Executor ===" << std::endl;
    std::cout << "Workload file: " << workload_file << std::endl;
    std::cout << "Partition count: " << PARTITION_COUNT << std::endl;
    std::cout << "Test workers: " << TEST_WORKERS << std::endl;
    std::cout << std::endl;
    
    // Read workload
    std::vector<Operation> operations;
    try {
        operations = read_workload(workload_file);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Loaded " << operations.size() << " operations from workload file" << std::endl;
    
    // Print summary of operations
    size_t read_count = 0, write_count = 0, scan_count = 0;
    for (const auto& op : operations) {
        switch (op.type) {
            case OperationType::READ:
                read_count++;
                break;
            case OperationType::WRITE:
                write_count++;
                break;
            case OperationType::SCAN:
                scan_count++;
                break;
        }
    }
    
    std::cout << "\nOperation summary:" << std::endl;
    std::cout << "  READ:  " << read_count << std::endl;
    std::cout << "  WRITE: " << write_count << std::endl;
    std::cout << "  SCAN:  " << scan_count << std::endl;
    
    // Create SoftRepartitioningKeyValueStorage instance
    std::cout << "\n=== Initializing Storage ===" << std::endl;
    SoftRepartitioningKeyValueStorage<TkrzwTreeStorageEngine, TkrzwTreeKeyStorage, TkrzwHashKeyStorage> storage(
        PARTITION_COUNT, 
        std::hash<std::string>(), 
        std::chrono::milliseconds(1000),  // tracking_duration: 1 second
        std::chrono::milliseconds(1000)   // repartition_interval: 1 second
    );
    std::cout << "Created SoftRepartitioningKeyValueStorage with " << PARTITION_COUNT << " partitions (Tkrzw)" << std::endl;
    std::cout << "Tracking duration: 1000ms, Repartition interval: 1000ms" << std::endl;
    
    // Setup metrics tracking
    std::vector<size_t> executed_counts(TEST_WORKERS, 0);  // One counter per worker
    std::atomic<bool> metrics_running(true);
    std::string metrics_file = "metrics.csv";
    
    // Execute operations
    std::cout << "\n=== Executing Workload ===" << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Start metrics logging thread
    std::thread metrics_thread(metrics_loop, std::ref(executed_counts), std::ref(metrics_running), 
                               metrics_file, start_time, std::ref(storage));
    
    // Create and start worker threads
    std::vector<std::thread> worker_threads;
    worker_threads.reserve(TEST_WORKERS);
    
    for (size_t worker_id = 0; worker_id < TEST_WORKERS; ++worker_id) {
        worker_threads.emplace_back(worker_function, worker_id, std::ref(operations), 
                                    std::ref(storage), std::ref(executed_counts));
    }
    
    // Wait for all worker threads to complete
    for (auto& thread : worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Stop metrics thread
    metrics_running = false;
    if (metrics_thread.joinable()) {
        metrics_thread.join();
    }
    
    // Calculate total executed operations
    size_t total_executed = 0;
    for (size_t count : executed_counts) {
        total_executed += count;
    }
    
    // Print execution statistics
    std::cout << "\n=== Execution Complete ===" << std::endl;
    std::cout << "Operations executed: " << total_executed << std::endl;
    std::cout << "Execution time: " << duration.count() << " microseconds" << std::endl;
    std::cout << "Average time per operation: " 
              << (total_executed == 0 ? 0.0 : static_cast<double>(duration.count()) / total_executed) 
              << " microseconds" << std::endl;
    std::cout << "Metrics saved to: " << metrics_file << std::endl;
    
    return 0;
}
