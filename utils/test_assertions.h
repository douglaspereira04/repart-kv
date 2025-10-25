#pragma once

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <functional>

// Test result tracking
extern int tests_passed;
extern int tests_failed;

// Test function type
using TestFunction = std::function<void()>;

// Test runner function
void run_test_suite(
    const std::string &suite_name,
    const std::vector<std::pair<std::string, TestFunction>> &tests) {
    std::cout << "========================================" << std::endl;
    std::cout << "  Testing " << suite_name << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    for (const auto &[test_name, test_func] : tests) {
        std::cout << "Running test: " << test_name << "...";
        test_func();
        std::cout << " ✓ " << test_name << " PASSED" << std::endl;
    }

    std::cout << std::endl;
}

// Test framework macros
#define TEST(name) try {

#define END_TEST(name)                                                         \
    tests_passed++;                                                            \
    }                                                                          \
    catch (const std::exception &e) {                                          \
        std::cout << "  ✗ " << name << " FAILED: " << e.what() << std::endl;   \
        tests_failed++;                                                        \
    }                                                                          \
    catch (...) {                                                              \
        std::cout << "  ✗ " << name << " FAILED: Unknown exception"            \
                  << std::endl;                                                \
        tests_failed++;                                                        \
    }

// Assertion macros
#define ASSERT_EQ(expected, actual)                                            \
    if ((expected) != (actual)) {                                              \
        throw std::runtime_error(std::string("Expected '") +                   \
                                 std::to_string(expected) + "' but got '" +    \
                                 std::to_string(actual) + "'");                \
    }

#define ASSERT_STATUS_EQ(expected, actual)                                     \
    if ((expected) != (actual)) {                                              \
        throw std::runtime_error(std::string("Expected '") +                   \
                                 to_string(expected) + "' but got '" +         \
                                 to_string(actual) + "'");                     \
    }
#define ASSERT_STR_EQ(expected, actual)                                        \
    if ((expected) != (actual)) {                                              \
        throw std::runtime_error(std::string("Expected '") + (expected) +      \
                                 "' but got '" + (actual) + "'");              \
    }

#define ASSERT_TRUE(condition)                                                 \
    if (!(condition)) {                                                        \
        throw std::runtime_error("Condition failed: " #condition);             \
    }

#define ASSERT_FALSE(condition)                                                \
    if (condition) {                                                           \
        throw std::runtime_error("Condition should be false: " #condition);    \
    }

#define ASSERT_GT(left, right)                                                 \
    if (!((left) > (right))) {                                                 \
        throw std::runtime_error(std::string("Expected ") +                    \
                                 std::to_string(left) + " > " +                \
                                 std::to_string(right));                       \
    }

#define ASSERT_GE(left, right)                                                 \
    if (!((left) >= (right))) {                                                \
        throw std::runtime_error(std::string("Expected ") +                    \
                                 std::to_string(left) +                        \
                                 " >= " + std::to_string(right));              \
    }

#define ASSERT_LT(left, right)                                                 \
    if (!((left) < (right))) {                                                 \
        throw std::runtime_error(std::string("Expected ") +                    \
                                 std::to_string(left) + " < " +                \
                                 std::to_string(right));                       \
    }

#define ASSERT_LE(left, right)                                                 \
    if (!((left) <= (right))) {                                                \
        throw std::runtime_error(std::string("Expected ") +                    \
                                 std::to_string(left) +                        \
                                 " <= " + std::to_string(right));              \
    }
