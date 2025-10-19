add_executable(modbus_tcp_server
  tcp_server.c
)
target_link_libraries(modbus_tcp_server PRIVATE ModbusCore::modbuscore)
target_include_directories(modbus_tcp_server PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

add_executable(modbus_tcp_client
  tcp_client.c
)
target_link_libraries(modbus_tcp_client PRIVATE ModbusCore::modbuscore)
target_include_directories(modbus_tcp_client PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

add_executable(modbus_rtu_server
  rtu_server.c
)
target_link_libraries(modbus_rtu_server PRIVATE ModbusCore::modbuscore)
target_include_directories(modbus_rtu_server PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

add_executable(modbus_rtu_client
  rtu_client.c
)
target_link_libraries(modbus_rtu_client PRIVATE ModbusCore::modbuscore)
target_include_directories(modbus_rtu_client PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)
