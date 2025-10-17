add_executable(runtime_smoke
  test_runtime.c
)

target_link_libraries(runtime_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(runtime_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_test(NAME runtime_smoke COMMAND runtime_smoke)

add_executable(transport_iface_smoke
  test_transport_iface.c
)

target_link_libraries(transport_iface_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(transport_iface_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_test(NAME transport_iface_smoke COMMAND transport_iface_smoke)

add_executable(protocol_engine_smoke
  test_protocol_engine.c
)

target_link_libraries(protocol_engine_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(protocol_engine_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_test(NAME protocol_engine_smoke COMMAND protocol_engine_smoke)

add_executable(pdu_smoke
  test_pdu.c
)

target_link_libraries(pdu_smoke PRIVATE ModbusCore::modbuscore)
target_include_directories(pdu_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_test(NAME pdu_smoke COMMAND pdu_smoke)
