cmake_minimum_required(VERSION 3.5.0)
project(modbus VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wswitch-enum -Wcast-qual -Woverflow")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -g0")

set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message("Project_Name: ${PROJECT_NAME}")
message("Project_Dir: ${PROJECT_DIR}")

include_directories(tests examples/win lib)

# define the app target
set(SOURCE 
    lib/fsm.c
    lib/modbus.c 
    lib/client.c
    lib/server.c  
    lib/helpers.c    
    )
add_executable(modbus ${SOURCE} examples/win/main.c )


# add_executable(example_modbus_client ${SOURCE} examples/win/client.c )

include(CTest)
enable_testing()

# add the tests
add_subdirectory(tests)
