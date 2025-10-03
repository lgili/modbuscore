add_executable(modbus_server_win main.c)
target_link_libraries(modbus_server_win PRIVATE Modbus::modbus)

if(NOT WIN32)
  add_executable(modbus_tcp_client_cli tcp_client_cli.c)
  target_link_libraries(modbus_tcp_client_cli PRIVATE Modbus::modbus)
  target_compile_options(modbus_tcp_client_cli PRIVATE ${MODBUS_WARN_FLAGS})
  install(TARGETS modbus_tcp_client_cli RUNTIME DESTINATION bin)
endif()

install(
  TARGETS modbus_server_win
  RUNTIME DESTINATION bin
)