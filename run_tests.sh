#!/bin/bash

# Test runner script for repart-kv project
# This script finds and runs all test executables in the build directory

# Don't exit on error - we want to run all tests even if some fail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters for test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

echo -e "${BLUE}=== Repart-KV Test Runner ===${NC}"
echo ""

# Check if build directory exists
if [ ! -d "build" ]; then
    echo -e "${RED}Error: build directory not found. Please run ./build.sh first.${NC}"
    exit 1
fi

# Find all test executables in build directory
TEST_EXECUTABLES=($(find build -name "test_*" -type f -executable | sort))

if [ ${#TEST_EXECUTABLES[@]} -eq 0 ]; then
    echo -e "${YELLOW}Warning: No test executables found in build directory.${NC}"
    echo "Available files in build directory:"
    ls -la build/ | grep -E "^-.*"
    exit 1
fi

echo -e "${BLUE}Found ${#TEST_EXECUTABLES[@]} test executable(s):${NC}"
for test in "${TEST_EXECUTABLES[@]}"; do
    echo "  - $(basename "$test")"
done
echo ""

# Function to run a single test
run_test() {
    local test_executable="$1"
    local test_name=$(basename "$test_executable")
    
    echo -e "${BLUE}Running: $test_name${NC}"
    echo "----------------------------------------"
    
    # Run the test and capture exit code
    if "$test_executable"; then
        echo -e "${GREEN}✓ $test_name PASSED${NC}"
        ((PASSED_TESTS++))
    else
        local exit_code=$?
        echo -e "${RED}✗ $test_name FAILED (exit code: $exit_code)${NC}"
        ((FAILED_TESTS++))
    fi
    
    ((TOTAL_TESTS++))
    echo ""
}

# Run all tests
for test_executable in "${TEST_EXECUTABLES[@]}"; do
    run_test "$test_executable"
done

# Print summary
echo -e "${BLUE}=== Test Summary ===${NC}"
echo -e "Total tests: $TOTAL_TESTS"
echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"
if [ $FAILED_TESTS -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED_TESTS${NC}"
fi
if [ $SKIPPED_TESTS -gt 0 ]; then
    echo -e "${YELLOW}Skipped: $SKIPPED_TESTS${NC}"
fi

echo ""

# Exit with appropriate code
if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}🎉 All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}❌ Some tests failed.${NC}"
    exit 1
fi
