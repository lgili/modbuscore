add_library(modbus_examples_common STATIC
  common/demo_tcp_socket.c
  common/demo_serial_port.c
)
target_include_directories(modbus_examples_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(modbus_examples_common PUBLIC Modbus::modbus)
target_compile_options(modbus_examples_common PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_tcp_client_cli tcp_client_cli.c)
target_link_libraries(modbus_tcp_client_cli PRIVATE modbus_examples_common)
target_compile_options(modbus_tcp_client_cli PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_tcp_server_demo tcp_server_demo.c)
target_link_libraries(modbus_tcp_server_demo PRIVATE modbus_examples_common)
target_compile_options(modbus_tcp_server_demo PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_tcp_multi_client_demo tcp_multi_client_demo.c)
target_link_libraries(modbus_tcp_multi_client_demo PRIVATE modbus_examples_common)
target_compile_options(modbus_tcp_multi_client_demo PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_rtu_loop_demo rtu_loop_demo.c)
target_link_libraries(modbus_rtu_loop_demo PRIVATE Modbus::modbus)
target_compile_options(modbus_rtu_loop_demo PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_rtu_serial_server rtu_serial_server.c)
target_link_libraries(modbus_rtu_serial_server PRIVATE modbus_examples_common)
target_compile_options(modbus_rtu_serial_server PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_rtu_serial_client rtu_serial_client.c)
target_link_libraries(modbus_rtu_serial_client PRIVATE modbus_examples_common)
target_compile_options(modbus_rtu_serial_client PRIVATE ${MODBUS_WARN_FLAGS})

add_executable(modbus_freertos_sim freertos_sim.c)
target_link_libraries(modbus_freertos_sim PRIVATE Modbus::modbus)
target_compile_options(modbus_freertos_sim PRIVATE ${MODBUS_WARN_FLAGS})

if(NOT WIN32)
  add_executable(modbus_posix_sim posix_sim.c)
  target_link_libraries(modbus_posix_sim PRIVATE Modbus::modbus)
  target_compile_options(modbus_posix_sim PRIVATE ${MODBUS_WARN_FLAGS})
endif()

add_executable(modbus_bare_sim bare_sim.c)
target_link_libraries(modbus_bare_sim PRIVATE Modbus::modbus)
target_compile_options(modbus_bare_sim PRIVATE ${MODBUS_WARN_FLAGS})

set(INSTALLABLE_TARGETS
  modbus_tcp_client_cli
  modbus_tcp_server_demo
  modbus_tcp_multi_client_demo
  modbus_rtu_loop_demo
  modbus_rtu_serial_server
  modbus_rtu_serial_client
  modbus_freertos_sim
  modbus_bare_sim
)

if(NOT WIN32)
  list(APPEND INSTALLABLE_TARGETS modbus_posix_sim)
endif()

install(TARGETS ${INSTALLABLE_TARGETS}
  RUNTIME DESTINATION bin)
