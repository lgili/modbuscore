find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(runtime_smoke
  test_runtime.c
)

target_link_libraries(runtime_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(runtime_smoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME runtime_smoke COMMAND runtime_smoke)

add_executable(transport_iface_smoke
  test_transport_iface.c
)

target_link_libraries(transport_iface_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(transport_iface_smoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME transport_iface_smoke COMMAND transport_iface_smoke)

add_executable(mock_transport_smoke
  test_mock_transport.c
)

target_link_libraries(mock_transport_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(mock_transport_smoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mock_transport_smoke COMMAND mock_transport_smoke)

add_executable(rtu_uart_smoke
  test_rtu_uart.c
)

target_link_libraries(rtu_uart_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(rtu_uart_smoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME rtu_uart_smoke COMMAND rtu_uart_smoke)

# Platform-specific transport tests
if(WIN32)
  # Windows-specific tests
  add_executable(winsock_tcp_smoke test_winsock_tcp.c)
  target_link_libraries(winsock_tcp_smoke PRIVATE ModbusCore::modbuscore)
  target_include_directories(winsock_tcp_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  add_test(NAME winsock_tcp_smoke COMMAND winsock_tcp_smoke)

  add_executable(win32_rtu_smoke test_win32_rtu.c)
  target_link_libraries(win32_rtu_smoke PRIVATE ModbusCore::modbuscore)
  target_include_directories(win32_rtu_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  add_test(NAME win32_rtu_smoke COMMAND win32_rtu_smoke)
else()
  # POSIX-specific tests (Linux, macOS, BSD, etc.)
  add_executable(posix_rtu_smoke test_posix_rtu.c)
  target_link_libraries(posix_rtu_smoke PRIVATE ModbusCore::modbuscore)
  target_include_directories(posix_rtu_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  add_test(NAME posix_rtu_smoke COMMAND posix_rtu_smoke)

  add_executable(posix_tcp_smoke test_posix_tcp.c)
  target_link_libraries(posix_tcp_smoke PRIVATE ModbusCore::modbuscore)
  target_include_directories(posix_tcp_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  add_test(NAME posix_tcp_smoke COMMAND posix_tcp_smoke)
endif()

add_executable(engine_resilience_smoke
  test_engine_resilience.c
)

target_link_libraries(engine_resilience_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(engine_resilience_smoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME engine_resilience_smoke COMMAND engine_resilience_smoke)

add_executable(engine_diagnostics_smoke
  test_engine_diagnostics.c
)

target_link_libraries(engine_diagnostics_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(engine_diagnostics_smoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME engine_diagnostics_smoke COMMAND engine_diagnostics_smoke)

add_executable(autoheal_smoke
  test_autoheal.c
)

target_link_libraries(autoheal_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(autoheal_smoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME autoheal_smoke COMMAND autoheal_smoke)

add_executable(protocol_engine_smoke
  test_protocol_engine.c
)

target_link_libraries(protocol_engine_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(protocol_engine_smoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME protocol_engine_smoke COMMAND protocol_engine_smoke)

add_executable(pdu_smoke
  test_pdu.c
)

target_link_libraries(pdu_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(pdu_smoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME pdu_smoke COMMAND pdu_smoke)

add_test(NAME modbus_cli_sanity
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Testing/modbus_cli_sanity.py
)

# Example: End-to-end TCP client with FC03 (POSIX only - uses unistd.h and usleep)
if(NOT WIN32)
  add_executable(example_tcp_client_fc03
    example_tcp_client_fc03.c
  )

  target_link_libraries(example_tcp_client_fc03 PRIVATE ModbusCore::modbuscore)
  target_include_directories(example_tcp_client_fc03 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
endif()

add_test(NAME modbus_cli_scaffold
  COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/modbus_cli/tests/test_scaffold.py
    --repo-root ${PROJECT_SOURCE_DIR}
    --build-root ${PROJECT_BINARY_DIR}
)
