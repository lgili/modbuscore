# Set SOURCES variable
set(SOURCES
  src/utils.c
  src/fsm.c
  # src/modbus_client.c
  # src/modbus_core.c
  # src/modbus_server.c
)

# Set HEADERS_PUBLIC variable (public headers, included in the library)
set(HEADERS_PUBLIC  
  include/modbus/modbus.h
  include/modbus/conf.h
  include/modbus/transport.h
  include/modbus/base.h
  include/modbus/utils.h
  include/modbus/fsm.h
  include/modbus/core.h
  # include/modbus/client.h
  # include/modbus/server.h
  
)

# Set HEADERS_PRIVATE variable (private headers won't be exposed)
set(HEADERS_PRIVATE  
  
)

# Include LibraryConfig.cmake
include(${PROJECT_SOURCE_DIR}/cmake/LibraryConfig.cmake)

