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
endif()

# Zero-copy IO (always enabled)
list(APPEND SOURCES src/mb_iovec.c)

# Lock-free queues and transaction pool (Gate 22)
list(APPEND SOURCES src/mb_queue.c)
list(APPEND SOURCES src/mb_txpool.c)

# Noise resilience features (Gate 33)
list(APPEND SOURCES src/rtu_resync.c)
list(APPEND SOURCES src/dup_filter.c)

# ISR-safe mode (Gate 23)
list(APPEND SOURCES src/mb_isr.c)

# QoS and backpressure (Gate 24)
list(APPEND SOURCES src/mb_qos.c)

# Power management (Gate 27) - enabled by MB_CONF_ENABLE_POWER_MANAGEMENT
list(APPEND SOURCES src/mb_power.c)

if(MODBUS_ENABLE_CLIENT)
  list(APPEND SOURCES src/client.c)
  # Add convenience functions for common operations
  list(APPEND SOURCES src/mb_client_convenience.c)
  # Add blocking host API for desktop/Linux/macOS/Windows
  list(APPEND SOURCES src/mb_host.c)
  # Add simplified public API
  list(APPEND SOURCES src/mb_simple.c)
endif()

if(MODBUS_ENABLE_COMPAT_LIBMODBUS)
  list(APPEND SOURCES src/compat/libmodbus.c)
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
# These are the new simplified public API headers
set(HEADERS_PUBLIC  
  include/modbus/mb_types.h
  include/modbus/mb_err.h
  include/modbus/modbus.h
  include/modbus/conf.h
  include/modbus/mb_embed.h
  include/modbus/mb_simple.h
  include/modbus/version.h
  include/modbus/mb_isr.h
  include/modbus/mb_qos.h
  include/modbus/mb_power.h
  include/modbus/transport.h
  include/modbus/transport_if.h
  include/modbus/profiles.h
  include/modbus/port/bare.h
  include/modbus/port/freertos.h
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

if(MODBUS_ENABLE_COMPAT_LIBMODBUS)
  list(APPEND HEADERS_PUBLIC
    include/modbus/compat/libmodbus.h
    include/modbus/compat/modbus-errno.h
  )
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

# Set HEADERS_PRIVATE variable (internal implementation headers)
set(HEADERS_PRIVATE
  include/modbus/internal/log.h
  include/modbus/internal/pdu.h
  include/modbus/internal/frame.h
  include/modbus/internal/observe.h
  include/modbus/internal/mapping.h
  include/modbus/internal/transport_core.h
  include/modbus/internal/utils.h
  include/modbus/internal/base.h
  include/modbus/internal/core.h
  include/modbus/internal/mempool.h
  include/modbus/internal/ringbuf.h
  include/modbus/internal/mb_log.h
  include/modbus/internal/mb_iovec.h
  include/modbus/internal/mb_queue.h
  include/modbus/internal/mb_txpool.h
  include/modbus/internal/features.h
  include/modbus/internal/fsm.h
  include/modbus/internal/dup_filter.h
  include/modbus/internal/rtu_resync.h
  include/modbus/internal/mb_simple_backend.h
)

if(MODBUS_ENABLE_SERVER)
  list(APPEND HEADERS_PRIVATE include/modbus/internal/server.h)
endif()

if(MODBUS_ENABLE_CLIENT)
  list(APPEND HEADERS_PRIVATE include/modbus/internal/client.h)
  list(APPEND HEADERS_PRIVATE include/modbus/mb_host.h)
endif()


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
  MB_CONF_ENABLE_COMPAT_LIBMODBUS=${MB_CONF_ENABLE_COMPAT_LIBMODBUS}
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
