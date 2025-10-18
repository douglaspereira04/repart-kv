#include "Future.h"
#include "../../../utils/test_assertions.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_future_constructor() {
    TEST("future_constructor")
        int value = 100;
        Future<int> future(value);
        
        // Test that constructor locks the mutex by checking if we can get the value
        ASSERT_EQ(100, future.get_value_ref());
    END_TEST("future_constructor")
}

void test_future_int_operations() {
    TEST("future_int_operations")
        int value = 0;
        Future<int> future(value);
        
        // Test basic operations
        ASSERT_EQ(0, future.get_value_ref());
        
        // Modify the value through the future
        future.get_value_ref() = 100;
        ASSERT_EQ(100, future.get_value_ref());
        ASSERT_EQ(100, value); // Verify original value is modified
    END_TEST("future_int_operations")
}

void test_future_string_operations() {
    TEST("future_string_operations")
        std::string value = "Hello";
        Future<std::string> future(value);
        
        // Test basic operations
        ASSERT_STR_EQ("Hello", future.get_value_ref());
        
        // Modify the string through the future
        future.get_value_ref() += " World";
        ASSERT_STR_EQ("Hello World", future.get_value_ref());
        ASSERT_STR_EQ("Hello World", value); // Verify original value is modified
    END_TEST("future_string_operations")
}

void test_future_const_correctness() {
    TEST("future_const_correctness")
        int value = 100;
        Future<int> future(value);
        
        // Test that we can read the value
        int read_value = future.get_value_ref();
        ASSERT_EQ(100, read_value);
        
        // Test wait and notify - start background thread to call notify
        std::thread notify_thread([&future]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            future.notify();
        });
        
        // Now wait (this will block until notify_thread calls notify)
        future.wait();
        ASSERT_EQ(100, future.get_value_ref());
        
        // Clean up
        notify_thread.join();
    END_TEST("future_const_correctness")
}

void test_future_correctness() {
    TEST("future_const_correctness")
        int value = 0;
        Future<int> future(value);
        
        // Test that we can read the value
        int read_value = future.get_value_ref();
        ASSERT_EQ(0, read_value);
        
        // Test wait and notify - start background thread to call notify
        std::thread notify_thread([&future]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            future.get_value_ref() = 100;
            future.notify();
        });
        
        // Now wait (this will block until notify_thread calls notify)
        future.wait();
        ASSERT_EQ(100, future.get_value_ref());
        
        // Clean up
        notify_thread.join();
    END_TEST("future_const_correctness")
}

void test_future_reference_relationship() {
    TEST("future_reference_relationship")
        std::string value = "test_value";
        Future<std::string> future(value);
        
        // Test that the future holds a reference to the original data
        ASSERT_TRUE(&future.get_value_ref() == &value);
        
        // Modify through future and verify original changes
        future.get_value_ref() = "modified_value";
        ASSERT_STR_EQ("modified_value", value);
        ASSERT_STR_EQ("modified_value", future.get_value_ref());
    END_TEST("future_reference_relationship")
}

int main() {
    std::cout << "Starting Future class tests..." << std::endl << std::endl;
    
    test_future_constructor();
    test_future_int_operations();
    test_future_string_operations();
    test_future_const_correctness();
    test_future_correctness();
    test_future_reference_relationship();
    
    std::cout << std::endl << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "All Future tests completed successfully!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
