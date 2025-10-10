#include "MapStorageEngine.h"
#include <iostream>
#include <thread>

/**
 * @brief Example usage of MapStorageEngine
 */
int main() {
    std::cout << "=== MapStorageEngine Example ===" << std::endl;
    
    MapStorageEngine storage;
    
    // Example 1: Basic write and read operations
    std::cout << "\n1. Basic Operations:" << std::endl;
    storage.write("user:1001", "Alice");
    storage.write("user:1002", "Bob");
    storage.write("user:1003", "Charlie");
    storage.write("product:2001", "Laptop");
    storage.write("product:2002", "Mouse");
    storage.write("product:2003", "Keyboard");
    
    std::cout << "Read user:1001: " << storage.read("user:1001") << std::endl;
    std::cout << "Read product:2002: " << storage.read("product:2002") << std::endl;
    std::cout << "Read non-existent key: '" << storage.read("user:9999") << "'" << std::endl;
    
    // Example 2: Scan with prefix
    std::cout << "\n2. Scan Operations:" << std::endl;
    std::cout << "Scan 'user:' (limit 10):" << std::endl;
    auto users = storage.scan("user:", 10);
    for (const auto& key : users) {
        std::cout << "  " << key << " -> " << storage.read(key) << std::endl;
    }
    
    std::cout << "\nScan 'product:' (limit 2):" << std::endl;
    auto products = storage.scan("product:", 2);
    for (const auto& key : products) {
        std::cout << "  " << key << " -> " << storage.read(key) << std::endl;
    }
    
    // Example 3: Update operation
    std::cout << "\n3. Update Operation:" << std::endl;
    std::cout << "Before update - user:1001: " << storage.read("user:1001") << std::endl;
    storage.write("user:1001", "Alice Updated");
    std::cout << "After update - user:1001: " << storage.read("user:1001") << std::endl;
    
    // Example 4: Manual locking for thread-safe operations
    std::cout << "\n4. Manual Locking Example:" << std::endl;
    
    auto writer = [&storage](int id) {
        for (int i = 0; i < 5; ++i) {
            std::string key = "thread:" + std::to_string(id) + ":item:" + std::to_string(i);
            std::string value = "Value from thread " + std::to_string(id) + " item " + std::to_string(i);
            
            // Manual exclusive lock for write
            storage.lock();
            storage.write(key, value);
            storage.unlock();
        }
    };
    
    auto reader = [&storage](int id) {
        // Manual shared lock for read
        storage.lock_shared();
        auto results = storage.scan("thread:", 100);
        storage.unlock_shared();
        
        std::cout << "  Thread " << id << " found " << results.size() << " keys" << std::endl;
    };
    
    // Create multiple writer and reader threads
    std::thread t1(writer, 1);
    std::thread t2(writer, 2);
    std::thread t3(reader, 3);
    std::thread t4(reader, 4);
    
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    
    std::cout << "\nAll thread operations completed successfully!" << std::endl;
    std::cout << "Note: Thread-safety achieved through manual lock()/unlock() calls" << std::endl;
    
    // Example 5: Scan all thread-written keys
    std::cout << "\n5. Final scan of thread-written keys:" << std::endl;
    auto thread_keys = storage.scan("thread:", 20);
    std::cout << "Total keys with 'thread:' prefix: " << thread_keys.size() << std::endl;
    for (const auto& key : thread_keys) {
        std::cout << "  " << key << std::endl;
    }
    
    std::cout << "\n=== Example Complete ===" << std::endl;
    return 0;
}

