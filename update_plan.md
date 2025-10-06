Let's architect this C Modbus library as a production-grade product: modular, portable, deadlock-free, fully tested, and well documented. Below is an incremental plan split into objective gates—move forward only when the gate is green.

Vision & principles
	•	Portable: “pure” C11 (C99 fallback), no malloc by default (optional pool), zero hard dependencies.
	•	Modular: clearly split PDU codec, Transport (RTU/TCP/ASCII), Core FSM (client/server), HAL/Port (UART/TCP, timers), Buffers/Queues, and Logging.
	•	Deadlock-proof: fully non-blocking, per-state timeouts, internal watchdog, transaction queue with retries and cancellation.
	•	Safe: strict bounds and opcode validation, PDU size limits (≤ 253 bytes), CRC/MBAP guards, overflow/underflow checks, UB-safe code paths.
	•	Observable: metrics, structured events, log levels, tracing hooks.
	•	Testable: TDD, fuzzers on decoders, sanitizers, coverage, and CI coverage.

⸻

Layered architecture

app (examples) ─────────────────────────────────────────────────────┐
                                                                    │
             ┌───────────── Core FSM ───────────────┐               │
             │  Client (master)  |  Server (slave)                 │
             │  Transactions, timeouts, retries, watchdog          │
             └──────────────────────────────────────┘               │
             ┌────────────  PDU Codec  ────────────┐                │
             │  encode/decode FCs, exceptions, limits               │
             └──────────────────────────────────────┘               │
             ┌──────────  Transport abstraction ───┐                │
             │  RTU | ASCII | TCP (plug-ins)        │               │
             └──────────────────────────────────────┘               │
             ┌───────────  Port/HAL  ───────────────┐               │
             │  UART/TCP, timers, monotonic clock, optional mutex   │
             └──────────────────────────────────────┘               │
             ┌────────── Utilities: Buffers/Queue/CRC/Log ─┐        │
             └─────────────────────────────────────────────┘        │

	•	Core FSM: poll-based finite state machine for Rx/Tx, per-transaction deadlines.
	•	PDU Codec: builders/parsers (FC03/04/06/16/…), exception helpers, limit tables.
	•	Transport: RTU (CRC/silence timing), TCP (MBAP/TID), ASCII (optional).
	•	Port/HAL: function pointers for read, write, now_ms, sleep(0), yield(), tick source, optional locks.
	•	Utilities: lock-free SPSC ring buffer, fixed pool, CRC16, MBAP helpers, logger façade.

⸻

Repository layout

/include/modbus/          # public headers
/src/core/                # FSM, transaction manager
/src/pdu/                 # encoders/decoders
/src/transport/rtu/       # RTU transport
/src/transport/tcp/       # TCP transport
/src/port/                # reference ports (posix, freertos, bare)
/src/util/                # crc, ring buffer, pool, log
/examples/                # client/server demos
/tests/unit/              # unit tests
/tests/fuzz/              # libFuzzer/AFL harnesses
/docs/                    # doxygen + sphinx (breathe)
/cmake/                   # toolchains, options

⸻

Incremental roadmap with gates

Each stage lists Objective, Deliverables, and Gate (acceptance). We only advance when the gate is satisfied.

0) Foundation & build
	•	Objective: project skeleton, build options, coding style.
	•	Deliverables: CMake + presets (gcc/clang/msvc), -Wall -Wextra -Werror, MODBUS_* options, clang-format, basic Doxygen, GitHub Actions CI matrix.
	•	Gate: CI green across three toolchains, style checks pass, docs generated.

1) Types, errors, logging, utilities
	•	Deliverables: mb_types.h (fixed types), mb_err.h, MB_LOG(level, ...), SPSC ring buffer, CRC16 helpers.
	•	Gate: 100% unit coverage on utilities; ASan/UBSan clean runs.

2) PDU codec (core)
	•	Deliverables: encode/decode FC 0x03, 0x06, 0x10 + exceptions; enforce PDU ≤ 253.
	•	Gate: golden-table tests (valid/invalid vectors), coverage ≥ 90%, fuzz decoders for 5 minutes without crashes.

3) Transport abstraction
	•	Deliverables: mb_transport_if.h (non-blocking send/recv), mb_frame.h scaffolding.
	•	Gate: transport mock passes integration tests with the PDU layer.

