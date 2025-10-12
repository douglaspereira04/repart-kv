#include <iostream>
#include "storage/MapStorageEngine.h"
#include "keystorage/MapKeyStorage.h"

int main() {
    std::cout << "=== Repart-KV Demo ===" << std::endl;
    
    // ========================================
    // MapStorageEngine Demo (CRTP-based)
    // ========================================
    std::cout << "\n--- MapStorageEngine Demo ---" << std::endl;
    
    MapStorageEngine engine;
    
    // Write some test data
    engine.write("user:1", "Alice");
    engine.write("user:2", "Bob");
    engine.write("user:3", "Charlie");
    engine.write("config:debug", "true");
    engine.write("config:port", "8080");
    
    std::cout << "\nReading values:" << std::endl;
    std::cout << "  user:1 = " << engine.read("user:1") << std::endl;
    std::cout << "  user:2 = " << engine.read("user:2") << std::endl;
    std::cout << "  config:debug = " << engine.read("config:debug") << std::endl;
    
    std::cout << "\nScanning for 'user:' prefix:" << std::endl;
    auto user_keys = engine.scan("user:", 10);
    for (const auto& [key, value] : user_keys) {
        std::cout << "  " << key << " = " << value << std::endl;
    }
    
    std::cout << "\nScanning for 'config:' prefix:" << std::endl;
    auto config_keys = engine.scan("config:", 10);
    for (const auto& [key, value] : config_keys) {
        std::cout << "  " << key << " = " << value << std::endl;
    }
    
    // ========================================
    // MapKeyStorage Demo (CRTP-based with concepts)
    // ========================================
    std::cout << "\n--- MapKeyStorage Demo ---" << std::endl;
    
    MapKeyStorage<int> intStore;
    
    intStore.put("counter:visits", 100);
    intStore.put("counter:clicks", 250);
    intStore.put("counter:downloads", 75);
    
    int visits;
    if (intStore.get("counter:visits", visits)) {
        std::cout << "\nVisits: " << visits << std::endl;
    }
    
    std::cout << "\nAll counters:" << std::endl;
    auto it = intStore.lower_bound("counter:");
    while (!it.is_end()) {
        std::cout << "  " << it.get_key() << " = " << it.get_value() << std::endl;
        ++it;
    }
    
    // ========================================
    std::cout << "\n=== Demo completed successfully! ===" << std::endl;
    std::cout << "\nProject uses C++20 with CRTP for zero-overhead abstractions." << std::endl;
    std::cout << "No virtual functions = maximum performance!" << std::endl;
    
    return 0;
}
