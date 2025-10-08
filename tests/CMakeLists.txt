if(NOT BUILD_TESTING)
  return()
endif()

if(MODBUS_USE_SYSTEM_GTEST)
  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND)
    find_package(GTest REQUIRED)
  endif()
else()
  include(FetchContent)

  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/release-1.12.1.zip
  )

  # Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)
endif()

set(_modbus_gtest_main_candidates
  GTest::gtest_main
  GTest::Main
  gtest_main
  GTest::gtest-main
)
set(MODBUS_GTEST_MAIN_TARGET "")
foreach(_candidate IN LISTS _modbus_gtest_main_candidates)
  if(TARGET ${_candidate})
    set(MODBUS_GTEST_MAIN_TARGET ${_candidate})
    break()
  endif()
endforeach()

if(NOT MODBUS_GTEST_MAIN_TARGET)
  message(FATAL_ERROR "Unable to locate a gtest_main target; provide GTest via MODBUS_USE_SYSTEM_GTEST or allow FetchContent downloads.")
endif()

set(TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/test_fsm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_crc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_utils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_ringbuf.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mempool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_pdu_codec.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mb_log.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mb_err.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_port_bare.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_port_freertos.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mb_iovec.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_iovec_integration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mb_queue.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mb_txpool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mb_isr.cpp  # Gate 23: ISR-safe mode
)

# Add concurrency tests when ThreadSanitizer is enabled
if(MODBUS_ENABLE_TSAN)
  list(APPEND TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_ringbuf_concurrent.cpp)
endif()

if(MODBUS_ENABLE_TRANSPORT_RTU)
  list(APPEND TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_transport_if.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rtu_transport.cpp
  )
endif()

if(MODBUS_ENABLE_TRANSPORT_ASCII)
  list(APPEND TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_ascii_transport.cpp)
endif()

if(MODBUS_ENABLE_TRANSPORT_TCP)
  list(APPEND TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tcp_transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tcp_multi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tcp_throughput.cpp
  )
endif()

if(NOT WIN32)
  list(APPEND TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_port_posix.cpp)
endif()

if(MODBUS_ENABLE_INTEGRATION_TESTS)
  list(APPEND TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_client_server_integration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_client_convenience.cpp
  )
endif()

# Iterate over each test source file and create an executable
set(MODBUS_TEST_TARGETS)

foreach(TEST_SRC ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SRC} modbus_transport.c)

  set_target_properties(${TEST_NAME}
    PROPERTIES
      C_STANDARD 11
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS OFF
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
  )

  target_link_libraries(${TEST_NAME} PRIVATE ${MODBUS_GTEST_MAIN_TARGET} modbus)

  if(MODBUS_WARN_FLAGS)
    target_compile_options(${TEST_NAME} PRIVATE ${MODBUS_WARN_FLAGS})
  endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    list(APPEND MODBUS_TEST_TARGETS ${TEST_NAME})
endforeach()

if(MODBUS_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()

if(MODBUS_ENABLE_COVERAGE)
  find_program(LCOV_EXECUTABLE lcov)
  find_program(GENHTML_EXECUTABLE genhtml)

  if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
  set(_coverage_ignore_errors "inconsistent,unsupported,format,unused")
    set(_coverage_capture_directories
      --directory ${CMAKE_BINARY_DIR}/modbus
    )

    add_custom_target(coverage
      COMMAND ${LCOV_EXECUTABLE} --directory ${CMAKE_BINARY_DIR} --zerocounters
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  COMMAND ${LCOV_EXECUTABLE} --capture ${_coverage_capture_directories} --output-file coverage.info --ignore-errors ${_coverage_ignore_errors}
      COMMAND ${GENHTML_EXECUTABLE} coverage.info --output-directory coverage_html
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      USES_TERMINAL
      COMMENT "Running unit tests and generating coverage report"
    )

    if(MODBUS_TEST_TARGETS)
      add_dependencies(coverage ${MODBUS_TEST_TARGETS})
    endif()
  else()
    message(WARNING "Coverage reports require both 'lcov' and 'genhtml' in PATH.")
  endif()
endif()
