# Toolchain file for building Modbus in a bare-metal ARM Cortex-M (STM32G0) context.
#
# This toolchain relies on the GNU Arm Embedded toolchain (arm-none-eabi-gcc).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m0plus)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_AR arm-none-eabi-ar CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB arm-none-eabi-ranlib CACHE FILEPATH "" FORCE)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m0plus -mthumb")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m0plus -mthumb")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=cortex-m0plus -mthumb")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-mcpu=cortex-m0plus -mthumb")
