cmake_minimum_required(VERSION 3.20)
project(PackageTest C)

find_package(ModbusCore REQUIRED CONFIG)

add_executable(test_package test_package.c)
target_link_libraries(test_package ModbusCore::modbuscore)

enable_testing()
add_test(NAME test_package COMMAND test_package)
