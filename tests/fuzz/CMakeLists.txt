set(FUZZ_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

if(MODBUS_SANITIZERS)
  message(WARNING "Skipping fuzzers: combine sanitizers (${MODBUS_SANITIZERS}) with libFuzzer is not supported.")
  return()
endif()

set(_modbus_fuzzer_extra_libs "")
set(_modbus_fuzzer_link_options "")

# Some toolchains (notably Homebrew LLVM on macOS) ship a newer libc++ in a
# non-standard location that is required to satisfy libFuzzer's dependencies.
# Discover the relevant libraries from the compiler path and make them
# available for both the capability probe and the actual target.
if(APPLE)
  get_filename_component(_modbus_cxx_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(_modbus_toolchain_root "${_modbus_cxx_dir}/.." ABSOLUTE)
  set(_modbus_libcxx_hint "${_modbus_toolchain_root}/lib/c++")
  set(_modbus_libcxx_search_dirs "")
  set(_modbus_libcxx_search_paths
      "${_modbus_libcxx_hint}"
      "${_modbus_toolchain_root}/lib"
  )

  foreach(_modbus_candidate IN LISTS _modbus_libcxx_search_paths)
    if(NOT EXISTS "${_modbus_candidate}")
      continue()
    endif()
    list(APPEND _modbus_libcxx_search_dirs "${_modbus_candidate}")
  endforeach()

  find_library(MODBUS_LIBFUZZER_LIBCXX
    NAMES c++ libc++ libc++.1
    PATHS ${_modbus_libcxx_search_dirs}
    NO_DEFAULT_PATH
  )
  if(NOT MODBUS_LIBFUZZER_LIBCXX)
    find_library(MODBUS_LIBFUZZER_LIBCXX NAMES c++ libc++ libc++.1)
  endif()

  find_library(MODBUS_LIBFUZZER_LIBCXXABI
    NAMES c++abi libc++abi libc++abi.1
    PATHS ${_modbus_libcxx_search_dirs}
    NO_DEFAULT_PATH
  )
  if(NOT MODBUS_LIBFUZZER_LIBCXXABI)
    find_library(MODBUS_LIBFUZZER_LIBCXXABI NAMES c++abi libc++abi libc++abi.1)
  endif()

  if(MODBUS_LIBFUZZER_LIBCXX)
    list(APPEND _modbus_fuzzer_extra_libs "${MODBUS_LIBFUZZER_LIBCXX}")
    get_filename_component(_modbus_libcxx_dir "${MODBUS_LIBFUZZER_LIBCXX}" DIRECTORY)
    list(APPEND _modbus_fuzzer_link_options "-Wl,-rpath,${_modbus_libcxx_dir}")
    message(STATUS "libFuzzer: using libc++ from ${MODBUS_LIBFUZZER_LIBCXX}")
  endif()
  if(MODBUS_LIBFUZZER_LIBCXXABI)
    list(APPEND _modbus_fuzzer_extra_libs "${MODBUS_LIBFUZZER_LIBCXXABI}")
    message(STATUS "libFuzzer: using libc++abi from ${MODBUS_LIBFUZZER_LIBCXXABI}")
  endif()
endif()
# Compose compile and link flag strings for the capability probe.
set(_modbus_fuzzer_compile_flags "${CMAKE_CXX_FLAGS}")
if(_modbus_fuzzer_compile_flags)
  string(APPEND _modbus_fuzzer_compile_flags " ")
endif()
string(APPEND _modbus_fuzzer_compile_flags "-fsanitize=fuzzer")

set(_modbus_fuzzer_link_flags "${CMAKE_EXE_LINKER_FLAGS}")
if(_modbus_fuzzer_link_flags)
  string(APPEND _modbus_fuzzer_link_flags " ")
endif()
string(APPEND _modbus_fuzzer_link_flags "-fsanitize=fuzzer")
if(_modbus_fuzzer_link_options)
  string(REPLACE ";" " " _modbus_fuzzer_link_options_string "${_modbus_fuzzer_link_options}")
  string(APPEND _modbus_fuzzer_link_flags " ${_modbus_fuzzer_link_options_string}")
endif()

set(_modbus_fuzzer_probe_dir "${CMAKE_BINARY_DIR}/CMakeFiles/modbus_fuzzer_check")
set(_modbus_fuzzer_probe_source "${_modbus_fuzzer_probe_dir}/modbus_fuzzer_smoke.cpp")
file(MAKE_DIRECTORY "${_modbus_fuzzer_probe_dir}")
file(WRITE "${_modbus_fuzzer_probe_source}" "#include <cstddef>\nextern \"C\" int LLVMFuzzerTestOneInput(const unsigned char *, std::size_t);\nextern \"C\" int LLVMFuzzerTestOneInput(const unsigned char *, std::size_t) { return 0; }\nint main() { return LLVMFuzzerTestOneInput(nullptr, 0); }\n")
set(_modbus_fuzzer_probe_binary "${_modbus_fuzzer_probe_dir}/modbus_fuzzer_smoke")
file(REMOVE "${_modbus_fuzzer_probe_binary}")

set(_modbus_fuzzer_probe_command
  "${CMAKE_CXX_COMPILER}"
  -std=c++17
  -fsanitize=fuzzer
  "${_modbus_fuzzer_probe_source}"
  -o "${_modbus_fuzzer_probe_binary}"
)
if(_modbus_fuzzer_link_options)
  list(APPEND _modbus_fuzzer_probe_command ${_modbus_fuzzer_link_options})
endif()
if(_modbus_fuzzer_extra_libs)
  list(APPEND _modbus_fuzzer_probe_command ${_modbus_fuzzer_extra_libs})
endif()

execute_process(
  COMMAND ${_modbus_fuzzer_probe_command}
  WORKING_DIRECTORY "${_modbus_fuzzer_probe_dir}"
  RESULT_VARIABLE _modbus_fuzzer_probe_status
  OUTPUT_VARIABLE _modbus_fuzzer_probe_stdout
  ERROR_VARIABLE _modbus_fuzzer_probe_stderr
)
if(_modbus_fuzzer_probe_status EQUAL 0)
  set(MODBUS_HAS_LIBFUZZER TRUE)
else()
  set(MODBUS_HAS_LIBFUZZER FALSE)
  string(REPLACE "\n" "\n    " _modbus_fuzzer_probe_stderr_indented "${_modbus_fuzzer_probe_stderr}")
  message(WARNING "Skipping fuzzers: compiler ${CMAKE_CXX_COMPILER_ID} lacks -fsanitize=fuzzer support.\n    ${_modbus_fuzzer_probe_stderr_indented}")
  return()
endif()

add_executable(modbus_pdu_fuzz
  ${FUZZ_ROOT}/pdu_decode_fuzz.cpp
)

target_compile_features(modbus_pdu_fuzz PRIVATE cxx_std_17)

target_link_libraries(modbus_pdu_fuzz PRIVATE modbus ${_modbus_fuzzer_extra_libs})

if(MODBUS_WARN_FLAGS)
  target_compile_options(modbus_pdu_fuzz PRIVATE ${MODBUS_WARN_FLAGS})
endif()

target_compile_options(modbus_pdu_fuzz PRIVATE -fsanitize=fuzzer-no-link)
target_link_options(modbus_pdu_fuzz PRIVATE -fsanitize=fuzzer)
if(_modbus_fuzzer_link_options)
  target_link_options(modbus_pdu_fuzz PRIVATE ${_modbus_fuzzer_link_options})
endif()

set_target_properties(modbus_pdu_fuzz PROPERTIES
  OUTPUT_NAME "modbus_pdu_fuzz"
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
)
