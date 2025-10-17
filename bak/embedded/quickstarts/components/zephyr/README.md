# Zephyr Modbus quickstart module

This quickstart exposes the amalgamated Modbus client as a Zephyr module and
bundles a minimal Modbus/TCP transport so you can speak Modbus from any Zephyr
application without integrating the full library tree.

## Layout

- `module.yml` – manifest consumed by the Zephyr module loader.
- `zephyr/CMakeLists.txt` – adds the amalgamated translation unit and the TCP
  transport shim to the build.
- `zephyr/Kconfig.modbus` – menu entries to tune queue depth, socket timeouts,
  and cooperative yield hints.
- `zephyr/include/modbus_zephyr_quickstart.h` – helper API surfaced to
  applications.
- `zephyr/src/modbus_zephyr_quickstart.c` – sockets-based `mb_transport_if_t`
  implementation.
- `zephyr/samples/hello_tcp/` – reference application that connects to a
  configurable Modbus/TCP endpoint and polls the client.

The module reuses the generated
[`../../drop_in/modbus_amalgamated.c`](../../drop_in/modbus_amalgamated.c) and
header. Regenerate them whenever you tweak the core library:

```sh
python3 scripts/amalgamate.py
```

## Using the module in an application

1. Copy `embedded/quickstarts/drop_in/` and
   `embedded/quickstarts/components/zephyr/` into your project (for example
   under `modules/modbus_quickstart`).
2. Teach CMake where to find it by adding to your project `CMakeLists.txt`:

   ```cmake
   list(APPEND ZEPHYR_EXTRA_MODULES "${CMAKE_SOURCE_DIR}/modules/modbus_quickstart")
   ```

3. Enable the menu options under **Modules → Modbus quickstart** if you want to
   adjust the transaction pool size, client queue capacity or socket timeout.
4. Either build the bundled sample (`west build -b <board>
   modules/modbus_quickstart/zephyr/samples/hello_tcp`) or call the helper from
   your own application:

   ```c
   #include <modbus_zephyr_quickstart.h>
   #include <zephyr/net/socket.h>

   static void on_response(struct mb_client *client,
               const struct mb_client_txn *txn,
               mb_err_t status,
               const mb_adu_view_t *response,
               void *user_ctx)
   {
     ARG_UNUSED(client);
     ARG_UNUSED(txn);
     ARG_UNUSED(user_ctx);
     if (mb_err_is_ok(status)) {
       printk("Received %u payload bytes\n", response->payload_len);
     } else {
       printk("Modbus error %d\n", status);
     }
   }

   int main(void)
   {
       modbus_zephyr_client_t modbus;
       if (modbus_zephyr_client_init(&modbus) != MB_OK) {
           return 0;
       }

       struct sockaddr_in server = {
           .sin_family = AF_INET,
           .sin_port = htons(CONFIG_MODBUS_HELLO_SERVER_PORT),
       };
       zsock_inet_pton(AF_INET, CONFIG_MODBUS_HELLO_SERVER_ADDR, &server.sin_addr);

       if (modbus_zephyr_client_connect(&modbus,
                                        (struct sockaddr *)&server,
                                        sizeof(server)) != 0) {
           return 0;
       }

  (void)modbus_zephyr_submit_read_holding(&modbus,
                 1,
                 0x0000,
                 4,
                 on_response,
                 NULL);

       while (1) {
           mb_client_poll(&modbus.client);
           k_msleep(10);
       }
   }
   ```

## Extending the helper

- Replace the TCP shim with a UART-based transport once you move towards
  hardware RTU deployments.
- Call `modbus_zephyr_client_shutdown()` during teardown to close sockets
  cleanly and release resources.
- Combine with Zephyr work queues or dedicated threads to control polling
  cadence and integrate power-management hooks.

The helper intentionally stays small—no automatic reconnects or TLS. Feel free
to fork the transport implementation to match your production requirements.
