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

option(BUILD_REPART_KV_RUNNER
    "Build the optional repart-kv runner/testing utility"
    OFF)

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

# Fetch loadgen workload generator library
FetchContent_Declare(
    loadgen
    GIT_REPOSITORY https://github.com/douglaspereira04/loadgen.git
    GIT_TAG main
)
FetchContent_MakeAvailable(loadgen)
if(DEFINED loadgen_SOURCE_DIR AND TARGET loadgen-core)
    target_include_directories(loadgen-core PUBLIC 
        ${loadgen_SOURCE_DIR}
        ${loadgen_SOURCE_DIR}/src
        ${loadgen_SOURCE_DIR}/src/request
        ${loadgen_SOURCE_DIR}/src/types
    )
endif()

# Find Abseil library (system-wide)
find_package(absl REQUIRED)

# Find TKRZW library and its dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(TKRZW REQUIRED tkrzw)

# Find LMDB library
find_library(LMDB_LIB NAMES lmdb)
if(LMDB_LIB)
    message(STATUS "LMDB library found: ${LMDB_LIB}")
else()
    message(WARNING "LMDB library not found. Install liblmdb++-dev to enable LMDB storage engine.")
endif()

# Find LevelDB library (libleveldb-dev)
find_library(LEVELDB_LIB NAMES leveldb)
find_path(LEVELDB_INCLUDE_DIR NAMES leveldb/db.h)
if(LEVELDB_LIB AND LEVELDB_INCLUDE_DIR)
    message(STATUS "LevelDB library found: ${LEVELDB_LIB}")
else()
    message(WARNING "LevelDB library not found. Install libleveldb-dev to enable LevelDB storage engine.")
endif()

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

# Find TBB library for concurrent queues
set(TBB_LIBRARIES "")
# Try pkg-config first (most reliable on Linux)
pkg_check_modules(TBB_PKG QUIET tbb)
if(TBB_PKG_FOUND)
    message(STATUS "TBB library found via pkg-config: ${TBB_PKG_LIBRARIES}")
    set(TBB_LIBRARIES ${TBB_PKG_LIBRARIES})
else()
    # Try find_package
    find_package(TBB QUIET)
    if(TBB_FOUND AND TBB_LIBRARIES)
        message(STATUS "TBB library found via find_package: ${TBB_LIBRARIES}")
    else()
        # Fallback: find library manually
        find_library(TBB_LIB NAMES tbb)
        if(TBB_LIB)
            message(STATUS "TBB library found manually: ${TBB_LIB}")
            set(TBB_LIBRARIES ${TBB_LIB})
        else()
            message(WARNING "TBB library not found. Install libtbb-dev to enable TBB concurrent queues.")
        endif()
    endif()
endif()

# Core library target for repart-kv functionality
add_library(repart-kv-core STATIC
    src/repart_kv.cpp
)

target_link_libraries(repart-kv-core PUBLIC 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${METIS_LIB}
    ${LMDB_LIB}
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
    absl::btree
    loadgen::core
)

target_compile_features(repart-kv-core PUBLIC cxx_std_20)
target_compile_options(repart-kv-core PRIVATE 
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2>
)

