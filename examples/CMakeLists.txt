add_executable(modbus_server_win main.c)

target_link_libraries(modbus_server_win PRIVATE Modbus::modbus)

install(
  TARGETS modbus_server_win
  RUNTIME DESTINATION bin
)