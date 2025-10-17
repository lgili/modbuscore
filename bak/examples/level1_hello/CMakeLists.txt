# Level 1 Hello World Examples
# Simple 10-line examples showing mb_host_* API usage

if(MODBUS_ENABLE_CLIENT AND MODBUS_ENABLE_PORT_POSIX AND NOT WIN32)
    
    # TCP Hello World
    if(MODBUS_ENABLE_TRANSPORT_TCP)
        add_executable(hello_tcp hello_tcp.c)
        target_link_libraries(hello_tcp PRIVATE modbus)
    endif()
    
    # RTU Hello World
    if(MODBUS_ENABLE_TRANSPORT_RTU)
        add_executable(hello_rtu hello_rtu.c)
        target_link_libraries(hello_rtu PRIVATE modbus)
    endif()
    
    # Write Example
    if(MODBUS_ENABLE_TRANSPORT_TCP)
        add_executable(hello_write hello_write.c)
        target_link_libraries(hello_write PRIVATE modbus)
    endif()
    
endif()

# Server convenience example
if(MODBUS_ENABLE_SERVER AND MODBUS_ENABLE_TRANSPORT_TCP AND MODBUS_ENABLE_PORT_POSIX AND NOT WIN32)
    add_executable(hello_server hello_server.c)
    target_link_libraries(hello_server PRIVATE modbus)
endif()
