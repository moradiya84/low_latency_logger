cmake_minimum_required(VERSION 3.16)
project(low_latency_logger LANGUAGES CXX)

# Require C++17 for inline variables and hardware interference size
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/internal)

# Add library (update with actual source files as you create them)
# add_library(low_latency_logger src/logger.cpp)

# Enable testing
# enable_testing()
# add_subdirectory(tests)
# add_subdirectory(benchmarks)
