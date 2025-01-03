# examples/win/server/tcp/CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

project(Modbus_Slave_TCP_Example)

# Specify the path to the Modbus library
# Adjust the path below to point to where your Modbus library is located
set(MODBUS_LIB_DIR "${CMAKE_SOURCE_DIR}/../../../lib")

# Include directories for Modbus library and transport
include_directories(
    "${MODBUS_LIB_DIR}/include"
    "${CMAKE_SOURCE_DIR}/../../../"
)

# Source files for the TCP transport and the example
set(SOURCES
    main.c
    "${CMAKE_SOURCE_DIR}/../../../tcp_windows.c"
)

# Add executable for the Modbus Slave TCP example
add_executable(slave_tcp_example ${SOURCES})

# Link against the Modbus library and necessary system libraries
# Replace 'modbus_library' with the actual target name of your Modbus library
target_link_libraries(slave_tcp_example modbus_library ws2_32)
