cmake_minimum_required(VERSION 3.20)

project(ModbusCore
  VERSION 0.1.0
  LANGUAGES C
  DESCRIPTION "Next-generation Modbus runtime with dependency injection"
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Enable building tests" ON)

add_library(modbuscore STATIC
  src/runtime/runtime.c
  src/runtime/runtime_builder.c
  src/transport/iface.c
  src/protocol/pdu.c
  src/protocol/engine.c
)

add_library(ModbusCore::modbuscore ALIAS modbuscore)

target_include_directories(modbuscore
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(modbuscore PUBLIC c_std_17)

if(BUILD_TESTING)
  include(CTest)
  add_subdirectory(tests)
endif()
