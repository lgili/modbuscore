if(NOT BUILD_TESTING)
  return()
endif()

# Include FetchContent module
include(FetchContent)

# Declare the dependency on GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.12.1.zip
)

# Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Fetch and make available GoogleTest
FetchContent_MakeAvailable(googletest)

# Now, GTest targets are available as GTest::gtest and GTest::gtest_main

set(TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/test_fsm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_core.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_crc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_utils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_ringbuf.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_mempool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_pdu_codec.cpp
)

if(MODBUS_ENABLE_INTEGRATION_TESTS)
  list(APPEND TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_transport.cpp
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

  target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main modbus)

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
