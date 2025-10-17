# ESP-IDF Project CMakeLists.txt
# 
# This file integrates the Modbus library with an ESP-IDF project.
#
# USAGE:
#   idf.py menuconfig  # Configure options
#   idf.py build       # Build firmware
#   idf.py flash       # Flash to ESP32
#   idf.py monitor     # View serial output

cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define project
project(esp32_modbus_rtu_client)
