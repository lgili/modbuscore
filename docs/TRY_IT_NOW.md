# Try It Now — 90 Second Tour

Want to confirm ModbusCore is alive on your machine before wiring real
hardware? The commands below build the library, run the loopback demo, and
exercise the optional libmodbus compatibility layer.

## 1. Configure & Build

```bash
cmake --preset host-debug
cmake --build --preset host-debug
```

All binaries live under `build/host-debug/examples/`.

## 2. Run the Loopback Demo

The loopback target connects the client and server cores through an in-memory
transport so you can watch FC 03/10 succeed without cabling.

```bash
cmake --build --preset host-debug --target modbus_loopback_demo
./build/host-debug/examples/modbus_loopback_demo
```

Sample output:

```
== Modbus loopback demo ==
Initial holding registers:
    [00] 0x1000
    [01] 0x1100
    [02] 0x1200
    [03] 0x1300
Holding registers after write:
    [00] 0x1000
    [01] 0x1100
    [02] 0xA5A5
    [03] 0x5A5A
```

## 3. Smoke-Test the Compatibility Layer

Reconfigure with the compat preset and run the migration demo. It exercises the
libmodbus-style API (FC 03/06/16) without modifying application source.

```bash
cmake --preset host-compat
cmake --build --preset host-compat --target libmodbus_migration_demo
./build/host-compat/examples/libmodbus_migration_demo 127.0.0.1 1502
```

Pair it with `examples/tcp_server_demo` or your existing Modbus TCP endpoint.

## 4. Run the Test Suite (Optional)

```bash
ctest --preset all --output-on-failure
ctest --preset compat --output-on-failure
```

Those commands mirror the gates enforced in CI, keeping your local checkout in
sync with the release pipeline.
