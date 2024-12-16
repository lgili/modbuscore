# set(CMAKE_C_STANDARD 99)
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wswitch-enum -Wcast-qual -Woverflow")
# set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
# set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -g0")


# Set minimum version of CMake.
cmake_minimum_required(VERSION 3.10)

# Set project name and version
project(Modbus VERSION 1.2.3)

# Set environment variables
include(${PROJECT_SOURCE_DIR}/cmake/SetEnv.cmake)

# Library sources
add_subdirectory(${LIBRARY_FOLDER})

# Library examples
add_subdirectory(examples/win/server)
add_subdirectory(examples/win/client)

include(CTest)
# enable_testing()

# # add the tests
add_subdirectory(tests)
