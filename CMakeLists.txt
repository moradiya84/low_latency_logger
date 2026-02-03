cmake_minimum_required(VERSION 3.16)
project(low_latency_logger LANGUAGES CXX)

# Require C++17 for inline variables and hardware interference size
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LLL_BUILD_TESTS "Build low_latency_logger tests" ON)
option(LLL_BUILD_BENCHMARKS "Build low_latency_logger benchmarks" OFF)

add_library(low_latency_logger STATIC
    src/formatter.cpp
    src/sink.cpp
    src/clock.cpp
    src/encoder.cpp
)

target_compile_features(low_latency_logger PUBLIC cxx_std_17)
target_include_directories(low_latency_logger
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/internal>
        $<INSTALL_INTERFACE:include>
)

if (LLL_BUILD_TESTS)
    enable_testing()

    add_executable(ring_buffer_test tests/ring_buffer_test.cpp)
    target_link_libraries(ring_buffer_test PRIVATE low_latency_logger)
    add_test(NAME ring_buffer_test COMMAND ring_buffer_test)

    add_executable(ordering_test tests/ordering_test.cpp)
    target_link_libraries(ordering_test PRIVATE low_latency_logger)
    add_test(NAME ordering_test COMMAND ordering_test)

    add_executable(latency_test tests/latency_test.cpp)
    target_link_libraries(latency_test PRIVATE low_latency_logger)
    add_test(NAME latency_test COMMAND latency_test)

    add_executable(test_compile_ringbuffer test_compile_ringbuffer.cc)
    target_link_libraries(test_compile_ringbuffer PRIVATE low_latency_logger)
    add_test(NAME test_compile_ringbuffer COMMAND test_compile_ringbuffer)
endif()

if (LLL_BUILD_BENCHMARKS)
    add_executable(log_throughput benchmarks/log_throughput.cpp)
    target_link_libraries(log_throughput PRIVATE low_latency_logger)
endif()
