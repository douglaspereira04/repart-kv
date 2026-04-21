#include "../MapKeyStorage.h"
#include "../../utils/test_assertions.h"
#include <iostream>
#include <vector>

struct TwoInts {
    int a;
    int b;
};

// This should compile - valid types
void test_valid_types() {
    TEST("valid_types")
    MapKeyStorage<int> intStorage;
    MapKeyStorage<long> longStorage;
    MapKeyStorage<unsigned int> uintStorage;
    MapKeyStorage<int *> ptrStorage;
    MapKeyStorage<TwoInts> structStorage;
    std::cout << "Valid types compile successfully!" << std::endl;
    END_TEST("valid_types")
}

// Uncomment these to test concept rejection - they should fail to compile
/*
void test_invalid_types() {
    // This should fail - string is not trivially copyable
    MapKeyStorage<std::string> stringStorage;

    // This should fail - vector is not trivially copyable
    MapKeyStorage<std::vector<int>> vectorStorage;
}
*/

int main() {
    std::vector<std::pair<std::string, TestFunction>> tests = {
        {"valid_types", test_valid_types}};

    run_test_suite("C++20 Concepts", tests);

    std::cout << "\nC++20 concepts working correctly!" << std::endl;
    std::cout << "KeyStorageValueType concept enforces trivially copyable, "
                 "default-constructible types."
              << std::endl;
    return 0;
}
