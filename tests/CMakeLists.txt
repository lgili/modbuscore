cmake_minimum_required(VERSION 3.14)

# Enable testing
enable_testing()

# Include the Modbus library
# find_package(modbus_rtu REQUIRED)

# Include FetchContent module
include(FetchContent)

# Declare the dependency on GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.12.1.zip
)

# Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Fetch and make available GoogleTest
FetchContent_MakeAvailable(googletest)

# Now, GTest targets are available as GTest::gtest and GTest::gtest_main

# Collect all test source files
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Iterate over each test source file and create an executable
foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC} modbus_transport.c)
    target_link_libraries(${TEST_NAME} gtest_main modbus)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
