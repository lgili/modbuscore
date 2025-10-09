# Porting From libmodbus

The compatibility layer makes it possible to reuse existing libmodbus-based
applications while progressively adopting the modern ModbusCore APIs.
It focuses on the most common blocking client calls (FC 03/06/16) and
provides drop-in replacements for the libmodbus entry points.

> **Enable the shim**
>
> ```bash
> cmake -DMODBUS_ENABLE_COMPAT_LIBMODBUS=ON -S . -B build
> ```
>
> The build exports the compatibility headers under
> `modbus/compat/`. Swap the includes in your application to:
>
> ```c
> #include <modbus/compat/libmodbus.h>
> ```

## Quick Checklist

- Keep the libmodbus API calls; no behavioural changes are required.
- The example `examples/libmodbus_migration_demo.c` mirrors the legacy
  blocking client sample using the compatibility shim.
- Use the `host-compat` CMake preset to build and run tests with the layer enabled.
- Select the correct transport at creation time (`modbus_new_rtu` or `modbus_new_tcp`).
- Continue using `modbus_set_slave`, `modbus_read_registers`,
  `modbus_write_register`, and `modbus_write_registers`.
- Optional: call `modbus_set_response_timeout()` to tune the blocking timeout.
- Link against `modbus` instead of `libmodbus`.

## Equivalence Table

| Original libmodbus call | Compatibility layer | Modern ModbusCore helper |
| --- | --- | --- |
| `modbus_new_tcp(ip, port)` | Same function | `mb_host_tcp_connect(host:port)` |
| `modbus_new_rtu(dev, baud, parity, data, stop)` | Same function (currently 8N1 only) | `mb_host_rtu_connect(device, baud)` |
| `modbus_connect(ctx)` | Same function | `mb_host_tcp_connect` / `mb_host_rtu_connect` |
| `modbus_close(ctx)` | Same function | `mb_host_disconnect()` |
| `modbus_set_slave(ctx, id)` | Same function | Pass `unit_id` to `mb_host_*` calls |
| `modbus_read_registers(ctx, addr, nb, dest)` | Same function | `mb_host_read_holding()` |
| `modbus_write_register(ctx, addr, value)` | Same function | `mb_host_write_single_register()` |
| `modbus_write_registers(ctx, addr, nb, src)` | Same function | `mb_host_write_multiple_registers()` |
| `modbus_set_response_timeout(ctx, sec, usec)` | Same function | `mb_host_set_timeout()` |
| `modbus_strerror(err)` | Same function | `mb_host_error_string()` |

> **Tip:** When you are ready to migrate fully, switch from
> `modbus/compat/libmodbus.h` to the synchronous host helpers in
> `<modbus/mb_host.h>` or to the asynchronous client API in
> `<modbus/client.h>`.

## Current Limitations

- The shim covers function codes 03, 06 and 16. Other FCs fall back to
  the native APIs described in the table above.
- RTU parity/data/stop options other than 8N1 are currently unsupported.
- The response timeout granularity is milliseconds (internally converted
  to the `mb_host_set_timeout()` call).

We track feature parity under Gate 34. Please report gaps or missing
wrappers as you migrate larger applications.
