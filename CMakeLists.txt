cmake_minimum_required(VERSION 3.20)

project(Modbus VERSION 1.0.0 LANGUAGES C CXX)

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
option(MODBUS_ENABLE_CLIENT "Build Modbus client components." ON)
option(MODBUS_ENABLE_SERVER "Build Modbus server components." ON)
option(MODBUS_ENABLE_TRANSPORT_RTU "Build Modbus RTU transport components." ON)
option(MODBUS_ENABLE_TRANSPORT_ASCII "Build Modbus ASCII transport components." ON)
option(MODBUS_ENABLE_TRANSPORT_TCP "Build Modbus TCP transport components." ON)
option(MODBUS_PEDANTIC_WARNINGS "Add -Wconversion/-Wsign-conversion diagnostics." OFF)
option(MODBUS_USE_SYSTEM_GTEST "Use an existing GTest installation instead of downloading it." OFF)

if(NOT MODBUS_ENABLE_CLIENT AND NOT MODBUS_ENABLE_SERVER)
    message(FATAL_ERROR "At least one of MODBUS_ENABLE_CLIENT or MODBUS_ENABLE_SERVER must be ON.")
endif()

if(NOT MODBUS_ENABLE_TRANSPORT_RTU AND NOT MODBUS_ENABLE_TRANSPORT_ASCII AND NOT MODBUS_ENABLE_TRANSPORT_TCP)
    message(FATAL_ERROR "At least one transport must be enabled. Enable RTU, ASCII, TCP or a custom backend.")
endif()

if(MODBUS_ENABLE_SERVER AND NOT MODBUS_ENABLE_TRANSPORT_RTU)
    message(FATAL_ERROR "Modbus server support currently relies on the RTU transport. Enable MODBUS_ENABLE_TRANSPORT_RTU or disable MODBUS_ENABLE_SERVER.")
endif()

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
    if(MODBUS_SANITIZERS AND NOT MODBUS_ENABLE_COVERAGE)
        string(JOIN "," MODBUS_SANITIZER_FLAGS ${MODBUS_SANITIZERS})
        add_compile_options(-fsanitize=${MODBUS_SANITIZER_FLAGS})
        add_link_options(-fsanitize=${MODBUS_SANITIZER_FLAGS})
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
