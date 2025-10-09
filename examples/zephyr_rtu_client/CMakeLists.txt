# Copyright (c) 2025
# SPDX-License-Identifier: Apache-2.0

# =============================================================================
# CMakeLists.txt for Zephyr + Modbus RTU Client Example
# =============================================================================
# This file integrates the Modbus library with a Zephyr RTOS application.
#
# USAGE:
#   west build -b nrf52840dk_nrf52840
#   west flash
#   west build -b nucleo_f411re  (for STM32)
# =============================================================================

cmake_minimum_required(VERSION 3.20.0)

# Find Zephyr package (sets ZEPHYR_BASE, BOARD, etc.)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Define project
project(zephyr_modbus_rtu_client VERSION 1.0.0)

# =============================================================================
# APPLICATION SOURCES
# =============================================================================

# Main application file
target_sources(app PRIVATE src/main.c)

# =============================================================================
# MODBUS LIBRARY INTEGRATION
# =============================================================================

# Path to Modbus library (adjust relative path as needed)
set(MODBUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Modbus library sources
target_sources(app PRIVATE
    ${MODBUS_DIR}/modbus/src/mb_client.c
    ${MODBUS_DIR}/modbus/src/mb_transport_rtu.c
    ${MODBUS_DIR}/modbus/src/mb_crc.c
    ${MODBUS_DIR}/modbus/src/mb_err.c
    ${MODBUS_DIR}/modbus/src/mb_util.c
    # Add more sources as needed:
    # ${MODBUS_DIR}/modbus/src/mb_server.c
    # ${MODBUS_DIR}/modbus/src/mb_transport_tcp.c
)

# Modbus library include directories
target_include_directories(app PRIVATE
    ${MODBUS_DIR}/modbus/include
)

# Modbus compile definitions
target_compile_definitions(app PRIVATE
    # Profile selection (FULL, LEAN, TINY)
    MB_CONF_PROFILE=MB_PROFILE_LEAN
    
    # Function code enablement
    MB_CONF_ENABLE_FC03=1  # Read Holding Registers
    MB_CONF_ENABLE_FC04=1  # Read Input Registers
    MB_CONF_ENABLE_FC06=1  # Write Single Register
    MB_CONF_ENABLE_FC16=1  # Write Multiple Registers
    
    # Transport enablement
    MB_CONF_RTU_ENABLED=1
    MB_CONF_ASCII_ENABLED=0
    MB_CONF_TCP_ENABLED=0
)

# =============================================================================
# ZEPHYR-SPECIFIC CONFIGURATION
# =============================================================================

# Enable C11 standard (Modbus library requires stdint.h, stdbool.h)
target_compile_options(app PRIVATE
    -std=c11
    -Wall
    -Wextra
)

# =============================================================================
# DEVICETREE OVERLAY (optional, for custom UART pinout)
# =============================================================================
# If your board requires custom UART configuration, create:
#   boards/nrf52840dk_nrf52840.overlay
# or
#   boards/nucleo_f411re.overlay
#
# Example overlay content (nRF52840 DK):
#
# / {
#     aliases {
#         modbus-uart = &uart0;
#     };
# };
# 
# &uart0 {
#     status = "okay";
#     current-speed = <9600>;
#     tx-pin = <6>;  /* P0.06 */
#     rx-pin = <8>;  /* P0.08 */
# };
#
# For STM32 Nucleo:
# &usart1 {
#     status = "okay";
#     pinctrl-0 = <&usart1_tx_pa9 &usart1_rx_pa10>;
#     pinctrl-names = "default";
# };
# =============================================================================

# =============================================================================
# NOTES FOR PORTING
# =============================================================================
#
# 1. BOARD SELECTION:
#    Supported boards: Any Zephyr-supported board with UART
#    Examples:
#    - nRF52840 DK:        west build -b nrf52840dk_nrf52840
#    - STM32 Nucleo F4:    west build -b nucleo_f411re
#    - ESP32 DevKit:       west build -b esp32_devkitc_wroom
#    - QEMU (simulation):  west build -b qemu_cortex_m3
#
# 2. UART CONFIGURATION:
#    - Default: Uses first available UART (zephyr,console or uart0)
#    - For production: Create devicetree overlay to define modbus-uart alias
#    - See boards/ directory for examples
#
# 3. MEMORY REQUIREMENTS:
#    - RAM:  ~8 KB (heap + stacks + Modbus buffers)
#    - ROM:  ~20 KB (Zephyr kernel + Modbus lib + example)
#    - Test on target before production deployment
#
# 4. DEBUGGING:
#    - Enable CONFIG_DEBUG=y in prj.conf
#    - Use "west debug" to start GDB session
#    - Set breakpoints: (gdb) break main
#
# 5. PRODUCTION BUILD:
#    west build -b <board> -- -DCMAKE_BUILD_TYPE=Release
#    This enables -Os optimization and disables debug symbols
