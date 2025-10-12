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

# Add pthread for multithreading support
find_package(Threads REQUIRED)

# Find TKRZW library and its dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(TKRZW REQUIRED tkrzw)

# TKRZW compression dependencies (optional, used if available)
find_library(LZMA_LIB NAMES lzma)
find_library(LZ4_LIB NAMES lz4)
find_library(ZSTD_LIB NAMES zstd)

# Collect all TKRZW-related libraries
set(TKRZW_ALL_LIBS ${TKRZW_LIBRARIES})
if(LZMA_LIB)
    list(APPEND TKRZW_ALL_LIBS ${LZMA_LIB})
endif()
if(LZ4_LIB)
    list(APPEND TKRZW_ALL_LIBS ${LZ4_LIB})
endif()
if(ZSTD_LIB)
    list(APPEND TKRZW_ALL_LIBS ${ZSTD_LIB})
endif()

# Create main executable
add_executable(repart-kv 
    main.cpp
)

# Link pthread and TKRZW
target_link_libraries(repart-kv PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
)

# Set compiler flags
target_compile_features(repart-kv PRIVATE cxx_std_20)

# Enable all warnings (but not as errors for flexibility)
target_compile_options(repart-kv PRIVATE 
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# Include directories
target_include_directories(repart-kv PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Optional: Build storage tests
add_executable(test_map_storage_engine 
    storage/test_map_storage_engine.cpp
)

target_link_libraries(test_map_storage_engine PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
)
target_compile_features(test_map_storage_engine PRIVATE cxx_std_20)
target_compile_options(test_map_storage_engine PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_map_storage_engine PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Optional: Build storage example
add_executable(example_storage 
    storage/example_storage.cpp
)

target_link_libraries(example_storage PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
)
target_compile_features(example_storage PRIVATE cxx_std_20)
target_compile_options(example_storage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(example_storage PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Optional: Build keystorage example (MapKeyStorage)
add_executable(example_keystorage 
    keystorage/example_usage.cpp
)

target_link_libraries(example_keystorage PRIVATE ${TKRZW_ALL_LIBS})
target_compile_features(example_keystorage PRIVATE cxx_std_20)
target_compile_options(example_keystorage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(example_keystorage PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Optional: Build TKRZW keystorage example
add_executable(example_tkrzw_keystorage 
    keystorage/example_tkrzw_keystorage.cpp
)

target_link_libraries(example_tkrzw_keystorage PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
)
target_compile_features(example_tkrzw_keystorage PRIVATE cxx_std_20)
target_compile_options(example_tkrzw_keystorage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(example_tkrzw_keystorage PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Optional: Build TKRZW Hash storage example
add_executable(example_tkrzw_storage 
    storage/example_tkrzw_storage.cpp
)

target_link_libraries(example_tkrzw_storage PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
)
target_compile_features(example_tkrzw_storage PRIVATE cxx_std_20)
target_compile_options(example_tkrzw_storage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(example_tkrzw_storage PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Optional: Build TKRZW Tree storage example
add_executable(example_tkrzw_tree_storage 
    storage/example_tkrzw_tree_storage.cpp
)

target_link_libraries(example_tkrzw_tree_storage PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
)
target_compile_features(example_tkrzw_tree_storage PRIVATE cxx_std_20)
target_compile_options(example_tkrzw_tree_storage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(example_tkrzw_tree_storage PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Interactive storage engine tester
add_executable(interactive_storage_test 
    storage/interactive_storage_test.cpp
)

target_link_libraries(interactive_storage_test PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
)
target_compile_features(interactive_storage_test PRIVATE cxx_std_20)
target_compile_options(interactive_storage_test PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(interactive_storage_test PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)
