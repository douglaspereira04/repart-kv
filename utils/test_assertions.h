#pragma once

#include <iostream>
#include <string>
#include <stdexcept>

// Test result tracking
extern int tests_passed;
extern int tests_failed;

// Test framework macros
#define TEST(name) \
    std::cout << "Running test: " << name << "..." << std::endl; \
    try {

#define END_TEST(name) \
        std::cout << "  ✓ " << name << " PASSED" << std::endl; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "  ✗ " << name << " FAILED: " << e.what() << std::endl; \
        tests_failed++; \
    } catch (...) { \
        std::cout << "  ✗ " << name << " FAILED: Unknown exception" << std::endl; \
        tests_failed++; \
    }

// Assertion macros
#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        throw std::runtime_error(std::string("Expected '") + std::to_string(expected) + \
                                "' but got '" + std::to_string(actual) + "'"); \
    }

#define ASSERT_STR_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        throw std::runtime_error(std::string("Expected '") + (expected) + \
                                "' but got '" + (actual) + "'"); \
    }

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error("Condition failed: " #condition); \
    }

#define ASSERT_FALSE(condition) \
    if (condition) { \
        throw std::runtime_error("Condition should be false: " #condition); \
    }

#define ASSERT_GT(left, right) \
    if (!((left) > (right))) { \
        throw std::runtime_error(std::string("Expected ") + std::to_string(left) + \
                                " > " + std::to_string(right)); \
    }

#define ASSERT_GE(left, right) \
    if (!((left) >= (right))) { \
        throw std::runtime_error(std::string("Expected ") + std::to_string(left) + \
                                " >= " + std::to_string(right)); \
    }

#define ASSERT_LT(left, right) \
    if (!((left) < (right))) { \
        throw std::runtime_error(std::string("Expected ") + std::to_string(left) + \
                                " < " + std::to_string(right)); \
    }

#define ASSERT_LE(left, right) \
    if (!((left) <= (right))) { \
        throw std::runtime_error(std::string("Expected ") + std::to_string(left) + \
                                " <= " + std::to_string(right)); \
    }
