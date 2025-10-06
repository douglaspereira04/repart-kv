#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "StorageEngine.h"

/**
 * @brief Example implementation of StorageEngine using std::unordered_map
 */
class MapStorageEngine : public StorageEngine<std::unordered_map<std::string, std::string>> {
public:
    MapStorageEngine() : StorageEngine<std::unordered_map<std::string, std::string>>(new std::unordered_map<std::string, std::string>()) {}
    
    ~MapStorageEngine() override {
        delete storage_engine;
    }
    
    std::string read(const std::string& key) override {
        auto it = storage_engine->find(key);
        return (it != storage_engine->end()) ? it->second : "";
    }
    
    void write(const std::string& key, const std::string& value) override {
        (*storage_engine)[key] = value;
    }
    
    std::vector<std::string> scan(const std::string& key_prefix, size_t limit) override {
        std::vector<std::string> results;
        for (const auto& pair : *storage_engine) {
            if (pair.first.starts_with(key_prefix)) {
                results.push_back(pair.first);
                if (results.size() >= limit) {
                    break;
                }
            }
        }
        return results;
    }
};

int main() {
    std::cout << "=== Repart-KV Storage Engine Demo ===" << std::endl;
    
    // Create a storage engine instance
    MapStorageEngine engine;
    
    // Write some test data
    engine.write("user:1", "Alice");
    engine.write("user:2", "Bob");
    engine.write("user:3", "Charlie");
    engine.write("config:debug", "true");
    engine.write("config:port", "8080");
    
    std::cout << "\n--- Reading values ---" << std::endl;
    std::cout << "user:1 = " << engine.read("user:1") << std::endl;
    std::cout << "user:2 = " << engine.read("user:2") << std::endl;
    std::cout << "config:debug = " << engine.read("config:debug") << std::endl;
    
    std::cout << "\n--- Scanning for 'user:' prefix ---" << std::endl;
    auto user_keys = engine.scan("user:", 10);
    for (const auto& key : user_keys) {
        std::cout << "Found key: " << key << " = " << engine.read(key) << std::endl;
    }
    
    std::cout << "\n--- Scanning for 'config:' prefix ---" << std::endl;
    auto config_keys = engine.scan("config:", 10);
    for (const auto& key : config_keys) {
        std::cout << "Found key: " << key << " = " << engine.read(key) << std::endl;
    }
    
    std::cout << "\nStorage engine demo completed successfully!" << std::endl;
    
    return 0;
}
