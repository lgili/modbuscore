# cmake/toolchains/esp32c3.cmake
# Toolchain file for ESP32-C3 (RISC-V 32-bit)
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/esp32c3.cmake ...
#
# Or use with preset:
#   cmake --preset esp32c3-lean
#
# Note: For full ESP-IDF integration, use idf.py build instead.
# This toolchain is for standalone builds without ESP-IDF framework.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

# Toolchain prefix
set(TOOLCHAIN_PREFIX riscv32-esp-elf-)

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
    message(WARNING "ESP32-C3 RISC-V toolchain not found.")
    message(WARNING "Install from: https://github.com/espressif/crosstool-NG/releases")
    message(WARNING "Or use ESP-IDF: idf.py build")
endif()

# Processor-specific flags for ESP32-C3 (RISC-V IMC)
set(CPU_FLAGS "-march=rv32imc -mabi=ilp32")

# Common flags
set(COMMON_FLAGS "${CPU_FLAGS} -fdata-sections -ffunction-sections -fno-common")
set(COMMON_FLAGS "${COMMON_FLAGS} -Wall -Wextra -Wpedantic")

# ESP32-C3 specific defines
set(COMMON_FLAGS "${COMMON_FLAGS} -DESP32 -DESP32C3")

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
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS} -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,--print-memory-usage")

# Generate map file
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,-Map=\${TARGET}.map")

# Search path configuration
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Target-specific configuration for Modbus
set(MODBUS_TARGET_ARCH "riscv32imc" CACHE STRING "Target architecture")
set(MODBUS_TARGET_FAMILY "esp32c3" CACHE STRING "Target MCU family")

# Recommended Modbus configuration for ESP32-C3
# (Can be overridden by user)
set(MB_CONF_PROFILE "LEAN" CACHE STRING "Modbus profile for ESP32-C3")
set(MB_CONF_BUILD_CLIENT "ON" CACHE BOOL "Enable client")
set(MB_CONF_BUILD_SERVER "ON" CACHE BOOL "Enable server")
set(MB_CONF_TRANSPORT_TCP "ON" CACHE BOOL "TCP for WiFi connectivity")
set(MB_CONF_TRANSPORT_RTU "ON" CACHE BOOL "RTU for UART")
set(MB_CONF_QUEUE_DEPTH "16" CACHE STRING "Medium queue for balanced RAM")

message(STATUS "ESP32-C3 toolchain configured")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  CPU: RISC-V 32-bit IMC")
message(STATUS "  Recommended Profile: ${MB_CONF_PROFILE}")
message(STATUS "  Note: For full ESP-IDF integration, use idf.py build")
