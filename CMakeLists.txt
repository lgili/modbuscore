cmake_minimum_required(VERSION 3.20)

project(ModbusCore 
    VERSION 1.0.0 
    DESCRIPTION "The Core of Modern Modbus Communication"
    HOMEPAGE_URL "https://github.com/lgili/modbus"
    LANGUAGES C CXX
)

# The library folder is 'modbus', not 'modbuscore'
set(LIBRARY_FOLDER "modbus")
set(LIBRARY_NAME "modbus")

# ---------------------------------------------------------------------------
# High-level configuration profiles (pre-Gate 17 scaffolding)
# ---------------------------------------------------------------------------
set(MODBUS_PROFILE "LEAN" CACHE STRING "Select the Modbus configuration profile (TINY, LEAN, FULL, CUSTOM).")
set_property(CACHE MODBUS_PROFILE PROPERTY STRINGS TINY LEAN FULL CUSTOM)

set(_MB_CONF_PROFILE_ID 1) # LEAN by default

set(_MODBUS_DEFAULT_ENABLE_CLIENT ON)
set(_MODBUS_DEFAULT_ENABLE_SERVER ON)
set(_MODBUS_DEFAULT_TRANSPORT_RTU ON)
set(_MODBUS_DEFAULT_TRANSPORT_ASCII ON)
set(_MODBUS_DEFAULT_TRANSPORT_TCP ON)

set(_MB_CONF_ENABLE_FC01 1)
set(_MB_CONF_ENABLE_FC02 1)
set(_MB_CONF_ENABLE_FC03 1)
set(_MB_CONF_ENABLE_FC04 1)
set(_MB_CONF_ENABLE_FC05 1)
set(_MB_CONF_ENABLE_FC06 1)
set(_MB_CONF_ENABLE_FC07 1)
set(_MB_CONF_ENABLE_FC0F 1)
set(_MB_CONF_ENABLE_FC10 1)
set(_MB_CONF_ENABLE_FC11 1)
set(_MB_CONF_ENABLE_FC16 1)
set(_MB_CONF_ENABLE_FC17 1)

if(MODBUS_PROFILE STREQUAL "TINY")
    set(_MB_CONF_PROFILE_ID 0)
    set(_MODBUS_DEFAULT_ENABLE_SERVER OFF)
    set(_MODBUS_DEFAULT_TRANSPORT_ASCII OFF)
    set(_MODBUS_DEFAULT_TRANSPORT_TCP OFF)

    set(_MB_CONF_ENABLE_FC01 0)
    set(_MB_CONF_ENABLE_FC02 0)
    set(_MB_CONF_ENABLE_FC04 1)
    set(_MB_CONF_ENABLE_FC05 0)
    set(_MB_CONF_ENABLE_FC07 0)
    set(_MB_CONF_ENABLE_FC0F 0)
    set(_MB_CONF_ENABLE_FC11 0)
    set(_MB_CONF_ENABLE_FC16 0)
    set(_MB_CONF_ENABLE_FC17 1)
elseif(MODBUS_PROFILE STREQUAL "LEAN")
    set(_MB_CONF_PROFILE_ID 1)
    set(_MODBUS_DEFAULT_TRANSPORT_ASCII OFF)

    set(_MB_CONF_ENABLE_FC07 0)
    set(_MB_CONF_ENABLE_FC11 0)
    set(_MB_CONF_ENABLE_FC16 0)
elseif(MODBUS_PROFILE STREQUAL "FULL")
    set(_MB_CONF_PROFILE_ID 2)
elseif(MODBUS_PROFILE STREQUAL "CUSTOM")
    set(_MB_CONF_PROFILE_ID 3)
else()
    message(FATAL_ERROR "Unknown MODBUS_PROFILE value: ${MODBUS_PROFILE}")
endif()

function(_mb_define_fc_option NAME DESCRIPTION DEFAULT_VALUE)
    if(DEFAULT_VALUE)
        set(_default ON)
    else()
        set(_default OFF)
    endif()

    option(MODBUS_ENABLE_${NAME} "${DESCRIPTION}" ${_default})

    if(MODBUS_ENABLE_${NAME})
        set(_MB_CONF_ENABLE_${NAME} 1 PARENT_SCOPE)
    else()
        set(_MB_CONF_ENABLE_${NAME} 0 PARENT_SCOPE)
    endif()
endfunction()

