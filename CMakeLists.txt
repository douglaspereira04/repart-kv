cmake_minimum_required(VERSION 3.20)

project(repart-kv VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Create executable
add_executable(repart-kv main.cpp StorageEngine.h RepartitionTable.h)

# Set compiler flags
target_compile_features(repart-kv PRIVATE cxx_std_20)

# Enable all warnings and treat them as errors
target_compile_options(repart-kv PRIVATE 
    -Wall -Wextra -Wpedantic -Werror
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)
