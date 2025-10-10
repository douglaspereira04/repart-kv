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

# Create main executable
add_executable(repart-kv 
    main.cpp
)

# Link pthread
target_link_libraries(repart-kv PRIVATE Threads::Threads)

# Set compiler flags
target_compile_features(repart-kv PRIVATE cxx_std_20)

# Enable all warnings (but not as errors for flexibility)
target_compile_options(repart-kv PRIVATE 
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# Include directories
target_include_directories(repart-kv PRIVATE ${CMAKE_SOURCE_DIR})

# Optional: Build storage tests
add_executable(test_map_storage_engine 
    storage/test_map_storage_engine.cpp
)

target_link_libraries(test_map_storage_engine PRIVATE Threads::Threads)
target_compile_features(test_map_storage_engine PRIVATE cxx_std_20)
target_compile_options(test_map_storage_engine PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(test_map_storage_engine PRIVATE ${CMAKE_SOURCE_DIR})

# Optional: Build storage example
add_executable(example_storage 
    storage/example_storage.cpp
)

target_link_libraries(example_storage PRIVATE Threads::Threads)
target_compile_features(example_storage PRIVATE cxx_std_20)
target_compile_options(example_storage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(example_storage PRIVATE ${CMAKE_SOURCE_DIR})

# Optional: Build keystorage example
add_executable(example_keystorage 
    keystorage/example_usage.cpp
)

target_compile_features(example_keystorage PRIVATE cxx_std_20)
target_compile_options(example_keystorage PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(example_keystorage PRIVATE ${CMAKE_SOURCE_DIR})
