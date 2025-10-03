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
  retry/backoff, watchdog and cancellation (Gate 5).
- **Extensible server registry** – contiguous register regions backed by either
  user callbacks or static storage, with strict bounds checking (Gate 6).
- **RTU and TCP transports** – shared MBAP encoder/decoder, optional
  multi-connection helper for Modbus TCP, and reusable RTU framing utilities.
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
ctest --output-on-failure --test-dir build/host-debug
```

Release build:

```bash
cmake --preset host-release
cmake --build --preset host-release
```

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

## Examples

Examples are disabled by default.  Enable them explicitly when configuring and
build the desired target:

```bash
cmake --preset host-release -DMODBUS_BUILD_EXAMPLES=ON
cmake --build --preset host-release --target modbus_tcp_client_cli
```

The CLI can be exercised against the CI Modbus TCP server or any compatible
PLC/simulator.

## Documentation

The full API reference (generated via Doxygen + Sphinx + Breathe) lives at
<https://lgili.github.io/modbus/>.  It covers the current surface of the library
only; legacy APIs have been removed.

## Developer Notes

- **Formatting** – `.clang-format` holds the project style; run `clang-format -i`
  on modified files.
- **Static analysis** – enable `.clang-tidy` with `run-clang-tidy -p build/host-debug`.
- **Fuzzing** – build the fuzz harness with `-DMODBUS_BUILD_FUZZERS=ON` (requires
  Clang/libFuzzer).
- **Coverage** – `-DMODBUS_ENABLE_COVERAGE=ON` plus the `coverage` target
  produces LCOV/HTML reports.
- **System GTest** – optional via `-DMODBUS_USE_SYSTEM_GTEST=ON`.

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
