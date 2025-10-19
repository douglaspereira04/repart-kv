#include "TkrzwTreeStorageEngine.h"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief Example usage of TkrzwTreeStorageEngine
 */
int main() {
    std::cout << "=== TkrzwTreeStorageEngine Example ===" << std::endl;
    
    // ========================================
    // Example 1: In-Memory Database
    // ========================================
    std::cout << "\n--- Example 1: In-Memory Database (Sorted Keys) ---" << std::endl;
    {
        TkrzwTreeStorageEngine engine;
        
        if (!engine.is_open()) {
            std::cerr << "Failed to open in-memory database!" << std::endl;
            return 1;
        }
        
        // Write some data
        engine.write("user:1001", "Alice");
        engine.write("user:1002", "Bob");
        engine.write("user:1003", "Charlie");
        engine.write("product:2001", "Laptop");
        engine.write("product:2002", "Mouse");
        
        std::cout << "Wrote 5 entries" << std::endl;
        std::cout << "Total records: " << engine.count() << std::endl;
        
        // Read data
        std::cout << "\nReading values:" << std::endl;
        std::string value;
        engine.read("user:1001", value);
        std::cout << "  user:1001 = " << value << std::endl;
        engine.read("product:2001", value);
        std::cout << "  product:2001 = " << value << std::endl;
        
        // Scan with prefix
        std::cout << "\nScanning for 'user:' prefix:" << std::endl;
        std::vector<std::pair<std::string, std::string>> users;
        engine.scan("user:", 10, users);
        for (const auto& [key, value] : users) {
            std::cout << "  " << key << " = " << value << std::endl;
        }
    }
    
    // ========================================
    // Example 2: Persistent File Database
    // ========================================
    std::cout << "\n--- Example 2: Persistent File Database ---" << std::endl;
    
    const std::string db_path = "/tmp/tkrzw_tree_test.tkt";
    
    // Write data to persistent database
    {
        std::cout << "Creating persistent database at: " << db_path << std::endl;
        TkrzwTreeStorageEngine engine(db_path);
        
        if (!engine.is_open()) {
            std::cerr << "Failed to open persistent database!" << std::endl;
            return 1;
        }
        
        engine.write("session:abc123", "user_id=42");
        engine.write("session:def456", "user_id=99");
        engine.write("config:max_connections", "1000");
        engine.write("config:timeout", "30");
        
        std::cout << "Wrote 4 entries" << std::endl;
        std::cout << "Total records: " << engine.count() << std::endl;
        
        // Synchronize to disk
        if (engine.sync()) {
            std::cout << "Database synchronized to disk" << std::endl;
        }
    }
    
    // Read data from persistent database
    {
        std::cout << "\nReopening persistent database..." << std::endl;
        TkrzwTreeStorageEngine engine(db_path);
        
        if (!engine.is_open()) {
            std::cerr << "Failed to reopen persistent database!" << std::endl;
            return 1;
        }
        
        std::cout << "Database reopened successfully" << std::endl;
        std::cout << "Total records: " << engine.count() << std::endl;
        
        // Verify data persisted
        std::cout << "\nVerifying persisted data:" << std::endl;
        std::string value;
        engine.read("session:abc123", value);
        std::cout << "  session:abc123 = " << value << std::endl;
        engine.read("config:max_connections", value);
        std::cout << "  config:max_connections = " << value << std::endl;
        
        // Scan all sessions
        std::cout << "\nAll sessions:" << std::endl;
        std::vector<std::pair<std::string, std::string>> sessions;
        engine.scan("session:", 10, sessions);
        for (const auto& [key, value] : sessions) {
            std::cout << "  " << key << " = " << value << std::endl;
        }
    }
    
    // ========================================
    // Example 3: Update and Remove Operations
    // ========================================
    std::cout << "\n--- Example 3: Update and Remove Operations ---" << std::endl;
    {
        TkrzwTreeStorageEngine engine(db_path);
        
        std::cout << "Records before operations: " << engine.count() << std::endl;
        
        // Update existing key
        engine.write("config:max_connections", "2000");
        std::string value;
        engine.read("config:max_connections", value);
        std::cout << "Updated config:max_connections = " 
                  << value << std::endl;
    }
    
    // ========================================
    // Example 4: Manual Locking for Thread Safety
    // ========================================
    std::cout << "\n--- Example 4: Manual Locking Example ---" << std::endl;
    {
        TkrzwTreeStorageEngine engine;
        
        auto writer = [&engine](int id) {
            for (int i = 0; i < 5; ++i) {
                std::string key = "thread:" + std::to_string(id) + ":count:" + std::to_string(i);
                std::string value = "thread_" + std::to_string(id) + "_value_" + std::to_string(i);
                
                // Manual exclusive lock for write
                engine.lock();
                engine.write(key, value);
                engine.unlock();
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        
        auto reader = [&engine]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Manual shared lock for read
            engine.lock_shared();
            std::vector<std::pair<std::string, std::string>> results;
            engine.scan("thread:", 100, results);
            int count = results.size();
            engine.unlock_shared();
            
            std::cout << "  Reader found " << count << " keys" << std::endl;
        };
        
        std::thread t1(writer, 1);
        std::thread t2(writer, 2);
        std::thread t3(reader);
        
        t1.join();
        t2.join();
        t3.join();
        
        std::cout << "Thread-safe operations completed" << std::endl;
        std::cout << "Final count: " << engine.count() << std::endl;
    }
    
    // ========================================
    // Example 5: Performance Characteristics
    // ========================================
    std::cout << "\n--- Example 5: Performance Test (with sorted scan) ---" << std::endl;
    {
        TkrzwTreeStorageEngine engine;
        
        const int num_entries = 10000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Write performance
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "perf:key:" + std::to_string(i);
            std::string value = "value_" + std::to_string(i);
            engine.write(key, value);
        }
        
        auto write_end = std::chrono::high_resolution_clock::now();
        auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_end - start);
        
        std::cout << "Wrote " << num_entries << " entries in " 
                  << write_duration.count() << " ms" << std::endl;
        std::cout << "Write throughput: " 
                  << (num_entries * 1000 / write_duration.count()) << " ops/sec" << std::endl;
        
        // Read performance
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "perf:key:" + std::to_string(i);
            [[maybe_unused]] std::string value;
            engine.read(key, value);
        }
        
        auto read_end = std::chrono::high_resolution_clock::now();
        auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(read_end - start);
        
        std::cout << "Read " << num_entries << " entries in " 
                  << read_duration.count() << " ms" << std::endl;
        std::cout << "Read throughput: " 
                  << (num_entries * 1000 / read_duration.count()) << " ops/sec" << std::endl;
        
        // Scan performance
        start = std::chrono::high_resolution_clock::now();
        std::vector<std::pair<std::string, std::string>> scan_results;
        engine.scan("perf:key:", 1000, scan_results);
        auto scan_end = std::chrono::high_resolution_clock::now();
        auto scan_duration = std::chrono::duration_cast<std::chrono::microseconds>(scan_end - start);
        
        std::cout << "Scanned " << scan_results.size() << " entries in " 
                  << scan_duration.count() << " μs" << std::endl;
    }
    
    std::cout << "\n=== All examples completed successfully! ===" << std::endl;
    std::cout << "\nTKRZW TreeDBM provides sorted key-value storage with:" << std::endl;
    std::cout << "  - Keys stored in lexicographic order" << std::endl;
    std::cout << "  - Very efficient prefix scanning (no full iteration)" << std::endl;
    std::cout << "  - Fast read operations" << std::endl;
    std::cout << "  - Optional file persistence" << std::endl;
    std::cout << "  - ACID properties" << std::endl;
    std::cout << "  - Zero virtual function overhead (CRTP)" << std::endl;
    std::cout << "  - Better for scan-heavy workloads than HashDBM" << std::endl;
    
    return 0;
}

