# cmake/toolchains/nrf52.cmake
# Toolchain file for nRF52 series (Cortex-M4F)
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/nrf52.cmake ...
#
# Or use with preset:
#   cmake --preset nrf52-full
#
# Supports: nRF52832, nRF52833, nRF52840

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain prefix
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

# Processor-specific flags for nRF52 (Cortex-M4F with FPU)
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")

# Common flags for all build types
set(COMMON_FLAGS "${CPU_FLAGS} -fdata-sections -ffunction-sections -fno-common")
set(COMMON_FLAGS "${COMMON_FLAGS} -Wall -Wextra -Wpedantic")

# nRF52-specific defines
set(COMMON_FLAGS "${COMMON_FLAGS} -DNRF52 -DNRF52832")
# Uncomment for specific variants:
# set(COMMON_FLAGS "${COMMON_FLAGS} -DNRF52833")
# set(COMMON_FLAGS "${COMMON_FLAGS} -DNRF52840")

# C flags
set(CMAKE_C_FLAGS_INIT "${COMMON_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG_INIT "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "-O2 -g")

# C++ flags
set(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Og -g3")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-O2 -g")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS} -Wl,--gc-sections -specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,--print-memory-usage")

# Use newlib-nano for smaller footprint
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -lc -lnosys")

# Generate map file
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,-Map=\${TARGET}.map")

# Search path configuration
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Target-specific configuration for Modbus
set(MODBUS_TARGET_ARCH "cortex-m4f" CACHE STRING "Target architecture")
set(MODBUS_TARGET_FAMILY "nrf52" CACHE STRING "Target MCU family")

# Recommended Modbus configuration for nRF52
# (Can be overridden by user)
set(MB_CONF_PROFILE "FULL" CACHE STRING "Modbus profile for capable M4F")
set(MB_CONF_BUILD_CLIENT "ON" CACHE BOOL "Enable client")
set(MB_CONF_BUILD_SERVER "ON" CACHE BOOL "Enable server")
set(MB_CONF_TRANSPORT_TCP "ON" CACHE BOOL "TCP for BLE gateway scenarios")
set(MB_CONF_TRANSPORT_RTU "ON" CACHE BOOL "RTU for UART")
set(MB_CONF_QUEUE_DEPTH "32" CACHE STRING "Large queue for M4F RAM")
set(MB_CONF_DIAG_ENABLE_COUNTERS "ON" CACHE BOOL "Enable diagnostics")
set(MB_CONF_DIAG_ENABLE_TRACE "ON" CACHE BOOL "Enable trace buffer")

message(STATUS "nRF52 toolchain configured")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  CPU: Cortex-M4F with FPU")
message(STATUS "  Recommended Profile: ${MB_CONF_PROFILE}")
