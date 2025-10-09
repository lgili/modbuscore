# cmake/toolchains/stm32g0.cmake
# Toolchain file for STM32G0 series (Cortex-M0+)
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/stm32g0.cmake ...
#
# Or use with preset:
#   cmake --preset stm32g0-tiny

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain paths
# Modify these paths according to your toolchain installation
set(TOOLCHAIN_PREFIX arm-none-eabi-)

# Search for the compiler
find_program(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
find_program(CMAKE_AR ${TOOLCHAIN_PREFIX}ar)
find_program(CMAKE_RANLIB ${TOOLCHAIN_PREFIX}ranlib)
find_program(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
find_program(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}objdump)
find_program(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR "ARM GCC toolchain not found. Please install arm-none-eabi-gcc")
endif()

# Processor-specific flags for STM32G0 (Cortex-M0+)
set(CPU_FLAGS "-mcpu=cortex-m0plus -mthumb")

# Common flags for all build types
set(COMMON_FLAGS "${CPU_FLAGS} -fdata-sections -ffunction-sections -fno-common")
set(COMMON_FLAGS "${COMMON_FLAGS} -Wall -Wextra -Wpedantic")

# C flags
set(CMAKE_C_FLAGS_INIT "${COMMON_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG_INIT "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "-O2 -g")

# C++ flags (if needed)
set(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Og -g3")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-O2 -g")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS} -Wl,--gc-sections -specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,--print-memory-usage")

# Generate map file
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,-Map=\${TARGET}.map")

# Search path configuration
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Target-specific configuration for Modbus
set(MODBUS_TARGET_ARCH "cortex-m0plus" CACHE STRING "Target architecture")
set(MODBUS_TARGET_FAMILY "stm32g0" CACHE STRING "Target MCU family")

# Recommended Modbus configuration for STM32G0
# (Can be overridden by user)
set(MB_CONF_PROFILE "TINY" CACHE STRING "Modbus profile for resource-constrained M0+")
set(MB_CONF_BUILD_SERVER "OFF" CACHE BOOL "Disable server on TINY profile")
set(MB_CONF_TRANSPORT_TCP "OFF" CACHE BOOL "No TCP on baremetal")
set(MB_CONF_TRANSPORT_RTU "ON" CACHE BOOL "RTU is primary for serial MCUs")
set(MB_CONF_QUEUE_DEPTH "8" CACHE STRING "Small queue for limited RAM")

message(STATUS "STM32G0 toolchain configured")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  CPU: Cortex-M0+")
message(STATUS "  Recommended Profile: ${MB_CONF_PROFILE}")
