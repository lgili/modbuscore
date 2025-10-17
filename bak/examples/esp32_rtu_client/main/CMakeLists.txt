# Component CMakeLists.txt for main component
#
# This registers the main application and integrates Modbus library.

# Set component sources
set(COMPONENT_SRCS "main.c")

# Set component include directories
set(COMPONENT_ADD_INCLUDEDIRS ".")

# Path to Modbus library (adjust relative path)
set(MODBUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

# Add Modbus library sources
list(APPEND COMPONENT_SRCS
    "${MODBUS_DIR}/modbus/src/mb_client.c"
    "${MODBUS_DIR}/modbus/src/mb_transport_rtu.c"
    "${MODBUS_DIR}/modbus/src/mb_crc.c"
    "${MODBUS_DIR}/modbus/src/mb_err.c"
    "${MODBUS_DIR}/modbus/src/mb_util.c"
)

# Add Modbus include directory
list(APPEND COMPONENT_ADD_INCLUDEDIRS
    "${MODBUS_DIR}/modbus/include"
)

# Register component
register_component()

# Add compile definitions for Modbus configuration
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    # Modbus profile
    MB_CONF_PROFILE=MB_PROFILE_LEAN
    
    # Function code enablement (from Kconfig)
    MB_CONF_ENABLE_FC03=$<IF:$<BOOL:${CONFIG_MODBUS_FC03_ENABLED}>,1,0>
    MB_CONF_ENABLE_FC04=$<IF:$<BOOL:${CONFIG_MODBUS_FC04_ENABLED}>,1,0>
    MB_CONF_ENABLE_FC06=$<IF:$<BOOL:${CONFIG_MODBUS_FC06_ENABLED}>,1,0>
    MB_CONF_ENABLE_FC16=$<IF:$<BOOL:${CONFIG_MODBUS_FC16_ENABLED}>,1,0>
    MB_CONF_ENABLE_FC01=$<IF:$<BOOL:${CONFIG_MODBUS_FC01_ENABLED}>,1,0>
    MB_CONF_ENABLE_FC02=$<IF:$<BOOL:${CONFIG_MODBUS_FC02_ENABLED}>,1,0>
    MB_CONF_ENABLE_FC05=$<IF:$<BOOL:${CONFIG_MODBUS_FC05_ENABLED}>,1,0>
    MB_CONF_ENABLE_FC15=$<IF:$<BOOL:${CONFIG_MODBUS_FC15_ENABLED}>,1,0>
    
    # Transport enablement
    MB_CONF_RTU_ENABLED=1
    MB_CONF_ASCII_ENABLED=0
    MB_CONF_TCP_ENABLED=0
)

# Compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)
