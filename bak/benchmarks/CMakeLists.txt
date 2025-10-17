# Modbus Benchmarks

cmake_minimum_required(VERSION 3.15)

# Option to enable benchmarks
option(MODBUS_BUILD_BENCHMARKS "Build performance benchmarks" OFF)

if(NOT MODBUS_BUILD_BENCHMARKS)
    return()
endif()

message(STATUS "Building Modbus benchmarks")

# Benchmark executable
    add_executable(modbus_benchmarks
        main_bench.c
        bench_common.c
        bench_baseline.c
        bench_crc.c
        bench_encode.c
        bench_poll.c
        # Future benchmark files (Phase 3):
        # bench_decode.c
        # bench_rtu.c
        # bench_tcp.c
    )

target_include_directories(modbus_benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/modbus/include
)

# Link to modbus library
target_link_libraries(modbus_benchmarks PRIVATE modbus)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    # Linux: need -lrt for clock_gettime (older glibc)
    target_link_libraries(modbus_benchmarks PRIVATE rt)
elseif(WIN32)
    # Windows: already have QueryPerformanceCounter in kernel32.lib
endif()

# Compiler flags for accurate benchmarking
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(modbus_benchmarks PRIVATE
        -O3                  # Maximum optimization
        -march=native        # Use native CPU instructions
        -fno-omit-frame-pointer  # Better profiling
    )
elseif(MSVC)
    target_compile_options(modbus_benchmarks PRIVATE
        /O2                  # Maximum optimization
        /Oi                  # Enable intrinsics
    )
endif()

# Install benchmark binary (optional)
install(TARGETS modbus_benchmarks
    RUNTIME DESTINATION bin
    COMPONENT benchmarks
)

# Add a custom target to run benchmarks
add_custom_target(run_benchmarks
    COMMAND modbus_benchmarks
    DEPENDS modbus_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Modbus benchmarks..."
    VERBATIM
)

# Add target to export JSON
add_custom_target(benchmark_json
    COMMAND modbus_benchmarks --json ${CMAKE_BINARY_DIR}/benchmark_results.json
    DEPENDS modbus_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running benchmarks and exporting to JSON..."
    VERBATIM
)

message(STATUS "Benchmark targets:")
message(STATUS "  - modbus_benchmarks (executable)")
message(STATUS "  - run_benchmarks (custom target)")
message(STATUS "  - benchmark_json (export to JSON)")
