set(FUZZ_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

if(MODBUS_SANITIZERS)
  message(WARNING "Skipping fuzzers: combine sanitizers (${MODBUS_SANITIZERS}) with libFuzzer is not supported.")
  return()
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsanitize=fuzzer" MODBUS_HAS_LIBFUZZER)
if(NOT MODBUS_HAS_LIBFUZZER)
  message(WARNING "Skipping fuzzers: compiler ${CMAKE_CXX_COMPILER_ID} lacks -fsanitize=fuzzer support.")
  return()
endif()

add_executable(modbus_pdu_fuzz
  ${FUZZ_ROOT}/pdu_decode_fuzz.cpp
)

target_compile_features(modbus_pdu_fuzz PRIVATE cxx_std_17)

target_link_libraries(modbus_pdu_fuzz PRIVATE modbus)

if(MODBUS_WARN_FLAGS)
  target_compile_options(modbus_pdu_fuzz PRIVATE ${MODBUS_WARN_FLAGS})
endif()

target_compile_options(modbus_pdu_fuzz PRIVATE -fsanitize=fuzzer-no-link)
target_link_options(modbus_pdu_fuzz PRIVATE -fsanitize=fuzzer)

set_target_properties(modbus_pdu_fuzz PROPERTIES
  OUTPUT_NAME "modbus_pdu_fuzz"
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
)
