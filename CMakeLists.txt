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

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(MODBUSCORE_SOURCES
  src/runtime/autoheal.c
  src/runtime/runtime.c
  src/runtime/runtime_builder.c
  src/transport/iface.c
  src/transport/mock.c
  src/transport/rtu_uart.c
  src/protocol/pdu.c
  src/protocol/engine.c
  src/protocol/mbap.c
  src/protocol/crc.c
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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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

install(TARGETS modbuscore
  EXPORT ModbusCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

export(EXPORT ModbusCoreTargets
  NAMESPACE ModbusCore::
  FILE ${CMAKE_CURRENT_BINARY_DIR}/ModbusCoreTargets.cmake
)

set(MODBUSCORE_CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/ModbusCore)

configure_package_config_file(
  ${PROJECT_SOURCE_DIR}/cmake/ModbusCoreConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ModbusCoreConfig.cmake
  INSTALL_DESTINATION ${MODBUSCORE_CONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ModbusCoreConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(
  EXPORT ModbusCoreTargets
  NAMESPACE ModbusCore::
  DESTINATION ${MODBUSCORE_CONFIG_INSTALL_DIR}
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ModbusCoreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ModbusCoreConfigVersion.cmake
  DESTINATION ${MODBUSCORE_CONFIG_INSTALL_DIR}
)

if(BUILD_TESTING)
  include(CTest)
  add_subdirectory(tests)
endif()

add_subdirectory(examples)
