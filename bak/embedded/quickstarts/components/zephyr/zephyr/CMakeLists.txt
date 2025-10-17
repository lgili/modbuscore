zephyr_include_directories(
    include
    ../../../drop_in
)

zephyr_library()

zephyr_library_sources(
    src/modbus_zephyr_quickstart.c
    ../../../drop_in/modbus_amalgamated.c
)