4) Minimal RTU
	•	Deliverables: RTU framing (address, PDU, CRC), stateful parser with 3.5 char silence timeout, inter-char TX pacing.
	•	Gate: loopback against mock port and simulated UART; lost/duplicated byte tests pass.

5) Client core FSM
	•	Deliverables: transaction queue, Idle→Sending→Waiting→Done/Timeout/Retry/Abort states, exponential backoff retries, cancel support.
	•	Gate: stress tests (1k requests), simulated packet loss, zero deadlocks; watchdog never fires in happy path.

6) Server core
	•	Deliverables: register table with read/write callbacks, range checks, exception generation.
	•	Gate: basic compliance (local client ↔ server) with FC 03/06/16; invalid frame injection maps to correct exceptions.

7) TCP (MBAP)
	•	Deliverables: MBAP header (TID, length, unit), TID↔transaction mapping, optional multi-connection helper.
	•	Gate: CI integration tests against mbtget/modpoll (docker); stable throughput at 1k rps (mock).

8) Advanced robustness
	•	Deliverables: backpressure (queue caps), anti-head-of-line priorities, per-FC timeouts, poison pill flush, metrics (counters/latency).
	•	Gate: chaos tests (latency/bursts/loss 20–40%), no starvation, valgrind clean.

9) Port/HALs
	•	Deliverables: port_posix, port_freertos, port_bare (tick + UART/TCP), optional locks (C11 atomics fallback).
	•	Status: ✅ POSIX sockets, bare-metal tick adapter, FreeRTOS stream/queue shim, and portable mutexes delivered.
	•	Gate: examples run on POSIX and simulated FreeRTOS.

10) ASCII (optional) + extra FCs
	•	Deliverables: ASCII framing; extend FC coverage (01/02/04/05/0F/17 as needed).
	•	Status: ✅ FC helpers and ASCII transport merged with unit coverage mirroring RTU/TCP.
	•	Gate: same regression suite as RTU/TCP.

11) Observability & debug
	•	Deliverables: state enter/leave events, optional hex trace, observability callback, error/function counters, MB_DIAG plumbing.
	•	Gate: example surfaces useful metrics and diagnoses induced failures.

12) Documentation & API stability
	•	Deliverables: porting guide, migration guide, cookbook (client/server), complete Doxygen, SemVer 1.0.0 release.
	•	Gate: docs build warning-free; examples build and run; breaking-change review yields zero blockers.

13) Hardening & release
	•	Deliverables: security-focused clang-tidy profile, MISRA-C checklist (non-strict), -fno-common, -fstack-protector, FORTIFY_SOURCE when available.
	•	Gate: full CI (sanitizers + fuzz + coverage + linters) green; tag v1.0.0.
	•	Status: ✅ clang-tidy tightened, hardening flags enabled, coverage/fuzz/lint jobs active in CI.

14) Protocol parity & data helpers
	•	Deliverables: close remaining FC gaps (0x05/0x07/0x11/0x16/0x17), extend exception catalog, add endian conversion helpers (float/int setters/getters mirroring libmodbus).
	•	Gate: test matrix covering all new FCs, documentation updated with support table, API comparison versus libmodbus.
	•	Status: ✅ deliverables shipped and documented.

15) Compatibility & port convenience
	•	Deliverables: libmodbus-style mapping helpers (`mb_server_mapping_init`), generated `pkg-config` + `find_package` artifacts, expanded migration guidance, demo mirroring the classic libmodbus unit-test client/server pairing.
	•	Gate: examples build using the installed package, migration guide highlights Gate 15 helpers, CI exercises the unit-test loop demo alongside existing samples.
	•	Status: ✅ helpers, packaging, docs and demo merged.

16) Footprint & feature toggles
	• Deliverables: build flags to enable/disable client/server/transports, footprint documentation, CI presets validating combinations (client-only, server-only), flash/RAM usage metrics.
	• Gate: dedicated builds pass regression suite with single component enabled, report demonstrates ≥20% footprint reduction on reference targets.
	• Progress 2025-10-05: Added `MODBUS_ENABLE_TRANSPORT_{RTU,ASCII,TCP}` options with matching `MB_CONF_` macros, gated transport headers/sources/tests, and introduced CMake presets for client-only/server-only and transport-specific builds. Guarded client FSM against missing transports, restricted server to RTU-capable builds, and validated host-debug, host-rtu-only, and host-tcp-only presets (with tailored test selections).
