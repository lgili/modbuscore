# Set SOURCES variable
set(SOURCES
  src/ringbuf.c
  src/mempool.c
  src/pdu.c
  src/frame.c
  src/transport.c
  src/transport/tcp/tcp.c
  src/transport/tcp/tcp_multi.c
  src/mb_log.c
  src/mb_err.c
  src/observe.c
  src/transport/rtu/rtu.c
  src/log.c
  src/utils.c
  src/fsm.c
  src/core.c
  src/port/bare.c
  src/port/freertos.c
  src/port/mutex.c
)

if(MODBUS_ENABLE_SERVER)
  list(APPEND SOURCES src/server.c)
endif()

if(MODBUS_ENABLE_CLIENT)
  list(APPEND SOURCES src/client.c)
endif()

if(NOT WIN32)
  list(APPEND SOURCES src/port/posix.c)
endif()

if(WIN32)
  list(APPEND SOURCES src/port/win.c)
endif()

# Set HEADERS_PUBLIC variable (public headers, included in the library)
set(HEADERS_PUBLIC  
  include/modbus/log.h
  include/modbus/mempool.h
  include/modbus/pdu.h
  include/modbus/frame.h
  include/modbus/mb_types.h
  include/modbus/ringbuf.h
  include/modbus/mb_err.h
  include/modbus/mb_log.h
  include/modbus/observe.h
  include/modbus/modbus.h
  include/modbus/conf.h
  include/modbus/transport.h
  include/modbus/transport/rtu.h
  include/modbus/transport/tcp.h
  include/modbus/transport/tcp_multi.h
  include/modbus/transport_if.h
  include/modbus/base.h
  include/modbus/utils.h
  include/modbus/fsm.h
  include/modbus/core.h
  include/modbus/port/mutex.h
  include/modbus/port/posix.h
  include/modbus/port/freertos.h
  include/modbus/port/bare.h
  
)

if(MODBUS_ENABLE_SERVER)
  list(APPEND HEADERS_PUBLIC include/modbus/server.h)
endif()

if(MODBUS_ENABLE_CLIENT)
  list(APPEND HEADERS_PUBLIC include/modbus/client.h)
endif()

if(WIN32)
  list(APPEND HEADERS_PUBLIC include/modbus/port/win.h)
endif()

# Set HEADERS_PRIVATE variable (private headers won't be exposed)
set(HEADERS_PRIVATE  
  
)

# Include LibraryConfig.cmake
include(${PROJECT_SOURCE_DIR}/cmake/LibraryConfig.cmake)

if(MODBUS_ENABLE_CLIENT)
  set(_mb_conf_build_client 1)
else()
  set(_mb_conf_build_client 0)
endif()

if(MODBUS_ENABLE_SERVER)
  set(_mb_conf_build_server 1)
else()
  set(_mb_conf_build_server 0)
endif()

target_compile_definitions(${LIBRARY_NAME} PUBLIC
  MB_CONF_BUILD_CLIENT=${_mb_conf_build_client}
  MB_CONF_BUILD_SERVER=${_mb_conf_build_server}
)

# Optional SEGGER RTT transport for logging
find_package(SEGGER_RTT QUIET)

if(SEGGER_RTT_FOUND)
  target_compile_definitions(${LIBRARY_NAME} PUBLIC MB_LOG_ENABLE_SEGGER_RTT=1)
  target_link_libraries(${LIBRARY_NAME} PUBLIC SEGGER_RTT::SEGGER_RTT)
  message(STATUS "Enabling SEGGER RTT logging sink")
else()
  message(STATUS "SEGGER RTT not found: MB_LOG_ENABLE_SEGGER_RTT defaults to 0")
endif()

if(WIN32)
  target_link_libraries(${LIBRARY_NAME} PUBLIC ws2_32)
endif()
