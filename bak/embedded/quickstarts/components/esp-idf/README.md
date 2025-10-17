# ESP-IDF Modbus quickstart component

This quickstart packages the single-file Modbus RTU client into an ESP-IDF
component so firmware teams can get a working stack in minutes.

## What you get

- `modbus/` component with:
  - `CMakeLists.txt` registering the Modbus amalgamated translation unit and
    a thin UART transport adapter.
  - `Kconfig` options to tune queue depth, UART speeds, and cooperative yields.
  - `idf_component.yml` declaring dependencies on ESP-IDF`s UART driver.
  - `include/modbus_esp_quickstart.h` exposing a tiny helper API.
  - `src/modbus_esp_quickstart.c` implementing a UART backed
    `mb_transport_if_t` and default client bootstrap.

The component reuses the generated
[`../../drop_in/modbus_amalgamated.c`](../../drop_in/modbus_amalgamated.c) and its
header. Make sure you regenerate them whenever you tweak the library by running:

```sh
python3 scripts/amalgamate.py
```

## How to try it

1. Copy `embedded/quickstarts/drop_in/` **and**
   `embedded/quickstarts/components/esp-idf/modbus/` into your ESP-IDF
   application (for example under `components/modbus/`).
2. Add the component’s folder to your project’s `EXTRA_COMPONENT_DIRS`, e.g. in
   your project `CMakeLists.txt`:

   ```cmake
   set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/components")
   ```

3. Run `idf.py menuconfig` and enable **Component config → Modbus quickstart** to
   adjust UART pins, baud rate, queue depth, and yield behaviour.
4. In your application code:

   ```c
   #include "modbus_esp_quickstart.h"

   void app_main(void)
   {
       modbus_esp_quickstart_config_t cfg = modbus_esp_quickstart_default_config();
       cfg.tx_pin = 17;
       cfg.rx_pin = 16;

       if (modbus_esp_quickstart_init(&cfg) != MB_OK) {
           abort();
       }

       while (true) {
           mb_client_poll(modbus_esp_quickstart_client());
           vTaskDelay(pdMS_TO_TICKS(10));
       }
   }
   ```

   Submit requests with `modbus_esp_quickstart_submit_read()`/`_write()` helpers
   or directly via the returned `mb_client_t` pointer.

5. Build and flash with `idf.py flash monitor`. The helper logs will show when
   the Modbus client queues transactions or encounters transport errors.

## Customising

- Need RS485 direction control? Implement it in `modbus_esp_quickstart.c`’s
  TODO hook or extend the component with your specific GPIO toggling.
- Use `mb_client_set_trace_hex()` or diagnostics helpers from the
  amalgamated header to log traffic.
- When you are ready to replace the drop-in file with a custom build, swap the
  `SRCS` entry inside `CMakeLists.txt` for your own sources.

## Next steps

This quickstart focuses on the RTU client path. Future gates will add
FreeRTOS task wiring examples and IDF component variants for the server stack.
