# Utilities

This directory contains shared utilities used across the project.

## `test_assertions.h`

A lightweight test helper used by multiple `test_*` executables.

### What it provides

- Test structure macros: `TEST()`, `END_TEST()`
- Assertion macros: `ASSERT_EQ`, `ASSERT_STR_EQ`, `ASSERT_TRUE`, `ASSERT_FALSE`
- Comparison helpers: `ASSERT_GT`, `ASSERT_GE`, `ASSERT_LT`, `ASSERT_LE`
- Global counters: `tests_passed`, `tests_failed`

### Include

```cpp
#include "utils/test_assertions.h"
```