_mb_define_fc_option(FC01 "Enable Modbus function code 0x01 (Read Coils)." ${_MB_CONF_ENABLE_FC01})
_mb_define_fc_option(FC02 "Enable Modbus function code 0x02 (Read Discrete Inputs)." ${_MB_CONF_ENABLE_FC02})
_mb_define_fc_option(FC03 "Enable Modbus function code 0x03 (Read Holding Registers)." ${_MB_CONF_ENABLE_FC03})
_mb_define_fc_option(FC04 "Enable Modbus function code 0x04 (Read Input Registers)." ${_MB_CONF_ENABLE_FC04})
_mb_define_fc_option(FC05 "Enable Modbus function code 0x05 (Write Single Coil)." ${_MB_CONF_ENABLE_FC05})
_mb_define_fc_option(FC06 "Enable Modbus function code 0x06 (Write Single Register)." ${_MB_CONF_ENABLE_FC06})
_mb_define_fc_option(FC07 "Enable Modbus function code 0x07 (Read Exception Status)." ${_MB_CONF_ENABLE_FC07})
_mb_define_fc_option(FC0F "Enable Modbus function code 0x0F (Write Multiple Coils)." ${_MB_CONF_ENABLE_FC0F})
_mb_define_fc_option(FC10 "Enable Modbus function code 0x10 (Write Multiple Registers)." ${_MB_CONF_ENABLE_FC10})
_mb_define_fc_option(FC11 "Enable Modbus function code 0x11 (Report Server ID)." ${_MB_CONF_ENABLE_FC11})
_mb_define_fc_option(FC16 "Enable Modbus function code 0x16 (Mask Write Register)." ${_MB_CONF_ENABLE_FC16})
_mb_define_fc_option(FC17 "Enable Modbus function code 0x17 (Read/Write Multiple Registers)." ${_MB_CONF_ENABLE_FC17})

