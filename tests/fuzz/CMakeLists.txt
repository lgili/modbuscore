#
#Fuzzing targets for ModbusCore
#
#Build with:
#cmake - DENABLE_FUZZING = ON - DCMAKE_C_COMPILER = clang - DCMAKE_BUILD_TYPE = Debug..
#make
#
#Run with:
#./ fuzz_mbap_decoder corpus /
#./ fuzz_rtu_crc corpus /
#./ fuzz_pdu_parser corpus /
#

if (NOT ENABLE_FUZZING)
    message(STATUS "Fuzzing disabled (use -DENABLE_FUZZING=ON to enable)")
    return()
endif()

message(STATUS "Fuzzing enabled - building fuzz targets")

#Check for LibFuzzer support
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzzing requires Clang compiler (use -DCMAKE_C_COMPILER=clang)")
endif()

#Fuzzing flags
#On macOS, LibFuzzer requires C++ runtime, so we compile as C++
if(APPLE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_language(CXX)
endif()

set(FUZZ_COMPILE_BASE -g;-O1;-fsanitize=address;-fsanitize=undefined)
set(FUZZ_COMPILE_FLAGS ${FUZZ_COMPILE_BASE})
list(APPEND FUZZ_COMPILE_FLAGS -fsanitize=fuzzer-no-link)
set(FUZZ_LINK_FLAGS -fsanitize=address;-fsanitize=undefined;-fsanitize=fuzzer)

#-- - Fuzzer 1 : MBAP Decoder -- -
add_executable(fuzz_mbap_decoder
    fuzz_mbap_decoder.c
    ${CMAKE_SOURCE_DIR}/src/protocol/mbap.c
)

#On macOS, compile as C++
if(APPLE)
    set_source_files_properties(
        fuzz_mbap_decoder.c
        ${CMAKE_SOURCE_DIR}/src/protocol/mbap.c
        PROPERTIES LANGUAGE CXX
    )
endif()

target_include_directories(fuzz_mbap_decoder PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(fuzz_mbap_decoder PRIVATE
    MBC_ENABLE_LOGGING=0  # Disable logging for performance
)

target_compile_options(fuzz_mbap_decoder PRIVATE ${FUZZ_COMPILE_FLAGS})
target_link_options(fuzz_mbap_decoder PRIVATE ${FUZZ_LINK_FLAGS})

#-- - Fuzzer 2 : RTU CRC -- -
add_executable(fuzz_rtu_crc
    fuzz_rtu_crc.c
    ${CMAKE_SOURCE_DIR}/src/protocol/crc.c
)

#On macOS, compile as C++
if(APPLE)
    set_source_files_properties(
        fuzz_rtu_crc.c
        ${CMAKE_SOURCE_DIR}/src/protocol/crc.c
        PROPERTIES LANGUAGE CXX
    )
endif()

target_include_directories(fuzz_rtu_crc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(fuzz_rtu_crc PRIVATE
    MBC_ENABLE_LOGGING=0
)

target_compile_options(fuzz_rtu_crc PRIVATE ${FUZZ_COMPILE_FLAGS})
target_link_options(fuzz_rtu_crc PRIVATE ${FUZZ_LINK_FLAGS})

#-- - Fuzzer 3 : PDU Parser -- -
add_executable(fuzz_pdu_parser
    fuzz_pdu_parser.c
    ${CMAKE_SOURCE_DIR}/src/protocol/pdu.c
    ${CMAKE_SOURCE_DIR}/src/protocol/crc.c
)

#On macOS, compile as C++
if(APPLE)
    set_source_files_properties(
        fuzz_pdu_parser.c
        ${CMAKE_SOURCE_DIR}/src/protocol/pdu.c
        ${CMAKE_SOURCE_DIR}/src/protocol/crc.c
        PROPERTIES LANGUAGE CXX
    )
endif()

target_include_directories(fuzz_pdu_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(fuzz_pdu_parser PRIVATE
    MBC_ENABLE_LOGGING=0
)

target_compile_options(fuzz_pdu_parser PRIVATE ${FUZZ_COMPILE_FLAGS})
target_link_options(fuzz_pdu_parser PRIVATE ${FUZZ_LINK_FLAGS})

#-- - Corpus Generator(not a fuzzer, just a helper tool) -- -
#Note : This is compiled WITHOUT fuzzing flags since it's a standalone tool
add_executable(generate_corpus
    corpus/generate_corpus.c
)

#Compile without fuzzing flags
target_compile_options(generate_corpus PRIVATE -O2)

target_link_libraries(generate_corpus PRIVATE
    modbuscore
)

#Custom target to run corpus generator
add_custom_target(generate_fuzz_corpus
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/generate_corpus
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/corpus
    DEPENDS generate_corpus
    COMMENT "Generating fuzzing corpus files..."
)

#-- - Installation -- -
install(TARGETS fuzz_mbap_decoder fuzz_rtu_crc fuzz_pdu_parser
    RUNTIME DESTINATION bin/fuzz
)

#-- - Summary -- -
message(STATUS "")
message(STATUS "Fuzzing targets configured:")
message(STATUS "  - fuzz_mbap_decoder (MBAP frame decoder)")
message(STATUS "  - fuzz_rtu_crc (RTU CRC validation)")
message(STATUS "  - fuzz_pdu_parser (PDU encoding/parsing)")
message(STATUS "")
message(STATUS "To generate seed corpus: make generate_fuzz_corpus")
message(STATUS "To run fuzzer: ./tests/fuzz/fuzz_mbap_decoder corpus/")
message(STATUS "")
