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

set(MODBUSCORE_SOURCES
  src/runtime/runtime.c
  src/runtime/runtime_builder.c
  src/transport/iface.c
  src/transport/mock.c
  src/transport/rtu_uart.c
  src/protocol/pdu.c
  src/protocol/engine.c
  src/protocol/mbap.c
)

# Platform-specific transport implementations
if(WIN32)
  list(APPEND MODBUSCORE_SOURCES
    src/transport/winsock_tcp.c
    src/transport/win32_rtu.c
  )
else()
  # POSIX systems (Linux, macOS, BSD, etc.)
  list(APPEND MODBUSCORE_SOURCES
    src/transport/posix_tcp.c
    src/transport/posix_rtu.c
  )
endif()

add_library(modbuscore STATIC ${MODBUSCORE_SOURCES})

add_library(ModbusCore::modbuscore ALIAS modbuscore)

target_include_directories(modbuscore
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(modbuscore PUBLIC c_std_17)

# Compiler flags for better compatibility
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(modbuscore PRIVATE
    -Wall
    -Wextra
    $<$<CONFIG:Release>:-fno-strict-aliasing>
  )
endif()

# Platform-specific libraries
if(WIN32)
  target_link_libraries(modbuscore PUBLIC ws2_32)
endif()

if(BUILD_TESTING)
  include(CTest)
  add_subdirectory(tests)
endif()
