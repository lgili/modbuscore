cmake_minimum_required(VERSION 3.10)

project(Modbus_Slave_UART_Example)

# Specify the path to the Modbus library
set(MODBUS_LIB_DIR "${CMAKE_SOURCE_DIR}/modbus")

# Include directories for Modbus library and transport
include_directories(
    "${MODBUS_LIB_DIR}/include"
    "${CMAKE_SOURCE_DIR}/examples/win"
)

# Source files for the UART transport and the example
set(SOURCES
    main.c
    "${CMAKE_SOURCE_DIR}/examples/win/uart_windows.c"
)

# Add executable for the Modbus Slave UART example
add_executable(master_uart_example ${SOURCES})

# Link against the Modbus library and necessary system libraries
# Replace 'modbus_library' with the actual target name of your Modbus library
target_link_libraries(master_uart_example modbus ws2_32)
