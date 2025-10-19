# ModbusCore Examples

This directory contains four standalone samples that exercise the ModbusCore
runtime end-to-end. Each executable demonstrates one half of the protocol
pair — run the server and client together to see requests and responses flow
through the full DI stack.

| Example              | Role    | Transport | Notes |
|----------------------|---------|-----------|-------|
| `modbus_tcp_server`  | Server  | TCP       | Listens on a configurable port and serves FC03/FC06/FC16 using an in-memory register table. |
| `modbus_tcp_client`  | Client  | TCP       | Connects to the server, reads registers, writes a value, and verifies the write. |
| `modbus_rtu_server`  | Server  | RTU       | Awaits FC03/FC06/FC16 requests over a serial link and answers from an in-memory register table. |
| `modbus_rtu_client`  | Client  | RTU       | Talks to the RTU server, mirroring the TCP client flow but over UART. |

All examples are platform aware: on POSIX they use the POSIX drivers, and on
Windows they automatically switch to Winsock/Win32 transports.

## Building

From the project root:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target modbus_tcp_server modbus_tcp_client modbus_rtu_server modbus_rtu_client
```

Executables are emitted under `build/examples/`.

> 💡 **CLI alternativa:** você pode usar `./scripts/modbus new app ...` para
> gerar rapidamente um projeto customizado (ver [docs/cli/modbus_cli_design.md](../docs/cli/modbus_cli_design.md)).

## TCP Examples

1. Start the server (default port 15020, unit 0x11):

   ```bash
   ./build/examples/modbus_tcp_server
   ```

   Optional flags:

   - `--port <tcp-port>`: override the listen port.
   - `--unit <id>`: set the Modbus unit/slave id (hex or decimal).
   - `--max-requests <n>`: stop after serving N requests (default: run forever).

2. In another terminal, run the client:

   ```bash
   ./build/examples/modbus_tcp_client --host 127.0.0.1 --port 15020 --unit 17
   ```

   Flags:

   - `--host <addr>`: target IP or hostname (default `127.0.0.1`).
   - `--port <tcp-port>`: target port (default `15020`).
   - `--unit <id>`: unit/slave id (default `0x11`).

The client reads four holding registers, writes a single register (value `0x1234`),
then reads again to confirm the update. The server logs each request it handles.

## RTU Examples

The RTU samples talk over a serial link. To run both ends without hardware you
can create a virtual null-modem pair:

- **Windows:** Install [com0com](https://sourceforge.net/projects/com0com/) and
  create a pair such as `COM5` ↔ `COM6`.
- **macOS / Linux:** Use `socat` to build a pseudo-terminal duo:

  ```bash
  socat -d -d pty,raw,echo=0 pty,raw,echo=0
  ```

  The output prints two device paths (e.g., `/dev/ttys010` and `/dev/ttys011`).
  Use one for the server and the other for the client.

### 1. Start the RTU server

```bash
# POSIX
./build/examples/modbus_rtu_server --device /dev/ttys010 --baud 9600 --unit 17

# Windows
modbus_rtu_server.exe --port COM5 --baud 9600 --unit 17
```

Arguments:

- `--device` (POSIX) / `--port` (Windows): serial endpoint.
- `--baud`: baud rate (default `9600`).
- `--unit`: Modbus unit id (default `0x11`).

The server initialises a register bank (values `0…63`) and waits indefinitely.

### 2. Run the RTU client

```bash
# POSIX
./build/examples/modbus_rtu_client --device /dev/ttys011 --baud 9600 --unit 17

# Windows
modbus_rtu_client.exe --port COM6 --baud 9600 --unit 17
```

The client mirrors the TCP interaction: read four registers, write register `1`
to `0x4321`, then read again to confirm.

> **Tip (POSIX):** If you need multiple sessions, assign stable names to the
> PTYs using symlinks: `ln -s /dev/ttys010 /tmp/modbus_server` etc., then point
> the examples at the symlink.

## Platform Notes

- On POSIX systems the build enables the POSIX TCP/RTU drivers automatically.
- On Windows the examples link against Winsock/Win32 RTU drivers; make sure the
  Visual C++ runtime is available (included with modern toolchains).
- Both RTU samples rely on hardware flow control being disabled; ensure your
  virtual COM pair or USB adapter is configured accordingly.

## Next Steps

These examples are intentionally small. Feel free to copy them as templates for
your own applications—swap the register table for your business logic, or plug
in custom transports by replacing the driver setup section.
