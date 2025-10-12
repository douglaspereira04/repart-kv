#include "MapKeyStorage.h"
#include "TkrzwHashKeyStorage.h"
#include "TkrzwTreeKeyStorage.h"
#include <iostream>
#include <string>
#include <memory>
#include <sstream>
#include <iomanip>

/**
 * @brief Interactive key storage testing program
 */

// Base class for storage wrapper to enable polymorphic storage
class KeyStorageWrapper {
public:
    virtual ~KeyStorageWrapper() = default;
    virtual bool get(const std::string& key, std::string& value_str) const = 0;
    virtual void put(const std::string& key, const std::string& value_str) = 0;
    virtual void scan(const std::string& key, size_t limit) = 0;
    virtual std::string get_name() const = 0;
    virtual std::string get_type() const = 0;
};

template<typename StorageType, typename ValueType>
class KeyStorageWrapperImpl : public KeyStorageWrapper {
private:
    StorageType storage_;
    std::string name_;
    std::string type_;

public:
    KeyStorageWrapperImpl(const std::string& name, const std::string& type) 
        : name_(name), type_(type) {}

    bool get(const std::string& key, std::string& value_str) const override {
        ValueType value;
        if (storage_.get(key, value)) {
            std::ostringstream oss;
            oss << value;
            value_str = oss.str();
            return true;
        }
        return false;
    }

    void put(const std::string& key, const std::string& value_str) override {
        std::istringstream iss(value_str);
        ValueType value;
        iss >> value;
        storage_.put(key, value);
    }

    void scan(const std::string& key, size_t limit) override {
        auto it = storage_.lower_bound(key);
        if (it.is_end()) {
            std::cout << "(no results)" << std::endl;
        } else {
            size_t count = 0;
            while (!it.is_end() && count < limit) {
                std::cout << it.get_key() << " -> " << it.get_value() << std::endl;
                ++it;
                count++;
            }
        }
    }

    std::string get_name() const override {
        return name_;
    }

    std::string get_type() const override {
        return type_;
    }
};

void print_help() {
    std::cout << "\nAvailable commands:" << std::endl;
    std::cout << "  get(\"key\")                  - Get a value by key" << std::endl;
    std::cout << "  put(\"key\", value)           - Put a key-value pair" << std::endl;
    std::cout << "  scan(\"start_key\", limit)    - Scan entries >= start_key (up to limit)" << std::endl;
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

std::string extract_value(const std::string& str, size_t pos) {
    // Find the comma after the key
    size_t comma = str.find(',', pos);
    if (comma == std::string::npos) {
        return "";
    }
    
    // Find the closing parenthesis
    size_t close = str.find(')', comma);
    if (close == std::string::npos) {
        return "";
    }
    
    // Extract and trim the value
    std::string value_str = str.substr(comma + 1, close - comma - 1);
    return trim(value_str);
}

bool parse_command(const std::string& input, KeyStorageWrapper* storage) {
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
        
        std::string value_str;
        if (storage->get(key, value_str)) {
            std::cout << value_str << std::endl;
        } else {
            std::cout << "(not found)" << std::endl;
        }
        return true;
    }
    
    // Parse put("key", value)
    if (cmd.find("put(") == 0) {
        size_t pos = 4;
        std::string key = extract_string(cmd, pos);
        std::string value_str = extract_value(cmd, pos);
        
        if (value_str.empty()) {
            std::cout << "Error: Invalid value" << std::endl;
            return true;
        }
        
        storage->put(key, value_str);
        std::cout << "OK" << std::endl;
        return true;
    }
    
    // Parse scan("key", limit)
    if (cmd.find("scan(") == 0) {
        size_t pos = 5;
        std::string key = extract_string(cmd, pos);
        
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
        
        storage->scan(key, limit);
        return true;
    }
    
    std::cout << "Unknown command. Type 'help' for available commands." << std::endl;
    return true;
}

int main() {
    std::cout << "=== Interactive KeyStorage Test ===" << std::endl;
    
    // First, select value type
    std::cout << "\nSelect value type:" << std::endl;
    std::cout << "  1. int" << std::endl;
    std::cout << "  2. long" << std::endl;
    std::cout << "  3. uint64_t" << std::endl;
    std::cout << "\nEnter choice (1-3): ";
    
    int type_choice;
    std::cin >> type_choice;
    std::cin.ignore();
    
    // Then, select storage implementation
    std::cout << "\nSelect a key storage implementation:" << std::endl;
    std::cout << "  1. MapKeyStorage (in-memory, std::map)" << std::endl;
    std::cout << "  2. TkrzwHashKeyStorage (hash-based, unordered)" << std::endl;
    std::cout << "  3. TkrzwTreeKeyStorage (tree-based, sorted)" << std::endl;
    std::cout << "\nEnter choice (1-3): ";
    
    int storage_choice;
    std::cin >> storage_choice;
    std::cin.ignore();
    
    std::unique_ptr<KeyStorageWrapper> storage;
    
    // Create storage based on both choices
    if (type_choice == 1) {
        // int type
        switch (storage_choice) {
            case 1:
                storage = std::make_unique<KeyStorageWrapperImpl<MapKeyStorage<int>, int>>(
                    "MapKeyStorage", "int");
                break;
            case 2:
                storage = std::make_unique<KeyStorageWrapperImpl<TkrzwHashKeyStorage<int>, int>>(
                    "TkrzwHashKeyStorage", "int");
                break;
            case 3:
                storage = std::make_unique<KeyStorageWrapperImpl<TkrzwTreeKeyStorage<int>, int>>(
                    "TkrzwTreeKeyStorage", "int");
                break;
            default:
                std::cout << "Invalid choice. Exiting." << std::endl;
                return 1;
        }
    } else if (type_choice == 2) {
        // long type
        switch (storage_choice) {
            case 1:
                storage = std::make_unique<KeyStorageWrapperImpl<MapKeyStorage<long>, long>>(
                    "MapKeyStorage", "long");
                break;
            case 2:
                storage = std::make_unique<KeyStorageWrapperImpl<TkrzwHashKeyStorage<long>, long>>(
                    "TkrzwHashKeyStorage", "long");
                break;
            case 3:
                storage = std::make_unique<KeyStorageWrapperImpl<TkrzwTreeKeyStorage<long>, long>>(
                    "TkrzwTreeKeyStorage", "long");
                break;
            default:
                std::cout << "Invalid choice. Exiting." << std::endl;
                return 1;
        }
    } else if (type_choice == 3) {
        // uint64_t type
        switch (storage_choice) {
            case 1:
                storage = std::make_unique<KeyStorageWrapperImpl<MapKeyStorage<uint64_t>, uint64_t>>(
                    "MapKeyStorage", "uint64_t");
                break;
            case 2:
                storage = std::make_unique<KeyStorageWrapperImpl<TkrzwHashKeyStorage<uint64_t>, uint64_t>>(
                    "TkrzwHashKeyStorage", "uint64_t");
                break;
            case 3:
                storage = std::make_unique<KeyStorageWrapperImpl<TkrzwTreeKeyStorage<uint64_t>, uint64_t>>(
                    "TkrzwTreeKeyStorage", "uint64_t");
                break;
            default:
                std::cout << "Invalid choice. Exiting." << std::endl;
                return 1;
        }
    } else {
        std::cout << "Invalid type choice. Exiting." << std::endl;
        return 1;
    }
    
    std::cout << "\nUsing: " << storage->get_name() << "<" << storage->get_type() << ">" << std::endl;
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
            if (!parse_command(line, storage.get())) {
                break; // exit command
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
    
    std::cout << "\nGoodbye!" << std::endl;
    return 0;
}

