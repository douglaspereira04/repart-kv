# Utils

This directory contains common utility files used across the project.

## Files

- `test_assertions.h` - Common testing framework with assertion macros and test result tracking

## Purpose

This directory is dedicated to storing reusable utility code that is shared across multiple parts of the project. These utilities help maintain consistency and reduce code duplication.

## Usage

To use utilities from this directory, include them with the appropriate relative path:

```cpp
#include "utils/test_assertions.h"
```

## Test Assertions

The `test_assertions.h` file provides:

- **Test Framework**: `TEST()` and `END_TEST()` macros for structured testing
- **Assertions**: `ASSERT_EQ`, `ASSERT_STR_EQ`, `ASSERT_TRUE`, `ASSERT_FALSE`
- **Comparisons**: `ASSERT_GT`, `ASSERT_GE`, `ASSERT_LT`, `ASSERT_LE`
- **Test Tracking**: Global counters for `tests_passed` and `tests_failed`

This ensures consistent testing across all test files in the project.
