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
)

if(MODBUS_ENABLE_INTEGRATION_TESTS)
  list(APPEND TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_transport.cpp
  )
endif()

# Iterate over each test source file and create an executable
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
endforeach()
