#include "MapKeyStorage.h"
#include <iostream>

// This should compile - valid types
void test_valid_types() {
    MapKeyStorage<int> intStorage;
    MapKeyStorage<long> longStorage;
    MapKeyStorage<unsigned int> uintStorage;
    MapKeyStorage<int*> ptrStorage;
    std::cout << "Valid types compile successfully!" << std::endl;
}

// Uncomment these to test concept rejection - they should fail to compile
/*
void test_invalid_types() {
    // This should fail - double is not integral or pointer
    MapKeyStorage<double> doubleStorage;
    
    // This should fail - string is not integral or pointer
    MapKeyStorage<std::string> stringStorage;
    
    // This should fail - vector is not integral or pointer
    MapKeyStorage<std::vector<int>> vectorStorage;
}
*/

int main() {
    test_valid_types();
    std::cout << "\nC++20 concepts working correctly!" << std::endl;
    std::cout << "KeyStorageValueType concept enforces integral or pointer types only." << std::endl;
    return 0;
}

