#include "MapStorageEngine.h"
#include "TkrzwHashStorageEngine.h"
#include "TkrzwTreeStorageEngine.h"
#include <iostream>
#include <string>
#include <memory>
#include <sstream>
#include <regex>

/**
 * @brief Interactive storage engine testing program
 */

// Base class for engine wrapper to enable polymorphic storage
class StorageEngineWrapper {
public:
    virtual ~StorageEngineWrapper() = default;
    virtual std::string read(const std::string& key) const = 0;
    virtual void write(const std::string& key, const std::string& value) = 0;
    virtual std::vector<std::pair<std::string, std::string>> scan(const std::string& prefix, size_t limit) const = 0;
    virtual std::string get_name() const = 0;
};

template<typename EngineType>
class StorageEngineWrapperImpl : public StorageEngineWrapper {
private:
    EngineType engine_;
    std::string name_;

public:
    StorageEngineWrapperImpl(const std::string& name) : name_(name) {}

    std::string read(const std::string& key) const override {
        return engine_.read(key);
    }

    void write(const std::string& key, const std::string& value) override {
        engine_.write(key, value);
    }

    std::vector<std::pair<std::string, std::string>> scan(const std::string& prefix, size_t limit) const override {
        return engine_.scan(prefix, limit);
    }

    std::string get_name() const override {
        return name_;
    }
};

void print_help() {
    std::cout << "\nAvailable commands:" << std::endl;
    std::cout << "  get(\"key\")                  - Read a value by key" << std::endl;
    std::cout << "  write(\"key\", \"value\")      - Write a key-value pair" << std::endl;
    std::cout << "  scan(\"start_key\", limit)    - Scan keys >= start_key (lower_bound)" << std::endl;
    std::cout << "  help                        - Show this help" << std::endl;
    std::cout << "  exit                        - Exit the program" << std::endl;
    std::cout << std::endl;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

std::string extract_string(const std::string& str, size_t& pos) {
    // Find opening quote
    size_t start = str.find('"', pos);
    if (start == std::string::npos) {
        return "";
    }
    start++; // Move past the opening quote
    
    // Find closing quote
    size_t end = str.find('"', start);
    if (end == std::string::npos) {
        return "";
    }
    
    pos = end + 1;
    return str.substr(start, end - start);
}

bool parse_command(const std::string& input, StorageEngineWrapper* engine) {
    std::string cmd = trim(input);
    
    if (cmd == "exit" || cmd == "quit") {
        return false;
    }
    
    if (cmd == "help" || cmd == "?") {
        print_help();
        return true;
    }
    
    // Parse get("key")
    if (cmd.find("get(") == 0) {
        size_t pos = 4;
        std::string key = extract_string(cmd, pos);
        
        std::string value = engine->read(key);
        if (value.empty()) {
            std::cout << "(empty)" << std::endl;
        } else {
            std::cout << value << std::endl;
        }
        return true;
    }
    
    // Parse write("key", "value")
    if (cmd.find("write(") == 0) {
        size_t pos = 6;
        std::string key = extract_string(cmd, pos);
        std::string value = extract_string(cmd, pos);
        
        engine->write(key, value);
        std::cout << "OK" << std::endl;
        return true;
    }
    
    // Parse scan("prefix", limit)
    if (cmd.find("scan(") == 0) {
        size_t pos = 5;
        std::string prefix = extract_string(cmd, pos);
        
        // Find the comma
        size_t comma = cmd.find(',', pos);
        if (comma == std::string::npos) {
            std::cout << "Error: scan requires two parameters" << std::endl;
            return true;
        }
        
        // Extract limit
        size_t close = cmd.find(')', comma);
        std::string limit_str = cmd.substr(comma + 1, close - comma - 1);
        limit_str = trim(limit_str);
        
        int limit = std::stoi(limit_str);
        
        auto results = engine->scan(prefix, limit);
        if (results.empty()) {
            std::cout << "(no results)" << std::endl;
        } else {
            for (const auto& [key, value] : results) {
                std::cout << key << " -> " << value << std::endl;
            }
        }
        return true;
    }
    
    std::cout << "Unknown command. Type 'help' for available commands." << std::endl;
    return true;
}

int main() {
    std::cout << "=== Interactive Storage Engine Test ===" << std::endl;
    std::cout << "\nSelect a storage engine:" << std::endl;
    std::cout << "  1. MapStorageEngine (in-memory, std::map)" << std::endl;
    std::cout << "  2. TkrzwHashStorageEngine (hash-based, unordered)" << std::endl;
    std::cout << "  3. TkrzwTreeStorageEngine (tree-based, sorted)" << std::endl;
    std::cout << "\nEnter choice (1-3): ";
    
    int choice;
    std::cin >> choice;
    std::cin.ignore(); // Clear newline
    
    std::unique_ptr<StorageEngineWrapper> engine;
    
    switch (choice) {
        case 1:
            engine = std::make_unique<StorageEngineWrapperImpl<MapStorageEngine>>("MapStorageEngine");
            break;
        case 2:
            engine = std::make_unique<StorageEngineWrapperImpl<TkrzwHashStorageEngine>>("TkrzwHashStorageEngine");
            break;
        case 3:
            engine = std::make_unique<StorageEngineWrapperImpl<TkrzwTreeStorageEngine>>("TkrzwTreeStorageEngine");
            break;
        default:
            std::cout << "Invalid choice. Exiting." << std::endl;
            return 1;
    }
    
    std::cout << "\nUsing: " << engine->get_name() << std::endl;
    print_help();
    
    // Main command loop
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break; // EOF or error
        }
        
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        
        try {
            if (!parse_command(line, engine.get())) {
                break; // exit command
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
    
    std::cout << "\nGoodbye!" << std::endl;
    return 0;
}

