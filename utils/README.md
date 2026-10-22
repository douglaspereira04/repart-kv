# Utilities

This directory contains shared utilities used across the project.

## `test_assertions.h`

A lightweight test helper used by multiple `test_*` executables.

### What it provides

- Test structure macros: `TEST()`, `END_TEST()`
- Assertion macros: `ASSERT_EQ`, `ASSERT_INT_EQ`, `ASSERT_STATUS_EQ`, `ASSERT_STR_EQ`, `ASSERT_TRUE`, `ASSERT_FALSE`
- Comparison helpers: `ASSERT_GT`, `ASSERT_GE`, `ASSERT_LT`, `ASSERT_LE`
- Suite helper: `run_test_suite(suite_name, tests)`
- Global counters: `tests_passed`, `tests_failed`

### Include

```cpp
#include "utils/test_assertions.h"
```

## `test_resources.h`

Tests that open on-disk engines write under `./test_resources` (created on first use, relative to the process cwd). `run_tests.sh` deletes that directory before each suite.

```cpp
#include "utils/test_resources.h"

EngineType engine(0, repart_kv_test::test_resources_dir());
```
