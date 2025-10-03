if(WIN32)
  message(WARNING "Windows example binaries are maintained under examples/win and are not yet integrated with the top-level build.")
  return()
endif()

add_executable(modbus_tcp_client_cli tcp_client_cli.c)
target_link_libraries(modbus_tcp_client_cli PRIVATE Modbus::modbus)
target_compile_options(modbus_tcp_client_cli PRIVATE ${MODBUS_WARN_FLAGS})

install(TARGETS modbus_tcp_client_cli RUNTIME DESTINATION bin)
