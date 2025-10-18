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

# Fetch unordered_dense library (header-only)
include(FetchContent)
FetchContent_Declare(
    unordered_dense
    GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
    GIT_TAG v4.7.0
)
FetchContent_MakeAvailable(unordered_dense)

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

# Find METIS library for graph partitioning (needed for main executable)
find_library(METIS_LIB NAMES metis)
if(METIS_LIB)
    message(STATUS "METIS library found: ${METIS_LIB}")
else()
    message(WARNING "METIS library not found. Install METIS to enable repartitioning features.")
endif()

# Create main executable
add_executable(repart-kv 
    main.cpp
)

# Link pthread, TKRZW, METIS, and unordered_dense
target_link_libraries(repart-kv PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${METIS_LIB}
    unordered_dense::unordered_dense
)

# Set compiler flags
target_compile_features(repart-kv PRIVATE cxx_std_20)

# Enable all warnings (but not as errors for flexibility)
target_compile_options(repart-kv PRIVATE 
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2>
)

# Include directories
target_include_directories(repart-kv PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Optional: Build generic storage engine tests (tests all engines)
add_executable(test_storage_engine 
    storage/test_storage_engine.cpp
)

target_link_libraries(test_storage_engine PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    unordered_dense::unordered_dense
)
target_compile_features(test_storage_engine PRIVATE cxx_std_20)
target_compile_options(test_storage_engine PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_storage_engine PRIVATE 
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
    unordered_dense::unordered_dense
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

target_link_libraries(example_keystorage PRIVATE 
    ${TKRZW_ALL_LIBS}
    unordered_dense::unordered_dense
)
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
    unordered_dense::unordered_dense
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
    unordered_dense::unordered_dense
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
    unordered_dense::unordered_dense
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
    unordered_dense::unordered_dense
)
target_compile_features(interactive_storage_test PRIVATE cxx_std_20)
target_compile_options(interactive_storage_test PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(interactive_storage_test PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Interactive keystorage tester
add_executable(interactive_keystorage_test 
    keystorage/interactive_keystorage_test.cpp
)

target_link_libraries(interactive_keystorage_test PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    unordered_dense::unordered_dense
)
target_compile_features(interactive_keystorage_test PRIVATE cxx_std_20)
target_compile_options(interactive_keystorage_test PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(interactive_keystorage_test PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Generic keystorage tests (tests all key storage implementations)
add_executable(test_keystorage 
    keystorage/test_keystorage.cpp
)

target_link_libraries(test_keystorage PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    unordered_dense::unordered_dense
)
target_compile_features(test_keystorage PRIVATE cxx_std_20)
target_compile_options(test_keystorage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_keystorage PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Generic partitioned key-value storage tests
add_executable(test_partitioned_kv_storage 
    kvstorage/test_partitioned_kv_storage.cpp
)

target_link_libraries(test_partitioned_kv_storage PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${METIS_LIB}
    unordered_dense::unordered_dense
)
target_compile_features(test_partitioned_kv_storage PRIVATE cxx_std_20)
target_compile_options(test_partitioned_kv_storage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_partitioned_kv_storage PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Graph tracking tests for repartitioning storage
add_executable(test_graph_tracking 
    kvstorage/test_graph_tracking.cpp
)

target_link_libraries(test_graph_tracking PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${METIS_LIB}
    unordered_dense::unordered_dense
)
target_compile_features(test_graph_tracking PRIVATE cxx_std_20)
target_compile_options(test_graph_tracking PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_graph_tracking PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Unified repartitioning tests (requires METIS)
if(METIS_LIB)
    add_executable(test_repartitioning_storage 
        kvstorage/test_repartitioning_storage.cpp
    )
    
    target_link_libraries(test_repartitioning_storage PRIVATE 
        Threads::Threads
        ${TKRZW_ALL_LIBS}
        ${METIS_LIB}
        unordered_dense::unordered_dense
    )
    target_compile_features(test_repartitioning_storage PRIVATE cxx_std_20)
    target_compile_options(test_repartitioning_storage PRIVATE -Wall -Wextra -Wpedantic)
    target_include_directories(test_repartitioning_storage PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${TKRZW_INCLUDE_DIRS}
    )
endif()

# Future class tests
add_executable(test_future 
    kvstorage/threaded/test_future.cpp
)

target_link_libraries(test_future PRIVATE 
    Threads::Threads
)
target_compile_features(test_future PRIVATE cxx_std_20)
target_compile_options(test_future PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_future PRIVATE 
    ${CMAKE_SOURCE_DIR}
)

# Operation class tests
add_executable(test_operation 
    kvstorage/threaded/operation/test_operation.cpp
)

target_link_libraries(test_operation PRIVATE 
    Threads::Threads
)
target_compile_features(test_operation PRIVATE cxx_std_20)
target_compile_options(test_operation PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_operation PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/operation
)

# Graph tests
add_executable(test_graph 
    graph/test_graph.cpp
)

target_link_libraries(test_graph PRIVATE unordered_dense::unordered_dense)
target_compile_features(test_graph PRIVATE cxx_std_20)
target_compile_options(test_graph PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_graph PRIVATE 
    ${CMAKE_SOURCE_DIR}
)

# Graph example
add_executable(example_graph 
    graph/example_graph.cpp
)

target_link_libraries(example_graph PRIVATE unordered_dense::unordered_dense)
target_compile_features(example_graph PRIVATE cxx_std_20)
target_compile_options(example_graph PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(example_graph PRIVATE 
    ${CMAKE_SOURCE_DIR}
)

if(METIS_LIB)
    # METIS graph tests
    add_executable(test_metis_graph 
        graph/test_metis_graph.cpp
    )
    
    target_link_libraries(test_metis_graph PRIVATE 
        ${METIS_LIB}
        unordered_dense::unordered_dense
    )
    target_compile_features(test_metis_graph PRIVATE cxx_std_20)
    target_compile_options(test_metis_graph PRIVATE -Wall -Wextra -Wpedantic)
    target_include_directories(test_metis_graph PRIVATE 
        ${CMAKE_SOURCE_DIR}
    )
    
    # METIS graph example
    add_executable(example_metis_graph 
        graph/example_metis_graph.cpp
    )
    
    target_link_libraries(example_metis_graph PRIVATE 
        ${METIS_LIB}
        unordered_dense::unordered_dense
    )
    target_compile_features(example_metis_graph PRIVATE cxx_std_20)
    target_compile_options(example_metis_graph PRIVATE -Wall -Wextra -Wpedantic)
    target_include_directories(example_metis_graph PRIVATE 
        ${CMAKE_SOURCE_DIR}
    )
else()
    message(WARNING "METIS library not found. Skipping METIS graph examples and tests. Install METIS to enable graph partitioning features.")
endif()

