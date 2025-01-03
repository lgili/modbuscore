cmake_minimum_required(VERSION 3.10)

project(Modbus_Win_Examples)

# Add subdirectories for server examples
add_subdirectory(server/uart)
add_subdirectory(client/uart)