cmake_minimum_required(VERSION 3.10...3.13)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wswitch-enum -Wcast-qual -Woverflow -Wunused-parameter")
set(CMAKE_CPP_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wswitch-enum -Wcast-qual -Woverflow -Wunused-parameter")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -g0")

# Set project name and version
project(Modbus VERSION 1.2.3)

# Set environment variables
include(${PROJECT_SOURCE_DIR}/cmake/SetEnv.cmake)

# Library sources
add_subdirectory(${LIBRARY_FOLDER})

# Library examples
add_subdirectory(examples/)
# add_subdirectory(examples/win/client)

# enable_testing()
include(CTest)

# # add the tests
add_subdirectory(tests)

# Find required packages
find_package(Doxygen REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Set Doxygen input and output directories
set(DOXYGEN_INPUT "${CMAKE_SOURCE_DIR}/modbus/include" "${CMAKE_SOURCE_DIR}/modbus/src")
set(DOXYGEN_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/doc/xml")
set(DOXYGEN_CONFIG "${CMAKE_BINARY_DIR}/Doxyfile")

# Configure the Doxyfile
configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${DOXYGEN_CONFIG} @ONLY)

# Add custom target to generate Doxygen documentation
add_custom_target(doc_doxygen
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating Doxygen XML documentation"
    VERBATIM
)

# Add custom target to generate Sphinx documentation
add_custom_target(doc_sphinx
    COMMAND ${Python3_EXECUTABLE} -m sphinx -b html ${CMAKE_SOURCE_DIR}/doc/source ${CMAKE_SOURCE_DIR}/doc/build
    DEPENDS doc_doxygen
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating Sphinx HTML documentation"
    VERBATIM
)

# Add a main documentation target that depends on both Doxygen and Sphinx
add_custom_target(doc
    DEPENDS doc_sphinx
    COMMENT "Generating full project documentation"
)


