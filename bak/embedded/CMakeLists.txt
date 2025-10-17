# Placeholder CMake scaffolding for embedded integration assets.
# This keeps the directory visible to IDEs and primes the tree for future build glue.

set(_EMBEDDED_ASSETS
    README.md
    porting-wizard.md
    quickstarts/README.md
    quickstarts/drop_in/README.md
    quickstarts/drop_in/modbus_amalgamated.c
    quickstarts/drop_in/modbus_amalgamated.h
    checklists/README.md
    ports/README.md
    ports/stm32-ll-dma-idle/README.md
    ports/nxp-lpuart-idle/README.md
)

add_custom_target(modbus_embedded_assets SOURCES ${_EMBEDDED_ASSETS})
