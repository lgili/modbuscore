# ===========================================================================
# FreeRTOS + Modbus RTU Client Example
# ===========================================================================
# This CMakeLists.txt demonstrates how to integrate the Modbus library with
# FreeRTOS on an embedded target (e.g., STM32, ESP32, nRF52).
#
# USAGE:
#   1. Copy this file to your project root
#   2. Adjust paths to FreeRTOS and Modbus library
#   3. Configure toolchain file for your target (e.g., arm-none-eabi)
#   4. Build:
#      mkdir build && cd build
#      cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake ..
#      make
#   5. Flash:
#      make flash  (if target is defined)
# ===========================================================================

cmake_minimum_required(VERSION 3.15)

# ===========================================================================
# PROJECT CONFIGURATION
# ===========================================================================

# IMPORTANT: Set this BEFORE project() to select cross-compiler
# Example toolchain files:
# - cmake/arm-none-eabi.cmake (for ARM Cortex-M)
# - cmake/esp32.cmake (for ESP32)
# - cmake/nrf52.cmake (for Nordic nRF52)
#
# OR pass on command line:
#   cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake ..

project(freertos_modbus_rtu_client
    VERSION 1.0.0
    DESCRIPTION "FreeRTOS + Modbus RTU Client Example"
    LANGUAGES C ASM
)

# ===========================================================================
# TARGET HARDWARE SELECTION
# ===========================================================================
# Set your target MCU here. This determines:
# - Linker script
# - Startup code
# - CPU flags (-mcpu, -mfpu, etc.)
# ===========================================================================

set(MCU_FAMILY "STM32F4" CACHE STRING "MCU family (STM32F4, STM32G0, NRF52, etc.)")
set(MCU_MODEL "STM32F407VG" CACHE STRING "Specific MCU model")

# CPU-specific flags
if(MCU_FAMILY STREQUAL "STM32F4")
    set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32F407VGTx_FLASH.ld")
elseif(MCU_FAMILY STREQUAL "STM32G0")
    set(CPU_FLAGS "-mcpu=cortex-m0plus -mthumb")
    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32G0B1RETx_FLASH.ld")
elseif(MCU_FAMILY STREQUAL "NRF52")
    set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/nrf52840_xxaa.ld")
else()
    message(WARNING "Unknown MCU_FAMILY: ${MCU_FAMILY}. Using default Cortex-M4 flags.")
    set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb")
endif()

# ===========================================================================
# COMPILER FLAGS
# ===========================================================================

set(COMMON_FLAGS "${CPU_FLAGS} -Wall -Wextra -Werror -g3")
set(CMAKE_C_FLAGS "${COMMON_FLAGS} -std=c11 -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}")

