# Modbus C Library

[![CI](https://github.com/lgili/modbus/actions/workflows/ci.yml/badge.svg)](https://github.com/lgili/modbus/actions/workflows/ci.yml)
[![Docs](https://github.com/lgili/modbus/actions/workflows/ci.yml/badge.svg?label=docs)](https://lgili.github.io/modbus/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A modern, embedded-friendly implementation of Modbus RTU and Modbus TCP written
in ISO C.  The library focuses on deterministic behaviour, no dynamic memory by
default, and a clean separation between protocol layers so that transports and
HALs can evolve independently.

## Highlights

- **Non-blocking client and server FSMs** – queue-based transaction manager with
  retry/backoff, watchdog and cancellation (Gate 5 + Gate 8).
- **Extensible server registry** – contiguous register regions backed by either
  user callbacks or static storage, with strict bounds checking (Gate 6).
- **RTU and TCP transports** – shared MBAP encoder/decoder, optional
  multi-connection helper for Modbus TCP, and reusable RTU framing utilities.
- **Extended PDU codec** – supports FC 01/02/03/04/05/06/07/0F/10/11/16/17 helpers with
  strict validation, covering the extra function codes from Gate 10.
- **Drop-in mapping helper** – Gate 15 bundles libmodbus-style mapping via
  `mb_server_mapping_init`, speeding up register binding with zero boilerplate.
- **Backpressure & observability** – configurable queue capacity, per-function
  timeouts, poison-pill flush and latency/response metrics for both client and
  server paths (Gate 8).
- **Portable HAL** – POSIX sockets, bare-metal tick adapters and FreeRTOS
  stream/queue bridges expose ready-made transports with optional mutex
  helpers (Gate 9).
- **Tracing & diagnostics** – opt-in hex dumps, structured event callbacks and
  per-function diagnostics counters surfaced through `modbus/observe.h`
  (Gate 11).
- **Heap-free utilities** – ring buffers, memory pool, CRC helpers and logging
  façade ready for embedded targets.
- **Comprehensive tests** – unit, integration, fuzzing harnesses and CI jobs
  covering GCC/Clang/ASan/MSVC plus Modbus TCP interoperability via `mbtget`
  and `modpoll` (Gate 7).

## Project Status (Gates 0–7)

| Gate | Summary | Status |
|------|---------|--------|
| 0 | CMake presets, CI, docs skeleton | ✅ |
| 1 | Core types, errors, utilities | ✅ |
| 2 | PDU codec for FC 03/06/16 | ✅ |
| 3 | Transport abstraction & RTU framing | ✅ |
| 4 | Minimal RTU parser/transmitter | ✅ |
| 5 | Client FSM (Idle→Done/Timeout/Retry/Abort) | ✅ |
| 6 | Server register mapping & exceptions | ✅ |
| 7 | Modbus TCP/MBAP + multi-slot helper | ✅ |
| 8 | Robustness: backpressure, priorities, poison, metrics | ✅ |
| 9 | Port/HAL scaffolding (POSIX, FreeRTOS, bare) | ✅ |
| 10 | Extra FCs (01/02/04/05/0F/17) + ASCII transport | ✅ |
| 11 | Observability & debug (events, tracing, diagnostics) | ✅ |
| 15 | Compatibility & port conveniences | ✅ |
| 14 | Protocol parity & data helpers | ✅ |

## Function code coverage

| FC  | Name                      | Scope               | Status |
|-----|---------------------------|---------------------|--------|
|0x01 | Read Coils                | Client/Server       | ✅ |
|0x02 | Read Discrete Inputs      | Client/Server       | ✅ |
|0x03 | Read Holding Registers    | Client/Server       | ✅ |
|0x04 | Read Input Registers      | Client/Server       | ✅ |
|0x05 | Write Single Coil         | Client/Server       | ✅ |
|0x06 | Write Single Register     | Client/Server       | ✅ |
|0x07 | Read Exception Status     | Client              | ✅ |
|0x0F | Write Multiple Coils      | Client/Server       | ✅ |
|0x10 | Write Multiple Registers  | Client/Server       | ✅ |
|0x11 | Report Server ID          | Server              | ✅ |
|0x16 | Mask Write Register       | Client/Server       | ✅ |
|0x17 | Read/Write Multiple Regs  | Client/Server       | ✅ |

ASCII transport now complements the RTU and TCP implementations with
colon-framed LRC-validated links.

## libmodbus parity helpers

Starting at Gate 14 the library exposes 32-bit integer and `float` converters
compatible with `libmodbus` (`modbus_get_uint32_*`, `modbus_get_float_*` and
the matching setters). Each variant honours the byte-order conventions
ABCD/DCBA/BADC/CDAB, making migrations painless without manual buffer shuffles.
Refer to `modbus/utils.h` for the full getter/setter roster.

Gate 15 extends the compatibility story with:

- `mb_server_mapping_init`/`mb_server_mapping_apply` to replicate
  `modbus_mapping_new_start_address` without dynamic allocation.
- Installation artifacts for both `find_package(Modbus CONFIG)` and
  `pkg-config --cflags --libs modbus`.
- A self-contained `modbus_unit_test_loop_demo` example that mirrors the
  libmodbus unit-test client/server flow on top of the cooperative FSMs.

Future gates (8+) track robustness, HAL ports, additional FCs and release
hardening.  See `update_plan.md` for the full roadmap.

## Quick Start

### Requirements

- CMake ≥ 3.20, Ninja (recommended)
- C11-capable toolchain (GCC, Clang, MSVC, MinGW)
- Python 3 for documentation (Sphinx, Breathe)
- Doxygen + Graphviz (optional, for docs)

### Configure & Build

```bash
cmake --preset host-debug
cmake --build --preset host-debug
```

### Run All Tests

Unit tests are enabled by default in the `host-*` presets.  After configuring
with `host-debug`, the command below exercises the full suite:

```bash
ctest --output-on-failure --test-dir build/host-debug
```

Integration tests (client/server transport + observability flows) are guarded
behind `MODBUS_ENABLE_INTEGRATION_TESTS`.  Configure a dedicated build tree and
invoke ctest from there:

```bash
cmake -S . -B build/host-debug-integ -G Ninja \
  -DMODBUS_ENABLE_TESTS=ON \
  -DMODBUS_ENABLE_INTEGRATION_TESTS=ON
cmake --build build/host-debug-integ
ctest --output-on-failure --test-dir build/host-debug-integ
```

Both commands honour `CTEST_PARALLEL_LEVEL` if you want to run tests in
parallel.

Release build:

```bash
cmake --preset host-release
cmake --build --preset host-release
```

### Minimise footprint

The library can be built with only the client or server runtime to trim flash
usage on embedded targets. Disable the unused component at configure time:

```bash
cmake -S . -B build/host-server-only -G Ninja \
  -DMODBUS_ENABLE_CLIENT=OFF

cmake -S . -B build/host-client-only -G Ninja \
  -DMODBUS_ENABLE_SERVER=OFF
```

Each option defaults to `ON`; at least one must remain enabled. When a feature
is disabled its public headers are omitted (or raise a compile-time error if
included directly), preventing accidental linkage against non-existent code.

Sanitizers:

```bash
cmake --preset host-asan
cmake --build --preset host-asan
ctest --output-on-failure --test-dir build/host-asan
```

### Generate Documentation

```bash
cmake --preset host-docs
cmake --build --preset docs
```

HTML output is produced under `build/docs/html` and is automatically published
to GitHub Pages (`gh-pages` branch) by the CI workflow.

### Consume with CMake

```cmake
find_package(Modbus CONFIG REQUIRED)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE Modbus::modbus)
```

Installation exports live under `${prefix}/lib/cmake/Modbus`. Any extra
components (examples, docs) follow the same namespace.

### Consume with pkg-config

```bash
pkg-config --cflags --libs modbus
```

The generated `modbus.pc` honours the install prefix and exposes both static
and shared builds.

## Examples

Examples are disabled by default. Enable them explicitly when configuring and
build the desired targets (a dedicated CMake preset is provided for convenience):

```bash
cmake --preset host-debug-examples
cmake --build --preset host-debug-examples --target \
  modbus_tcp_server_demo \
  modbus_tcp_client_cli \
  modbus_unit_test_loop_demo \
  modbus_rtu_loop_demo \
  modbus_rtu_serial_server \
  modbus_rtu_serial_client
```

### Running the TCP client/server demos

After the build completes the binaries live under
`build/host-debug-examples/examples/`. Open two terminals:

```bash
# Terminal 1 – start the TCP server (press Ctrl+C to stop)
./build/host-debug-examples/examples/modbus_tcp_server_demo \
  --port 1502 \
  --unit 17 \
  --trace

# Terminal 2 – run the TCP client against the server above
./build/host-debug-examples/examples/modbus_tcp_client_cli \
  --host 127.0.0.1 \
  --port 1502 \
  --unit 17 \
  --register 0 \
  --count 4
```

Adjust the host, port, unit id and register range to match your setup. Add
`--expect v1,v2,...` to assert on returned holding registers.

### Running the RTU loopback demo

The RTU example spins an in-memory client/server pair and prints the observed
transactions:

```bash
./build/host-debug-examples/examples/modbus_rtu_loop_demo
```

### Running the RTU serial demos

The serial demos bind directly to a real COM/TTY device. You can point them to
USB-to-RS485 adapters or virtual pairs for local testing.

**Create a virtual serial port pair (optional)**

- **Windows** – install [com0com](https://sourceforge.net/projects/com0com/) and
  create a linked pair (for example `COM5` ↔ `COM6`). Disable strict baud checks
  if your adapter needs custom rates.
- **macOS/Linux** – use `socat` to spawn two linked pseudo-terminals:

  ```bash
  socat -d -d pty,raw,echo=0,link=./ttyV0 pty,raw,echo=0,link=./ttyV1
  ```

  Leave the command running so the devices remain available. The pair will show
  up as `/dev/ttyV0` and `/dev/ttyV1`.

**Start the RTU server**

```bash
./build/host-debug-examples/examples/modbus_rtu_serial_server \
  --device /dev/ttyV0 \
  --baud 115200 \
  --unit 17 \
  --trace
```

**Run the RTU client**

```bash
./build/host-debug-examples/examples/modbus_rtu_serial_client \
  --device /dev/ttyV1 \
  --baud 115200 \
  --unit 17 \
  --interval 1000 \
  --trace
```

Swap `/dev/ttyV*` for the COM port of your USB-to-serial adapter when talking
to real hardware. The server demo updates a small bank of holding registers in
real time; the client polls them periodically and logs the values it receives.

Once configured, the following additional example binaries become available:

- `modbus_tcp_client_cli` – cross-platform CLI driver for Modbus TCP endpoints (POSIX sockets or Winsock via the shared helper).
- `modbus_tcp_server_demo` – single-connection Modbus TCP server with live register updates and tracing hooks (macOS/Linux/Windows).
- `modbus_tcp_multi_client_demo` – fan-out client that concurrently polls multiple Modbus TCP servers using the multi-transport helper.
- `modbus_rtu_loop_demo` – self-contained RTU client/server loop using the in-memory transport shim for quick sanity checks.
- `modbus_unit_test_loop_demo` – Gate 15 showcase that replicates the libmodbus
  unit-test client/server exchange with the new mapping helpers.
- `modbus_rtu_serial_server` – binds the RTU server FSM to a real serial port/USB adapter using the portable helper abstraction.
- `modbus_rtu_serial_client` – polls holding registers from a serial Modbus slave (or the server demo) with optional trace output.
- `modbus_freertos_sim` – FreeRTOS-style stream/queue simulation backed by `mb_port_freertos_transport_init`.
- `modbus_bare_sim` – bare-metal loop example using `mb_port_bare_transport_init` with a synthetic tick source.
- `modbus_posix_sim` – socketpair demo wrapping a POSIX descriptor via `mb_port_posix_socket_init` (available on non-Windows hosts).

The CLI can be exercised against the CI Modbus TCP server or any compatible
PLC/simulator, while the simulation binaries provide minimal host-side sanity
checks for each HAL adapter.

## Documentation

The full API reference (generated via Doxygen + Sphinx + Breathe) lives at
<https://lgili.github.io/modbus/>.  It covers the current surface of the library
only; legacy APIs have been removed.  Once the docs preset is built you can dive
into:

- [Cookbook](doc/build/html/cookbook.html) – ready-made client/server recipes,
  observability hooks and testing patterns.
- [Migration guide](doc/build/html/migration.html) – breaking changes and
  adoption checklist for 1.x upgrades.

## Developer Notes

- **Formatting** – `.clang-format` holds the project style; run `clang-format -i`
  on modified files.
- **Static analysis** – `.clang-tidy` ships with the security and CERT bundles;
  run it locally via `cmake --preset host-debug -DCMAKE_C_CLANG_TIDY=clang-tidy`.
- **Fuzzing** – build the fuzz harness with `-DMODBUS_BUILD_FUZZERS=ON` (requires
  Clang/libFuzzer).
- **Coverage** – `-DMODBUS_ENABLE_COVERAGE=ON` plus the `coverage` target
  produces LCOV/HTML reports.
- **System GTest** – optional via `-DMODBUS_USE_SYSTEM_GTEST=ON`.

## Security & Hardening

- **Compiler flags** – non-MSVC builds now opt into `-fno-common` and
  `-fstack-protector-strong`; Release/RelWithDebInfo also add `_FORTIFY_SOURCE=2`
  and RELRO/Now on ELF platforms.
- **CI pipeline** – dedicated `lint`, `coverage` and `fuzz` jobs keep the
  security profile, instrumentation, and libFuzzer harnesses green alongside
  the existing multi-platform builds.
- **MISRA checklist** – the documentation set includes
  [a living MISRA-C compliance checklist](doc/build/html/misra.html) that tracks
  enforcement hooks and justified deviations.

## Contributing

Bug reports, feature suggestions and pull requests are welcome.  Please:

1. Open an issue describing the motivation/scope.
2. Follow the coding style and keep builds warning-free (`-Werror`).
3. Provide tests for new behaviour.
4. Run the `host-debug` preset (and, when relevant, sanitizers/docs) before
   submitting.

See `CONTRIBUTING.md` for full guidelines.

## License

This project is released under the MIT License.  See `LICENSE` for details.