target_include_directories(repart-kv-core PUBLIC 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

if(BUILD_REPART_KV_RUNNER)
    add_executable(repart-kv-runner 
        main.cpp
    )
    target_link_libraries(repart-kv-runner PRIVATE 
        repart-kv-core
    )
    target_compile_features(repart-kv-runner PRIVATE cxx_std_20)
    target_compile_options(repart-kv-runner PRIVATE 
        -Wall -Wextra -Wpedantic
    )
endif()

# Optional: Build generic storage engine tests (tests all engines)
add_executable(test_storage_engine 
    storage/test/test_storage_engine.cpp
)

target_link_libraries(test_storage_engine PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${LMDB_LIB}
    ${LEVELDB_LIB}
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
    absl::btree
)
target_include_directories(test_storage_engine PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
    ${LEVELDB_INCLUDE_DIR}
)
target_compile_features(test_storage_engine PRIVATE cxx_std_20)
target_compile_options(test_storage_engine PRIVATE -Wall -Wextra -Wpedantic)

# Interactive storage engine tester
add_executable(interactive_storage_test 
    storage/interactive_storage_test.cpp
)

target_link_libraries(interactive_storage_test PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
    absl::btree
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
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
    absl::btree
)
target_compile_features(interactive_keystorage_test PRIVATE cxx_std_20)
target_compile_options(interactive_keystorage_test PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(interactive_keystorage_test PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Generic keystorage tests (tests all key storage implementations)
add_executable(test_keystorage 
    keystorage/test/test_keystorage.cpp
)

target_link_libraries(test_keystorage PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
    ${LMDB_LIB}
    absl::btree
)
target_compile_features(test_keystorage PRIVATE cxx_std_20)
target_compile_options(test_keystorage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_keystorage PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Generic partitioned key-value storage tests
add_executable(test_partitioned_kv_storage 
    kvstorage/test/test_partitioned_kv_storage.cpp
)

target_link_libraries(test_partitioned_kv_storage PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${METIS_LIB}
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
    absl::btree
)
target_compile_features(test_partitioned_kv_storage PRIVATE cxx_std_20)
target_compile_options(test_partitioned_kv_storage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_partitioned_kv_storage PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Graph tracking tests for repartitioning storage
add_executable(test_graph_tracking 
    kvstorage/test/test_graph_tracking.cpp
)

target_link_libraries(test_graph_tracking PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${METIS_LIB}
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
    absl::btree
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
        kvstorage/test/test_repartitioning_storage.cpp
    )
    
    target_link_libraries(test_repartitioning_storage PRIVATE 
        Threads::Threads
        ${TKRZW_ALL_LIBS}
        ${METIS_LIB}
        ${LMDB_LIB}
        ${TBB_LIBRARIES}
        unordered_dense::unordered_dense
        absl::btree
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
    kvstorage/threaded/future/test/test_future.cpp
)

target_link_libraries(test_future PRIVATE 
    Threads::Threads
    ${TBB_LIBRARIES}
)
target_compile_features(test_future PRIVATE cxx_std_20)
target_compile_options(test_future PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_future PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/future
)

# Operation class tests
add_executable(test_operation 
    kvstorage/threaded/operation/test/test_operation.cpp
)

target_link_libraries(test_operation PRIVATE 
    Threads::Threads
    ${TBB_LIBRARIES}
)
target_compile_features(test_operation PRIVATE cxx_std_20)
target_compile_options(test_operation PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_operation PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/operation
)

# ReadOperation class tests
add_executable(test_readoperation 
    kvstorage/threaded/operation/test/test_readoperation.cpp
)

target_link_libraries(test_readoperation PRIVATE 
    Threads::Threads
    ${TBB_LIBRARIES}
)
target_compile_features(test_readoperation PRIVATE cxx_std_20)
target_compile_options(test_readoperation PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_readoperation PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/operation
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/future
)

# Storage Operation tests
add_executable(test_storage_operation 
    kvstorage/threaded/operation/test/test_storage_operation.cpp
)

target_link_libraries(test_storage_operation PRIVATE 
    Threads::Threads
    ${TBB_LIBRARIES}
)
target_compile_features(test_storage_operation PRIVATE cxx_std_20)
target_compile_options(test_storage_operation PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_storage_operation PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/operation
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/future
)

# WriteOperation class tests
add_executable(test_writeoperation 
    kvstorage/threaded/operation/test/test_writeoperation.cpp
)

target_link_libraries(test_writeoperation PRIVATE 
    Threads::Threads
    ${TBB_LIBRARIES}
)
target_compile_features(test_writeoperation PRIVATE cxx_std_20)
target_compile_options(test_writeoperation PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_writeoperation PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/operation
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/future
)

# ScanOperation class tests
add_executable(test_scanoperation 
    kvstorage/threaded/operation/test/test_scanoperation.cpp
)

target_link_libraries(test_scanoperation PRIVATE 
    Threads::Threads
    ${TBB_LIBRARIES}
)
target_compile_features(test_scanoperation PRIVATE cxx_std_20)
target_compile_options(test_scanoperation PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_scanoperation PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/operation
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/future
)

# DoneOperation class tests
add_executable(test_doneoperation 
    kvstorage/threaded/operation/test/test_doneoperation.cpp
)

target_link_libraries(test_doneoperation PRIVATE 
    Threads::Threads
    ${TBB_LIBRARIES}
)
target_compile_features(test_doneoperation PRIVATE cxx_std_20)
target_compile_options(test_doneoperation PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_doneoperation PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/operation
)

# SyncOperation class tests
add_executable(test_syncoperation 
    kvstorage/threaded/operation/test/test_syncoperation.cpp
)

target_link_libraries(test_syncoperation PRIVATE 
    Threads::Threads
    ${TBB_LIBRARIES}
)
target_compile_features(test_syncoperation PRIVATE cxx_std_20)
target_compile_options(test_syncoperation PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_syncoperation PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/operation
    ${CMAKE_SOURCE_DIR}/kvstorage/threaded/future
)

# SoftPartitionWorker class tests
add_executable(test_soft_partition_worker 
    kvstorage/threaded/test/test_soft_partition_worker.cpp
)

target_link_libraries(test_soft_partition_worker PRIVATE 
    Threads::Threads
    ${TKRZW_ALL_LIBS}
    ${METIS_LIB}
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
    absl::btree
)
target_compile_features(test_soft_partition_worker PRIVATE cxx_std_20)
target_compile_options(test_soft_partition_worker PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_soft_partition_worker PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${TKRZW_INCLUDE_DIRS}
)

# Graph tests
add_executable(test_graph 
    graph/test/test_graph.cpp
)

target_link_libraries(test_graph PRIVATE 
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
)
target_compile_features(test_graph PRIVATE cxx_std_20)
target_compile_options(test_graph PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_graph PRIVATE 
    ${CMAKE_SOURCE_DIR}
)

# Graph example
add_executable(example_graph 
    graph/example_graph.cpp
)

target_link_libraries(example_graph PRIVATE 
    ${TBB_LIBRARIES}
    unordered_dense::unordered_dense
)
target_compile_features(example_graph PRIVATE cxx_std_20)
target_compile_options(example_graph PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(example_graph PRIVATE 
    ${CMAKE_SOURCE_DIR}
)

if(METIS_LIB)
    # METIS graph tests
    add_executable(test_metis_graph 
        graph/test/test_metis_graph.cpp
    )
    
    target_link_libraries(test_metis_graph PRIVATE 
        ${METIS_LIB}
        ${TBB_LIBRARIES}
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
        ${TBB_LIBRARIES}
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

