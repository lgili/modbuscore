# Set SOURCES variable
set(SOURCES
  fsm.c
  modbus.c 
  modbus_client.c
  modbus_server.c  
  helpers.c
)

# Set HEADERS_PUBLIC variable (public headers, included in the library)
set(HEADERS_PUBLIC  
  modbus.h 
  modbus_client.h
  modbus_server.h  
)

# Set HEADERS_PRIVATE variable, if needed.
# (these private headers won't be included in the library)
set(HEADERS_PRIVATE
  helpers.h 
  fsm.h
)

include(${PROJECT_SOURCE_DIR}/cmake/LibraryConfig.cmake)