# ---------------------------------------------------------------------------
# Build configuration knobs (Gate 0 deliverables)
# ---------------------------------------------------------------------------
option(MODBUS_FORCE_C99 "Force building the library with C99 instead of C11." OFF)
option(MODBUS_STRICT_WARNINGS "Treat compiler warnings as errors." ON)
option(MODBUS_ENABLE_TESTS "Build Modbus unit/integration test targets." ON)
option(MODBUS_ENABLE_INTEGRATION_TESTS "Build extended integration-style tests." OFF)
option(MODBUS_BUILD_EXAMPLES "Build example applications." OFF)
option(MODBUS_BUILD_DOCS "Build API/reference documentation targets." OFF)
option(MODBUS_BUILD_FUZZERS "Build libFuzzer harnesses (requires Clang)." OFF)
option(MODBUS_ENABLE_COVERAGE "Enable code coverage instrumentation (GCC/Clang)." OFF)
option(MODBUS_ENABLE_ASAN "Enable AddressSanitizer when supported." OFF)
option(MODBUS_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer when supported." OFF)
option(MODBUS_ENABLE_TSAN "Enable ThreadSanitizer for concurrency testing." OFF)
option(MODBUS_ENABLE_CLIENT "Build Modbus client components." ${_MODBUS_DEFAULT_ENABLE_CLIENT})
option(MODBUS_ENABLE_SERVER "Build Modbus server components." ${_MODBUS_DEFAULT_ENABLE_SERVER})
option(MODBUS_ENABLE_TRANSPORT_RTU "Build Modbus RTU transport components." ${_MODBUS_DEFAULT_TRANSPORT_RTU})
option(MODBUS_ENABLE_TRANSPORT_ASCII "Build Modbus ASCII transport components." ${_MODBUS_DEFAULT_TRANSPORT_ASCII})
option(MODBUS_ENABLE_TRANSPORT_TCP "Build Modbus TCP transport components." ${_MODBUS_DEFAULT_TRANSPORT_TCP})
option(MODBUS_ENABLE_COMPAT_LIBMODBUS "Enable libmodbus-style compatibility wrappers on top of the host API." OFF)
option(MODBUS_PEDANTIC_WARNINGS "Add -Wconversion/-Wsign-conversion diagnostics." OFF)
option(MODBUS_USE_SYSTEM_GTEST "Use an existing GTest installation instead of downloading it." OFF)
option(MODBUS_INSTALL "Generate install rules and package configuration artifacts." ON)
option(MODBUS_DIAG_ENABLE_COUNTERS "Collect lightweight diagnostic counters." ON)
option(MODBUS_DIAG_ENABLE_TRACE "Enable diagnostic trace ring buffer." OFF)
set(MODBUS_DIAG_TRACE_DEPTH 64 CACHE STRING "Maximum number of diagnostic trace entries to retain.")

set(_MODBUS_DEFAULT_PORT_POSIX ON)
set(_MODBUS_DEFAULT_PORT_MUTEX ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    set(_MODBUS_DEFAULT_PORT_POSIX OFF)
    set(_MODBUS_DEFAULT_PORT_MUTEX OFF)
endif()
option(MODBUS_ENABLE_PORT_POSIX "Build POSIX port helpers (requires sockets)." ${_MODBUS_DEFAULT_PORT_POSIX})
option(MODBUS_ENABLE_PORT_MUTEX "Build generic mutex helper backed by C11 threads/atomics." ${_MODBUS_DEFAULT_PORT_MUTEX})

if(NOT MODBUS_ENABLE_CLIENT AND NOT MODBUS_ENABLE_SERVER)
    message(FATAL_ERROR "At least one of MODBUS_ENABLE_CLIENT or MODBUS_ENABLE_SERVER must be ON.")
endif()

if(NOT MODBUS_ENABLE_TRANSPORT_RTU AND NOT MODBUS_ENABLE_TRANSPORT_ASCII AND NOT MODBUS_ENABLE_TRANSPORT_TCP)
    message(FATAL_ERROR "At least one transport must be enabled. Enable RTU, ASCII, TCP or a custom backend.")
endif()

if(MODBUS_ENABLE_SERVER AND NOT MODBUS_ENABLE_TRANSPORT_RTU)
    message(FATAL_ERROR "Modbus server support currently relies on the RTU transport. Enable MODBUS_ENABLE_TRANSPORT_RTU or disable MODBUS_ENABLE_SERVER.")
endif()

if(MODBUS_DIAG_ENABLE_COUNTERS)
    set(MB_CONF_DIAG_ENABLE_COUNTERS 1)
else()
    set(MB_CONF_DIAG_ENABLE_COUNTERS 0)
endif()

if(MODBUS_DIAG_ENABLE_TRACE)
    set(MB_CONF_DIAG_ENABLE_TRACE 1)
else()
    set(MB_CONF_DIAG_ENABLE_TRACE 0)
endif()

if(MODBUS_ENABLE_COMPAT_LIBMODBUS)
    if(NOT MODBUS_ENABLE_CLIENT)
        message(FATAL_ERROR "MODBUS_ENABLE_COMPAT_LIBMODBUS requires MODBUS_ENABLE_CLIENT=ON.")
    endif()
    set(MB_CONF_ENABLE_COMPAT_LIBMODBUS 1)
else()
    set(MB_CONF_ENABLE_COMPAT_LIBMODBUS 0)
endif()

set(MB_CONF_DIAG_TRACE_DEPTH ${MODBUS_DIAG_TRACE_DEPTH})

if(MODBUS_DIAG_ENABLE_TRACE AND MODBUS_DIAG_TRACE_DEPTH LESS 1)
    message(FATAL_ERROR "MODBUS_DIAG_TRACE_DEPTH must be at least 1 when MODBUS_DIAG_ENABLE_TRACE is ON.")
endif()

if(MODBUS_ENABLE_TESTS)
    if(NOT MODBUS_PROFILE STREQUAL "FULL")
        message(STATUS "Tests enabled; forcing MODBUS_PROFILE=FULL so every function code remains available.")
        set(MODBUS_PROFILE "FULL" CACHE STRING "Select the Modbus configuration profile (TINY, LEAN, FULL, CUSTOM)." FORCE)
    endif()

    set(_MB_CONF_PROFILE_ID 2)
    foreach(_fc IN ITEMS FC01 FC02 FC03 FC04 FC05 FC06 FC07 FC0F FC10 FC11 FC16 FC17)
        set(_MB_CONF_ENABLE_${_fc} 1)
        set(MODBUS_ENABLE_${_fc} ON CACHE BOOL "Enable Modbus function code option" FORCE)
    endforeach()
endif()

set(MB_CONF_PROFILE_ID ${_MB_CONF_PROFILE_ID})
set(MB_CONF_ENABLE_FC01 ${_MB_CONF_ENABLE_FC01})
set(MB_CONF_ENABLE_FC02 ${_MB_CONF_ENABLE_FC02})
set(MB_CONF_ENABLE_FC03 ${_MB_CONF_ENABLE_FC03})
set(MB_CONF_ENABLE_FC04 ${_MB_CONF_ENABLE_FC04})
set(MB_CONF_ENABLE_FC05 ${_MB_CONF_ENABLE_FC05})
set(MB_CONF_ENABLE_FC06 ${_MB_CONF_ENABLE_FC06})
set(MB_CONF_ENABLE_FC07 ${_MB_CONF_ENABLE_FC07})
set(MB_CONF_ENABLE_FC0F ${_MB_CONF_ENABLE_FC0F})
set(MB_CONF_ENABLE_FC10 ${_MB_CONF_ENABLE_FC10})
set(MB_CONF_ENABLE_FC11 ${_MB_CONF_ENABLE_FC11})
set(MB_CONF_ENABLE_FC16 ${_MB_CONF_ENABLE_FC16})
set(MB_CONF_ENABLE_FC17 ${_MB_CONF_ENABLE_FC17})

if(MODBUS_FORCE_C99)
    set(CMAKE_C_STANDARD 99)
else()
    set(CMAKE_C_STANDARD 11)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Ensure POSIX feature-test macros expose required APIs on Unix-like toolchains.
if(UNIX AND NOT APPLE)
    add_compile_definitions(
        _POSIX_C_SOURCE=200809L
        _DEFAULT_SOURCE
    )
endif()

# Propagate warning configuration
if(MSVC)
    set(MODBUS_WARN_FLAGS /W4 /permissive-)
    if(MODBUS_STRICT_WARNINGS)
        list(APPEND MODBUS_WARN_FLAGS /WX)
    endif()
else()
    set(MODBUS_WARN_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wswitch-enum
        -Wcast-qual
        -Wstrict-prototypes
        -Wmissing-prototypes
    -Wformat
    -Wformat-security
        -Wundef
    )
    if(MODBUS_PEDANTIC_WARNINGS)
        list(APPEND MODBUS_WARN_FLAGS -Wconversion -Wsign-conversion)
    endif()
    if(MODBUS_STRICT_WARNINGS)
        list(APPEND MODBUS_WARN_FLAGS -Werror)
    endif()
endif()

set(MODBUS_SANITIZERS)
if(NOT MSVC)
    if(MODBUS_ENABLE_ASAN)
        list(APPEND MODBUS_SANITIZERS address)
    endif()
    if(MODBUS_ENABLE_UBSAN)
        list(APPEND MODBUS_SANITIZERS undefined)
    endif()
    if(MODBUS_ENABLE_TSAN)
        # ThreadSanitizer cannot be combined with ASan/UBSan
        if(MODBUS_ENABLE_ASAN OR MODBUS_ENABLE_UBSAN)
            message(FATAL_ERROR "ThreadSanitizer cannot be combined with AddressSanitizer or UBSan. Use separate builds.")
        endif()
        list(APPEND MODBUS_SANITIZERS thread)
    endif()
    if(MODBUS_SANITIZERS AND NOT MODBUS_ENABLE_COVERAGE)
        string(JOIN "," MODBUS_SANITIZER_FLAGS ${MODBUS_SANITIZERS})
        add_compile_options(-fsanitize=${MODBUS_SANITIZER_FLAGS})
        add_link_options(-fsanitize=${MODBUS_SANITIZER_FLAGS})
        
        # ThreadSanitizer requires -fPIE on some platforms
        if(MODBUS_ENABLE_TSAN AND NOT MSVC)
            add_compile_options(-fPIE)
            add_link_options(-pie)
        endif()
    endif()
endif()

if(MODBUS_ENABLE_COVERAGE)
    if(MSVC)
        message(FATAL_ERROR "Coverage instrumentation currently requires GCC or Clang toolchains.")
    endif()

    if(MODBUS_SANITIZERS)
        message(WARNING "Coverage requested; sanitizer flags will be ignored for accurate reporting.")
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(-O0 -g --coverage)
        # Avoid data races when multiple threads update coverage counters.
        # See lcov/geninfo negative counter errors.
        if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
            add_compile_options(-fprofile-update=atomic)
        else()
            add_compile_options(-fprofile-update=atomic)
        endif()
        add_link_options(--coverage)
        message(STATUS "Modbus coverage instrumentation enabled")
    else()
        message(FATAL_ERROR "Compiler ${CMAKE_C_COMPILER_ID} is not supported for coverage builds.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Hardening flags
# ---------------------------------------------------------------------------
if(NOT MSVC AND (CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "AppleClang"))
    add_compile_options(-fno-common -fstack-protector-strong)
    add_link_options(-fstack-protector-strong)

    if(UNIX AND NOT APPLE)
        add_link_options($<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-Wl,-z,relro,-z,now>)
    endif()

    add_compile_definitions($<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:_FORTIFY_SOURCE=2>)
endif()

# Make warnings available to subdirectories
set(MODBUS_WARN_FLAGS ${MODBUS_WARN_FLAGS} CACHE INTERNAL "Compiler warnings for Modbus targets")

# ---------------------------------------------------------------------------
# Project environment and layout
# ---------------------------------------------------------------------------
include(${PROJECT_SOURCE_DIR}/cmake/SetEnv.cmake)

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_subdirectory(${LIBRARY_FOLDER})

# Expose embedded integration scaffolding (documentation/assets only for now).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/embedded/CMakeLists.txt")
    add_subdirectory(embedded EXCLUDE_FROM_ALL)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(MODBUS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
include(CTest)
if(MODBUS_ENABLE_TESTS)
    set(BUILD_TESTING ON CACHE BOOL "Enable CTest targets" FORCE)
    add_subdirectory(tests)
else()
    set(BUILD_TESTING OFF CACHE BOOL "Enable CTest targets" FORCE)
endif()

if(MODBUS_BUILD_FUZZERS AND NOT MODBUS_ENABLE_TESTS)
    add_subdirectory(tests/fuzz)
endif()

# ---------------------------------------------------------------------------
# Benchmarks (opt-in)
# ---------------------------------------------------------------------------
add_subdirectory(benchmarks)

# ---------------------------------------------------------------------------
# Documentation (opt-in)
# ---------------------------------------------------------------------------
if(MODBUS_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(DOXYGEN_CONFIG "${CMAKE_BINARY_DIR}/Doxyfile")

    configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${DOXYGEN_CONFIG} @ONLY)

    set(DOXYGEN_XML_DIR "${CMAKE_BINARY_DIR}/doxygen/xml")
    set(SPHINX_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/html")

    add_custom_target(doc_doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating Doxygen XML documentation"
        VERBATIM
    )

    add_custom_target(doc_sphinx
        COMMAND ${CMAKE_COMMAND} -E env DOXYGEN_XML_DIR=${DOXYGEN_XML_DIR}
                ${Python3_EXECUTABLE} -m sphinx -b html
                ${CMAKE_SOURCE_DIR}/doc/source ${SPHINX_OUTPUT_DIR}
        DEPENDS doc_doxygen
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating Sphinx HTML documentation"
        VERBATIM
    )

    add_custom_target(doc
        DEPENDS doc_sphinx
        COMMENT "Generating full project documentation"
    )
endif()

# ============================================================================
#  Installation Configuration
# ============================================================================

include(GNUInstallDirs)

# Install library
install(TARGETS modbus
    EXPORT modbusTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/modbus/include/modbus
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Install generated version.h
install(FILES ${CMAKE_BINARY_DIR}/modbus/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/modbus
)

# Install CMake package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/modbusConfig.cmake.in
    ${CMAKE_BINARY_DIR}/modbusConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/modbus
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/modbusConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_BINARY_DIR}/modbusConfig.cmake
    ${CMAKE_BINARY_DIR}/modbusConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/modbus
)

install(EXPORT modbusTargets
    FILE modbusTargets.cmake
    NAMESPACE modbus::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/modbus
)

# Install pkg-config file
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/modbus.pc.in
    ${CMAKE_BINARY_DIR}/modbus.pc
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/modbus.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Install documentation (if built)
if(MODBUS_BUILD_DOCS)
    install(DIRECTORY ${SPHINX_OUTPUT_DIR}/
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        OPTIONAL
    )
endif()

# Install examples (optional)
if(MODBUS_BUILD_EXAMPLES)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/examples/
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples
        FILES_MATCHING
        PATTERN "*.c"
        PATTERN "*.h"
        PATTERN "*.md"
        PATTERN "CMakeLists.txt"
    )
endif()

# Uninstall target
if(NOT TARGET uninstall)
    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/Uninstall.cmake.in
        ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
        IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
        COMMENT "Uninstalling modbus library"
    )
endif()