# Optimization levels
set(CMAKE_C_FLAGS_DEBUG "-O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "${CPU_FLAGS} -T${LINKER_SCRIPT} -Wl,--gc-sections -Wl,-Map=${PROJECT_NAME}.map --specs=nano.specs")

# ===========================================================================
# DEPENDENCY PATHS
# ===========================================================================
# Adjust these to point to your FreeRTOS and Modbus installations
# ===========================================================================

# FreeRTOS Kernel
set(FREERTOS_DIR "${CMAKE_SOURCE_DIR}/../../../third_party/FreeRTOS-Kernel"
    CACHE PATH "Path to FreeRTOS-Kernel directory")

if(NOT EXISTS ${FREERTOS_DIR})
    message(FATAL_ERROR
        "FreeRTOS not found at: ${FREERTOS_DIR}\n"
        "Please install FreeRTOS-Kernel:\n"
        "  git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel third_party/FreeRTOS-Kernel\n"
        "OR set FREERTOS_DIR:\n"
        "  cmake -DFREERTOS_DIR=/path/to/FreeRTOS-Kernel .."
    )
endif()

# Modbus Library
set(MODBUS_DIR "${CMAKE_SOURCE_DIR}/../.."
    CACHE PATH "Path to Modbus library root")

if(NOT EXISTS ${MODBUS_DIR}/modbus/include/modbus)
    message(FATAL_ERROR
        "Modbus library not found at: ${MODBUS_DIR}\n"
        "Please set MODBUS_DIR:\n"
        "  cmake -DMODBUS_DIR=/path/to/modbus/root .."
    )
endif()

# ===========================================================================
# FREERTOS LIBRARY
# ===========================================================================

add_library(freertos STATIC
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/timers.c
    ${FREERTOS_DIR}/stream_buffer.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c  # Heap implementation (can use heap_1/2/3/4/5)
)

# Select FreeRTOS port based on MCU
if(MCU_FAMILY STREQUAL "STM32F4" OR MCU_FAMILY STREQUAL "NRF52")
    target_sources(freertos PRIVATE
        ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
    )
    target_include_directories(freertos PUBLIC
        ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
    )
elseif(MCU_FAMILY STREQUAL "STM32G0")
    target_sources(freertos PRIVATE
        ${FREERTOS_DIR}/portable/GCC/ARM_CM0/port.c
    )
    target_include_directories(freertos PUBLIC
        ${FREERTOS_DIR}/portable/GCC/ARM_CM0
    )
endif()

target_include_directories(freertos PUBLIC
    ${FREERTOS_DIR}/include
    ${CMAKE_SOURCE_DIR}  # For FreeRTOSConfig.h
)

target_compile_definitions(freertos PUBLIC
    ${MCU_MODEL}
)

# ===========================================================================
# MODBUS LIBRARY
# ===========================================================================

add_library(modbus STATIC
    ${MODBUS_DIR}/modbus/src/mb_client.c
    ${MODBUS_DIR}/modbus/src/mb_transport_rtu.c
    ${MODBUS_DIR}/modbus/src/mb_crc.c
    ${MODBUS_DIR}/modbus/src/mb_err.c
    ${MODBUS_DIR}/modbus/src/mb_util.c
    # Add more source files as needed (mb_server.c, mb_transport_tcp.c, etc.)
)

target_include_directories(modbus PUBLIC
    ${MODBUS_DIR}/modbus/include
)

target_compile_definitions(modbus PUBLIC
    # Modbus configuration
    MB_CONF_PROFILE=MB_PROFILE_LEAN
    MB_CONF_ENABLE_FC03=1
    MB_CONF_ENABLE_FC04=1
    MB_CONF_ENABLE_FC06=1
    MB_CONF_ENABLE_FC16=1
    MB_CONF_RTU_ENABLED=1
    MB_CONF_ASCII_ENABLED=0
    MB_CONF_TCP_ENABLED=0
)

# ===========================================================================
# HAL/BSP LIBRARY (Platform-Specific Hardware Abstraction)
# ===========================================================================
# This is where you'd add your platform's HAL/SDK:
# - STM32: STM32CubeF4 HAL
# - nRF52: Nordic SDK
# - ESP32: ESP-IDF components (use different build system)
# ===========================================================================

# Example: STM32 HAL
if(MCU_FAMILY STREQUAL "STM32F4")
    set(STM32_HAL_DIR "${CMAKE_SOURCE_DIR}/../../../third_party/STM32CubeF4"
        CACHE PATH "Path to STM32CubeF4")
    
    if(EXISTS ${STM32_HAL_DIR})
        add_library(hal STATIC
            ${STM32_HAL_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
            ${STM32_HAL_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
            ${STM32_HAL_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
            ${STM32_HAL_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
            ${STM32_HAL_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
            ${STM32_HAL_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
            # Add more HAL modules as needed
        )
        
        target_include_directories(hal PUBLIC
            ${STM32_HAL_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
            ${STM32_HAL_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
            ${STM32_HAL_DIR}/Drivers/CMSIS/Include
        )
        
        target_compile_definitions(hal PUBLIC
            USE_HAL_DRIVER
            ${MCU_MODEL}
        )
    else()
        message(WARNING
            "STM32 HAL not found at: ${STM32_HAL_DIR}\n"
            "You'll need to implement HAL functions manually or download STM32CubeF4"
        )
        add_library(hal INTERFACE)  # Empty library
    endif()
endif()

# ===========================================================================
# APPLICATION EXECUTABLE
# ===========================================================================

add_executable(${PROJECT_NAME}.elf
    main.c
    # Add HAL implementation file here (e.g., hal_stm32f4.c)
    # system_stm32f4xx.c  # System initialization
    # startup_stm32f407xx.s  # Startup code
)

target_link_libraries(${PROJECT_NAME}.elf
    freertos
    modbus
    hal
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# ===========================================================================
# POST-BUILD STEPS (generate .bin, .hex, print size)
# ===========================================================================

# Generate binary file
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMENT "Generating ${PROJECT_NAME}.bin"
)

# Generate hex file
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMENT "Generating ${PROJECT_NAME}.hex"
)

# Print size information
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Firmware size:"
)

# ===========================================================================
# FLASH TARGET (adjust for your programmer)
# ===========================================================================

# Option 1: ST-Link (for STM32)
if(MCU_FAMILY STREQUAL "STM32F4" OR MCU_FAMILY STREQUAL "STM32G0")
    find_program(STLINK_FLASH st-flash)
    if(STLINK_FLASH)
        add_custom_target(flash
            COMMAND ${STLINK_FLASH} --reset write ${PROJECT_NAME}.bin 0x08000000
            DEPENDS ${PROJECT_NAME}.elf
            COMMENT "Flashing ${PROJECT_NAME}.bin via ST-Link"
        )
    endif()
endif()

# Option 2: OpenOCD (generic)
find_program(OPENOCD openocd)
if(OPENOCD)
    add_custom_target(flash-openocd
        COMMAND ${OPENOCD} -f interface/stlink.cfg -f target/stm32f4x.cfg
                -c "program ${PROJECT_NAME}.elf verify reset exit"
        DEPENDS ${PROJECT_NAME}.elf
        COMMENT "Flashing ${PROJECT_NAME}.elf via OpenOCD"
    )
endif()

# Option 3: J-Link (for nRF52, etc.)
find_program(JLINK JLinkExe)
if(JLINK)
    add_custom_target(flash-jlink
        COMMAND ${JLINK} -device ${MCU_MODEL} -if SWD -speed 4000 -autoconnect 1
                -CommanderScript ${CMAKE_SOURCE_DIR}/flash.jlink
        DEPENDS ${PROJECT_NAME}.hex
        COMMENT "Flashing ${PROJECT_NAME}.hex via J-Link"
    )
endif()

# ===========================================================================
# DEBUG TARGET (start GDB server)
# ===========================================================================

if(OPENOCD)
    add_custom_target(debug
        COMMAND ${OPENOCD} -f interface/stlink.cfg -f target/stm32f4x.cfg
        COMMENT "Starting OpenOCD GDB server on localhost:3333"
    )
endif()

# ===========================================================================
# HELPER TARGETS
# ===========================================================================

# Clean + rebuild
add_custom_target(rebuild
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    COMMENT "Clean rebuild"
)

# Display memory footprint
add_custom_target(size
    COMMAND ${CMAKE_SIZE} --format=sysv $<TARGET_FILE:${PROJECT_NAME}.elf>
    DEPENDS ${PROJECT_NAME}.elf
    COMMENT "Detailed memory usage:"
)

# ===========================================================================
# BUILD SUMMARY
# ===========================================================================

message(STATUS "=======================================================")
message(STATUS "FreeRTOS + Modbus RTU Client")
message(STATUS "=======================================================")
message(STATUS "MCU Family:      ${MCU_FAMILY}")
message(STATUS "MCU Model:       ${MCU_MODEL}")
message(STATUS "Linker Script:   ${LINKER_SCRIPT}")
message(STATUS "FreeRTOS:        ${FREERTOS_DIR}")
message(STATUS "Modbus:          ${MODBUS_DIR}")
message(STATUS "Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler:      ${CMAKE_C_COMPILER}")
message(STATUS "=======================================================")

# ===========================================================================
# NOTES FOR ADAPTING TO YOUR PROJECT
# ===========================================================================

# 1. TOOLCHAIN FILE:
#    Create cmake/arm-none-eabi.cmake:
#
#    set(CMAKE_SYSTEM_NAME Generic)
#    set(CMAKE_SYSTEM_PROCESSOR arm)
#    set(CMAKE_C_COMPILER arm-none-eabi-gcc)
#    set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
#    set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
#    set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
#    set(CMAKE_SIZE arm-none-eabi-size)
#    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
#    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
#    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
#    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
#
# 2. LINKER SCRIPT:
#    Copy from your MCU vendor (STM32CubeIDE, Nordic SDK, etc.)
#    Adjust LINKER_SCRIPT variable above
#
# 3. STARTUP CODE:
#    Add startup_xxx.s to add_executable() sources
#
# 4. SYSTEM INITIALIZATION:
#    Implement SystemInit() in system_xxx.c (clock config, etc.)
#
# 5. HAL FUNCTIONS:
#    Implement functions in modbus_tasks.h:
#    - uart_init_dma()
#    - uart_send_dma()
#    - uart_get_rx_count()
#    - uart_idle_callback() (ISR)
#
# 6. DEBUGGING:
#    - Connect ST-Link/J-Link
#    - Run: make debug (starts OpenOCD)
#    - In another terminal: arm-none-eabi-gdb build/freertos_modbus_rtu_client.elf
#    - (gdb) target remote localhost:3333
#    - (gdb) load
#    - (gdb) monitor reset halt
#    - (gdb) break main
#    - (gdb) continue
