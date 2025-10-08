# Set SOURCES variable
set(SOURCES
  src/ringbuf.c
  src/mempool.c
  src/pdu.c
  src/frame.c
  src/transport.c
  src/mb_log.c
  src/mb_err.c
  src/observe.c
  src/log.c
  src/utils.c
  src/fsm.c
  src/core.c
  src/features.c
  src/port/bare.c
  src/port/freertos.c
)

if(MODBUS_ENABLE_TRANSPORT_ASCII)
  list(APPEND SOURCES src/transport/ascii/ascii.c)
endif()

if(MODBUS_ENABLE_TRANSPORT_TCP)
  list(APPEND SOURCES
    src/transport/tcp/tcp.c
    src/transport/tcp/tcp_multi.c
  )
endif()

if(MODBUS_ENABLE_TRANSPORT_RTU)
  list(APPEND SOURCES src/transport/rtu/rtu.c)
endif()

if(MODBUS_ENABLE_SERVER)
  list(APPEND SOURCES
    src/server.c
    src/mapping.c
  )
  # Add convenience API for simplified server setup
  list(APPEND SOURCES src/mb_server_convenience.c)
endif()

if(MODBUS_ENABLE_CLIENT)
  list(APPEND SOURCES src/client.c)
  # Add high-level convenience functions for common operations
  list(APPEND SOURCES src/mb_client_convenience.c)
  # Add blocking host API for desktop/Linux/macOS/Windows
  list(APPEND SOURCES src/mb_host.c)
endif()

if(MODBUS_ENABLE_PORT_MUTEX)
  list(APPEND SOURCES src/port/mutex.c)
endif()

if(NOT WIN32 AND MODBUS_ENABLE_PORT_POSIX)
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
  include/modbus/mapping.h
  include/modbus/transport.h
  include/modbus/transport_if.h
  include/modbus/base.h
  include/modbus/mb_embed.h
  include/modbus/features.h
  include/modbus/utils.h
  include/modbus/fsm.h
  include/modbus/core.h
  include/modbus/port/freertos.h
  include/modbus/port/bare.h
  
)

if(MODBUS_ENABLE_TRANSPORT_ASCII)
  list(APPEND HEADERS_PUBLIC include/modbus/transport/ascii.h)
endif()

if(MODBUS_ENABLE_TRANSPORT_RTU)
  list(APPEND HEADERS_PUBLIC include/modbus/transport/rtu.h)
endif()

if(MODBUS_ENABLE_TRANSPORT_TCP)
  list(APPEND HEADERS_PUBLIC
    include/modbus/transport/tcp.h
    include/modbus/transport/tcp_multi.h
  )
endif()

if(MODBUS_ENABLE_SERVER)
  list(APPEND HEADERS_PUBLIC include/modbus/server.h)
  # Add convenience API header
  list(APPEND HEADERS_PUBLIC include/modbus/mb_server_convenience.h)
endif()

if(MODBUS_ENABLE_CLIENT)
  list(APPEND HEADERS_PUBLIC include/modbus/client.h)
  # Add blocking host API header for desktop/Linux/macOS/Windows
  list(APPEND HEADERS_PUBLIC include/modbus/mb_host.h)
endif()

if(MODBUS_ENABLE_PORT_MUTEX)
  list(APPEND HEADERS_PUBLIC include/modbus/port/mutex.h)
endif()

if(NOT WIN32 AND MODBUS_ENABLE_PORT_POSIX)
  list(APPEND HEADERS_PUBLIC include/modbus/port/posix.h)
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

if(MODBUS_ENABLE_TRANSPORT_RTU)
  set(_mb_conf_transport_rtu 1)
else()
  set(_mb_conf_transport_rtu 0)
endif()

if(MODBUS_ENABLE_TRANSPORT_ASCII)
  set(_mb_conf_transport_ascii 1)
else()
  set(_mb_conf_transport_ascii 0)
endif()

if(MODBUS_ENABLE_TRANSPORT_TCP)
  set(_mb_conf_transport_tcp 1)
else()
  set(_mb_conf_transport_tcp 0)
endif()

if(MODBUS_ENABLE_PORT_MUTEX)
  set(_mb_conf_port_mutex 1)
else()
  set(_mb_conf_port_mutex 0)
endif()

if(MODBUS_ENABLE_PORT_POSIX)
  set(_mb_conf_port_posix 1)
else()
  set(_mb_conf_port_posix 0)
endif()

target_compile_definitions(${LIBRARY_NAME} PUBLIC
  MB_CONF_BUILD_CLIENT=${_mb_conf_build_client}
  MB_CONF_BUILD_SERVER=${_mb_conf_build_server}
  MB_CONF_TRANSPORT_RTU=${_mb_conf_transport_rtu}
  MB_CONF_TRANSPORT_ASCII=${_mb_conf_transport_ascii}
  MB_CONF_TRANSPORT_TCP=${_mb_conf_transport_tcp}
  MB_CONF_PORT_MUTEX=${_mb_conf_port_mutex}
  MB_CONF_PORT_POSIX=${_mb_conf_port_posix}
  MB_CONF_PROFILE=${MB_CONF_PROFILE_ID}
  MB_CONF_ENABLE_FC01=${MB_CONF_ENABLE_FC01}
  MB_CONF_ENABLE_FC02=${MB_CONF_ENABLE_FC02}
  MB_CONF_ENABLE_FC03=${MB_CONF_ENABLE_FC03}
  MB_CONF_ENABLE_FC04=${MB_CONF_ENABLE_FC04}
  MB_CONF_ENABLE_FC05=${MB_CONF_ENABLE_FC05}
  MB_CONF_ENABLE_FC06=${MB_CONF_ENABLE_FC06}
  MB_CONF_ENABLE_FC07=${MB_CONF_ENABLE_FC07}
  MB_CONF_ENABLE_FC0F=${MB_CONF_ENABLE_FC0F}
  MB_CONF_ENABLE_FC10=${MB_CONF_ENABLE_FC10}
  MB_CONF_ENABLE_FC11=${MB_CONF_ENABLE_FC11}
  MB_CONF_ENABLE_FC16=${MB_CONF_ENABLE_FC16}
  MB_CONF_ENABLE_FC17=${MB_CONF_ENABLE_FC17}
  MB_CONF_DIAG_ENABLE_COUNTERS=${MB_CONF_DIAG_ENABLE_COUNTERS}
  MB_CONF_DIAG_ENABLE_TRACE=${MB_CONF_DIAG_ENABLE_TRACE}
  MB_CONF_DIAG_TRACE_DEPTH=${MB_CONF_DIAG_TRACE_DEPTH}
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
