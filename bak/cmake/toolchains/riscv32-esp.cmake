# Toolchain file for building Modbus targeting ESP32-C3 style RISC-V MCUs.
#
# Uses the GNU RISC-V Embedded toolchain (riscv64-unknown-elf-gcc).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-elf-g++)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)

set(CMAKE_AR riscv64-unknown-elf-ar CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB riscv64-unknown-elf-ranlib CACHE FILEPATH "" FORCE)

set(CMAKE_C_FLAGS_INIT "-march=rv32imc -mabi=ilp32")
set(CMAKE_CXX_FLAGS_INIT "-march=rv32imc -mabi=ilp32")
set(CMAKE_ASM_FLAGS_INIT "-march=rv32imc -mabi=ilp32")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-march=rv32imc -mabi=ilp32")
