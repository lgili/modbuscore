cmake_minimum_required(VERSION 3.15)
project(modbus_cmake_consume LANGUAGES C)

find_package(modbuscore REQUIRED)

add_executable(modbus_cmake_consume main.c)
target_link_libraries(modbus_cmake_consume PRIVATE modbuscore::modbuscore)
