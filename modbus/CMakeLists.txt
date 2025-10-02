# Set SOURCES variable
set(SOURCES
  src/ringbuf.c
  src/mempool.c
  src/mb_log.c
  src/log.c
  src/utils.c
  src/fsm.c
  src/core.c
  src/server.c
  src/client.c
)

# Set HEADERS_PUBLIC variable (public headers, included in the library)
set(HEADERS_PUBLIC  
  include/modbus/log.h
  include/modbus/mempool.h
  include/modbus/mb_types.h
  include/modbus/ringbuf.h
  include/modbus/mb_err.h
  include/modbus/mb_log.h
  include/modbus/modbus.h
  include/modbus/conf.h
  include/modbus/transport.h
  include/modbus/base.h
  include/modbus/utils.h
  include/modbus/fsm.h
  include/modbus/core.h
  include/modbus/server.h
  include/modbus/client.h
  
)

# Set HEADERS_PRIVATE variable (private headers won't be exposed)
set(HEADERS_PRIVATE  
  
)

# Include LibraryConfig.cmake
include(${PROJECT_SOURCE_DIR}/cmake/LibraryConfig.cmake)

# Optional SEGGER RTT transport for logging
find_package(SEGGER_RTT QUIET)

if(SEGGER_RTT_FOUND)
  target_compile_definitions(${LIBRARY_NAME} PUBLIC MB_LOG_ENABLE_SEGGER_RTT=1)
  target_link_libraries(${LIBRARY_NAME} PUBLIC SEGGER_RTT::SEGGER_RTT)
  message(STATUS "Enabling SEGGER RTT logging sink")
else()
  message(STATUS "SEGGER RTT not found: MB_LOG_ENABLE_SEGGER_RTT defaults to 0")
endif()

