add_executable(modbus_client_win main.c)

target_link_libraries(modbus_client_win PRIVATE Modbus::modbus)

install(
  TARGETS modbus_client_win
  RUNTIME DESTINATION bin
)