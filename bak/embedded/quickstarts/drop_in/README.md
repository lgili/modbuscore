# Drop-in single translation unit

The files `modbus_amalgamated.c` and `modbus_amalgamated.h` provide a ready-to-build
Modbus client that targets the baseline RTU transport. They are generated from the
main library sources via `scripts/amalgamate.py` and bundle all required headers
and implementation detail into two compilation units.

## Features

- ✅ Modbus client core with transaction queue, retries, diagnostics counters
- ✅ PDU codec (FC 01/02/03/04/05/06/0F/10/11/16/17)
- ✅ RTU transport implementation (encoder/decoder, CRC16, timing helpers)
- ✅ Baremetal-friendly ring buffer, memory pool and logging helpers
- 🚫 TCP, ASCII and server-side logic are omitted to keep size small

## Using the drop-in pair

1. Copy `modbus_amalgamated.c` and `modbus_amalgamated.h` into your firmware tree.
2. Add the `.c` file to your build system (CMake, Makefile, etc.).
3. Include `modbus_amalgamated.h` from your application sources and wire up a
   transport using `mb_transport_if_t` callbacks.
4. Optionally override configuration macros (for example `MB_CONF_ENABLE_FC03`)
   by defining them **before** including the header.

A minimal CMake snippet:

```cmake
add_library(modbus_drop_in STATIC modbus_amalgamated.c)
target_include_directories(modbus_drop_in PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
```

See the parent `embedded/quickstarts` index for a walkthrough that explains how
this drop-in pairs with the other Gate 17 assets.
