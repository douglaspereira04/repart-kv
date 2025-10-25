#include "TkrzwHashKeyStorage.h"
#include "TkrzwTreeKeyStorage.h"
#include <iostream>

/**
 * @brief Example usage of TKRZW-based KeyStorage implementations
 */
int main() {
    std::cout << "=== TKRZW KeyStorage Examples ===" << std::endl;

    // ========================================
    // Example 1: TkrzwHashKeyStorage with integers
    // ========================================
    std::cout << "\n--- Example 1: TkrzwHashKeyStorage (integers) ---"
              << std::endl;
    {
        TkrzwHashKeyStorage<int> hashStorage;

        if (!hashStorage.is_open()) {
            std::cerr << "Failed to open hash key storage!" << std::endl;
            return 1;
        }

        // Put some values
        hashStorage.put("counter:visits", 100);
        hashStorage.put("counter:clicks", 250);
        hashStorage.put("counter:downloads", 75);
        hashStorage.put("counter:shares", 50);

        // Get a value
        int visits;
        if (hashStorage.get("counter:visits", visits)) {
            std::cout << "Visits: " << visits << std::endl;
        }

        // Iterate through all entries
        std::cout << "\nAll counters (unordered - HashDBM):" << std::endl;
        auto it = hashStorage.lower_bound("");
        while (!it.is_end()) {
            std::cout << "  " << it.get_key() << " = " << it.get_value()
                      << std::endl;
            ++it;
        }
    }

    // ========================================
    // Example 2: TkrzwTreeKeyStorage with integers (sorted)
    // ========================================
    std::cout << "\n--- Example 2: TkrzwTreeKeyStorage (integers, sorted) ---"
              << std::endl;
    {
        TkrzwTreeKeyStorage<int> treeStorage;

        if (!treeStorage.is_open()) {
            std::cerr << "Failed to open tree key storage!" << std::endl;
            return 1;
        }

        // Put some values
        treeStorage.put("counter:visits", 100);
        treeStorage.put("counter:clicks", 250);
        treeStorage.put("counter:downloads", 75);
        treeStorage.put("counter:shares", 50);

        // Get a value
        int clicks;
        if (treeStorage.get("counter:clicks", clicks)) {
            std::cout << "Clicks: " << clicks << std::endl;
        }

        // Iterate through all entries (sorted by key)
        std::cout << "\nAll counters (sorted - TreeDBM):" << std::endl;
        auto it = treeStorage.lower_bound("");
        while (!it.is_end()) {
            std::cout << "  " << it.get_key() << " = " << it.get_value()
                      << std::endl;
            ++it;
        }

        // Use lower_bound to find entries >= "counter:d"
        std::cout << "\nCounters >= 'counter:d':" << std::endl;
        auto it2 = treeStorage.lower_bound("counter:d");
        while (!it2.is_end()) {
            std::cout << "  " << it2.get_key() << " = " << it2.get_value()
                      << std::endl;
            ++it2;
        }
    }

    // ========================================
    // Example 3: TkrzwTreeKeyStorage with long integers
    // ========================================
    std::cout << "\n--- Example 3: TkrzwTreeKeyStorage (long integers) ---"
              << std::endl;
    {
        TkrzwTreeKeyStorage<long> treeStorage;

        if (!treeStorage.is_open()) {
            std::cerr << "Failed to open tree key storage!" << std::endl;
            return 1;
        }

        // Put some large values
        treeStorage.put("timestamp:start", 1234567890123L);
        treeStorage.put("timestamp:end", 1234567999999L);
        treeStorage.put("user_id:alice", 1001L);
        treeStorage.put("user_id:bob", 1002L);

        // Get and display
        long alice_id;
        if (treeStorage.get("user_id:alice", alice_id)) {
            std::cout << "Alice's ID: " << alice_id << std::endl;
        }

        // Iterate through timestamps
        std::cout << "\nAll timestamps:" << std::endl;
        auto it = treeStorage.lower_bound("timestamp:");
        while (!it.is_end()) {
            std::string key = it.get_key();
            if (key.find("timestamp:") == 0) {
                std::cout << "  " << key << " = " << it.get_value()
                          << std::endl;
            } else {
                break; // TreeDBM is sorted, stop when prefix doesn't match
            }
            ++it;
        }
    }

    // ========================================
    // Example 4: TkrzwHashKeyStorage with pointers
    // ========================================
    std::cout << "\n--- Example 4: TkrzwHashKeyStorage (pointers) ---"
              << std::endl;
    {
        TkrzwHashKeyStorage<std::string *> ptrStorage;

        if (!ptrStorage.is_open()) {
            std::cerr << "Failed to open hash key storage for pointers!"
                      << std::endl;
            return 1;
        }

        // Create some string objects
        std::string *str1 = new std::string("Hello");
        std::string *str2 = new std::string("World");
        std::string *str3 = new std::string("TKRZW");

        // Store pointers
        ptrStorage.put("greeting", str1);
        ptrStorage.put("target", str2);
        ptrStorage.put("technology", str3);

        // Retrieve pointer
        std::string *retrieved;
        if (ptrStorage.get("greeting", retrieved)) {
            std::cout << "Retrieved greeting: " << *retrieved << std::endl;
            std::cout << "Original pointer: " << str1
                      << ", Retrieved: " << retrieved << std::endl;
            std::cout << "Pointers match: "
                      << (str1 == retrieved ? "Yes" : "No") << std::endl;
        }

        // Clean up
        delete str1;
        delete str2;
        delete str3;
    }

    // ========================================
    // Example 5: Comparison - Hash vs Tree performance
    // ========================================
    std::cout << "\n--- Example 5: Performance Comparison ---" << std::endl;
    {
        TkrzwHashKeyStorage<int> hashStorage;
        TkrzwTreeKeyStorage<int> treeStorage;

        const int num_entries = 1000;

        std::cout << "\nWriting " << num_entries << " entries..." << std::endl;

        // Write to both
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "key:" + std::to_string(i);
            hashStorage.put(key, i);
            treeStorage.put(key, i);
        }

        std::cout << "Data written successfully to both storages" << std::endl;

        // Test lower_bound efficiency
        std::cout << "\nTesting lower_bound('key:5')..." << std::endl;

        // Hash storage (less efficient - must iterate through all)
        auto hash_it = hashStorage.lower_bound("key:5");
        int hash_count = 0;
        while (!hash_it.is_end() && hash_count < 5) {
            hash_count++;
            ++hash_it;
        }
        std::cout << "Hash storage: Found entries (unordered)" << std::endl;

        // Tree storage (more efficient - direct jump)
        auto tree_it = treeStorage.lower_bound("key:5");
        int tree_count = 0;
        std::cout << "Tree storage: First 5 entries >= 'key:5':" << std::endl;
        while (!tree_it.is_end() && tree_count < 5) {
            std::cout << "  " << tree_it.get_key() << " = "
                      << tree_it.get_value() << std::endl;
            ++tree_it;
            tree_count++;
        }
    }

    std::cout << "\n=== All examples completed successfully! ===" << std::endl;
    std::cout << "\nKey differences:" << std::endl;
    std::cout
        << "  - TkrzwHashKeyStorage: Fast random access, unordered iteration"
        << std::endl;
    std::cout << "  - TkrzwTreeKeyStorage: Sorted keys, efficient range queries"
              << std::endl;
    std::cout << "  - Both support integral types and pointers" << std::endl;
    std::cout << "  - Both use CRTP for zero-overhead abstraction" << std::endl;

    return 0;
}
