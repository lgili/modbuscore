if(WIN32)
  message(WARNING "Windows example binaries are maintained under examples/win and are not yet integrated with the top-level build.")
  return()
endif()

add_executable(modbus_tcp_client_cli tcp_client_cli.c)
target_link_libraries(modbus_tcp_client_cli PRIVATE Modbus::modbus)
target_compile_options(modbus_tcp_client_cli PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_freertos_sim freertos_sim.c)
target_link_libraries(modbus_freertos_sim PRIVATE Modbus::modbus)
target_compile_options(modbus_freertos_sim PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_posix_sim posix_sim.c)
target_link_libraries(modbus_posix_sim PRIVATE Modbus::modbus)
target_compile_options(modbus_posix_sim PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_bare_sim bare_sim.c)
target_link_libraries(modbus_bare_sim PRIVATE Modbus::modbus)
target_compile_options(modbus_bare_sim PRIVATE ${MODBUS_WARN_FLAGS})

install(TARGETS modbus_tcp_client_cli modbus_freertos_sim modbus_posix_sim modbus_bare_sim
  RUNTIME DESTINATION bin)
