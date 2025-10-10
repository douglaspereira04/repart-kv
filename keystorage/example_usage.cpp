#include "MapKeyStorage.h"
#include <iostream>
#include <string>

/**
 * @brief Example usage of KeyStorage abstract class and its std::map implementation
 */
int main() {
    // Example 1: Using MapKeyStorage with integer values
    std::cout << "=== Example 1: Integer values ===" << std::endl;
    MapKeyStorage<int> intStorage;
    
    // Put some key-value pairs
    intStorage.put("apple", 5);
    intStorage.put("banana", 10);
    intStorage.put("cherry", 15);
    intStorage.put("date", 20);
    
    // Get a value
    int value;
    if (intStorage.get("banana", value)) {
        std::cout << "Value for 'banana': " << value << std::endl;
    }
    
    // Use lower_bound to iterate from a specific key
    std::cout << "Keys >= 'cherry':" << std::endl;
    auto it = intStorage.lower_bound("cherry");
    while (!it.is_end()) {
        std::cout << "  " << it.get_key() << " -> " << it.get_value() << std::endl;
        ++it;
    }
    
    std::cout << std::endl;
    
    // Example 2: Using MapKeyStorage with pointer values
    std::cout << "=== Example 2: Pointer values ===" << std::endl;
    MapKeyStorage<std::string*> ptrStorage;
    
    // Create some string objects
    std::string* str1 = new std::string("Hello");
    std::string* str2 = new std::string("World");
    std::string* str3 = new std::string("KeyStorage");
    
    // Put pointers
    ptrStorage.put("greeting", str1);
    ptrStorage.put("target", str2);
    ptrStorage.put("concept", str3);
    
    // Get a pointer value
    std::string* retrievedPtr;
    if (ptrStorage.get("greeting", retrievedPtr)) {
        std::cout << "Value at 'greeting': " << *retrievedPtr << std::endl;
    }
    
    // Iterate through all entries
    std::cout << "All entries:" << std::endl;
    auto ptrIt = ptrStorage.lower_bound("");
    while (!ptrIt.is_end()) {
        std::cout << "  " << ptrIt.get_key() << " -> " << *ptrIt.get_value() << std::endl;
        ++ptrIt;
    }
    
    // Clean up
    delete str1;
    delete str2;
    delete str3;
    
    std::cout << std::endl;
    
    // Example 3: Using MapKeyStorage with long values
    std::cout << "=== Example 3: Long integer values ===" << std::endl;
    MapKeyStorage<long> longStorage;
    
    longStorage.put("small", 100L);
    longStorage.put("medium", 10000L);
    longStorage.put("large", 1000000L);
    
    // Lower bound search
    std::cout << "Keys >= 'med':" << std::endl;
    auto longIt = longStorage.lower_bound("med");
    while (!longIt.is_end()) {
        std::cout << "  " << longIt.get_key() << " -> " << longIt.get_value() << std::endl;
        ++longIt;
    }
    
    return 0;
}

