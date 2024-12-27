# # see /opt/local/share/cmake-3.16/Modules/FindGTest.cmake
# # find_package(GTest QUIET)

# # # NOTE: the upper case GTEST! CK
# # if(NOT GTEST_FOUND)
# #   # Download and unpack googletest at configure time
# #   # but only if needed! CK
# #   configure_file(${CMAKE_SOURCE_DIR}/cmake/GoogleTest-CMakeLists.txt.in
# #     ${CMAKE_BINARY_DIR}/googletest-download/CMakeLists.txt)
# #   execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
# #     RESULT_VARIABLE result
# #     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
# #   if(result)
# #     message(FATAL_ERROR "CMake step for googletest failed: ${result}")
# #   endif()

# #   execute_process(COMMAND ${CMAKE_COMMAND} --build .
# #     RESULT_VARIABLE result
# #     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
# #   if(result)
# #     message(FATAL_ERROR "Build step for googletest failed: ${result}")
# #   endif()

# #   # Add googletest directly to our build. This defines
# #   # the gtest and gtest_main targets.
# #   add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
# #     ${CMAKE_BINARY_DIR}/googletest-build
# #     EXCLUDE_FROM_ALL)
  

# # endif()




# # Tenta encontrar o GTest instalado no sistema ou no CMAKE_PREFIX_PATH
# find_package(GTest QUIET)

# if(NOT GTEST_FOUND)
#   # Caminho onde GTest será instalado
#   set(GTEST_INSTALL_DIR "${CMAKE_BINARY_DIR}/gtest_install")

#   if(NOT EXISTS "${GTEST_INSTALL_DIR}/lib" OR NOT EXISTS "${GTEST_INSTALL_DIR}/include")
#     # Ainda não baixamos nem instalamos o GTest, então vamos fazer isso agora.

#     # Gerar o CMakeLists para baixar e configurar o gtest
#     configure_file(${CMAKE_SOURCE_DIR}/cmake/GoogleTest-CMakeLists.txt.in
#       ${CMAKE_BINARY_DIR}/googletest-download/CMakeLists.txt
#       @ONLY)

#     # Passo 1: Baixar o GTest
#     execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
#       RESULT_VARIABLE result
#       WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
#     if(result)
#       message(FATAL_ERROR "CMake step for googletest download failed: ${result}")
#     endif()

#     execute_process(COMMAND ${CMAKE_COMMAND} --build .
#       RESULT_VARIABLE result
#       WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
#     if(result)
#       message(FATAL_ERROR "Build step for googletest download failed: ${result}")
#     endif()

#     # Passo 2: Configurar o GTest baixado
#     # Vamos criar uma subpasta de build para o gtest
#     set(GTEST_SRC_DIR "${CMAKE_BINARY_DIR}/googletest-download/googletest-src")
#     set(GTEST_BUILD_DIR "${CMAKE_BINARY_DIR}/googletest-download/googletest-build")

#     file(MAKE_DIRECTORY ${GTEST_BUILD_DIR})
#     execute_process(COMMAND ${CMAKE_COMMAND}
#       -DCMAKE_INSTALL_PREFIX=${GTEST_INSTALL_DIR}
#       -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
#       ${GTEST_SRC_DIR}
#       WORKING_DIRECTORY ${GTEST_BUILD_DIR}
#     )

#     # Compilar e instalar o GTest
#     execute_process(COMMAND ${CMAKE_COMMAND} --build .
#       WORKING_DIRECTORY ${GTEST_BUILD_DIR}
#     )
#     execute_process(COMMAND ${CMAKE_COMMAND} --build . --target install
#       WORKING_DIRECTORY ${GTEST_BUILD_DIR}
#     )
#   endif()

#   # Agora que GTest está instalado localmente no GTEST_INSTALL_DIR, apontamos o CMAKE_PREFIX_PATH
#   set(CMAKE_PREFIX_PATH "${GTEST_INSTALL_DIR};${CMAKE_PREFIX_PATH}")

#   # Tentar novamente encontrar o GTest
#   # find_package(GTest REQUIRED)
# endif()

# # Agora já temos GTest garantidamente encontrado
# # Criar executáveis de teste
# file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# # foreach(TEST_SRC ${TEST_SOURCES})
# #     get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
# #     add_executable(${TEST_NAME} ${TEST_SRC})
# #     target_link_libraries(${TEST_NAME} modbus_rtu  GTest::GTest GTest::Main)
# #     add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
# # endforeach()


# # # Now simply link against gtest as needed. Eg
# add_executable(modbus_test test_modbus_utils.cpp)
# target_compile_features(modbus_test PRIVATE cxx_auto_type)
# target_link_libraries(modbus_test gtest_main Modbus::modbus)

# add_test(NAME modbus_test
#     COMMAND modbus_test)

cmake_minimum_required(VERSION 3.14)

# Enable testing
enable_testing()

# Include the Modbus library
# find_package(modbus_rtu REQUIRED)

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

# Collect all test source files
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Iterate over each test source file and create an executable
foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} gtest_main modbus)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
