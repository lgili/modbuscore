Roadmap & Gates (0–13)
======================

This section summarises the completed gates up to **Gate 13**. Each gate lists
the objectives, key deliverables and the acceptance checks that were wired into
the test suite or CI.

Gate 0 – Foundation & Build
---------------------------

* **Objective:** Initialise the project structure and toolchain presets.
* **Deliverables:** CMake presets for GCC/Clang/MSVC, warning flags
  (``-Wall -Wextra -Werror``), baseline CI workflow, Doxygen/Sphinx skeleton.
* **Gate:** CI must build on Linux, macOS and Windows (GitHub Actions matrix)
  and the documentation target must succeed.

Gate 1 – Core Types & Utilities
-------------------------------

* **Deliverables:** ``mb_types.h`` (fixed width aliases), ``mb_err.h``, logging
  macros, ring buffer (SPSC), CRC16 helper.
* **Gate:** 100 % unit-test coverage for utils (`ctest` target) and clean runs
  under ASan/UBSan (``cmake --preset host-asan``).

Gate 2 – PDU Codec
------------------

* **Deliverables:** Encode/decode for FC 03/06/16 plus exception helpers and
  payload limit checks (≤ 253 bytes).
* **Gate:** Table-driven unit tests (good/bad vectors) with ≥ 90 % coverage and
  5 min fuzz run without crashes (``tests/fuzz`` harness).

Gate 3 – Transport Abstraction
------------------------------

* **Deliverables:** ``mb_transport_if_t`` (non-blocking send/recv), frame helper
  ``mb_frame_rtu_*``.
* **Gate:** Transport mock + frame helpers exercised in integration tests
  (`test_transport_if`, `test_modbus_core`).

Gate 4 – RTU Minimum
--------------------

* **Deliverables:** Stateful RTU parser with inter-byte silence detection and
  transmit helper.
* **Gate:** Loopback tests (mock/UART simulator) covering echo, byte loss and
  duplication (`test_rtu_transport`).

Gate 5 – Client FSM
-------------------

* **Deliverables:** Non-blocking transaction manager (Idle→Sending→Waiting→
  Done/Timeout/Retry/Abort), exponential backoff with jitter, cancellation
  support.
* **Gate:** Stress test with 1 000 sequential requests and simulated packet
  loss (``MbClientTest`` suite); watchdog remains idle in nominal cases.

Gate 6 – Server Core
--------------------

* **Deliverables:** Register-region mapping with callbacks/storage, FC 03/06/16
  handling, exception validation.
* **Gate:** Client↔server integration (loopback) covering read/write/multi-write
  and malformed frames (`test_client_server_integration`, `test_modbus_server`).

Gate 7 – TCP / MBAP
-------------------

* **Deliverables:** MBAP framing, TID ↔ transaction mapping, optional
  multi-connection helper (`mb_tcp_multi_*`), TCP CLI example.
* **Gate:** CI job spins up a Python Modbus TCP server and validates behaviour
  using the library CLI plus ``mbtget``/``modpoll``; throughput mock processes
  ≥ 1 000 responses (`test_tcp_throughput`).

Future gates (13+) will focus on release hardening (sanitizers, MISRA profiles),
security posture and long-term maintenance. Refer to ``update_plan.md`` for the
full roadmap.

Gate 8 – Robustness & Backpressure
----------------------------------

* **Deliverables:** Queue backpressure (configurable capacity), poison-pill
  drains, per-function timeout overrides, latency/response metrics for both
  client and server.
* **Gate:** Chaos tests introduce bursty traffic, latency and packet loss;
  the metrics counters capture drops/timeouts and no starvation occurs.

Gate 9 – Port/HAL Adapters
--------------------------

* **Deliverables:** POSIX socket wrapper, FreeRTOS stream/queue adapter and
  bare-metal transport helper (non-blocking UART/SPI integrations), plus a
  portable mutex shim.
* **Gate:** POSIX and FreeRTOS examples compile/run, bare-metal adapter passes
  hardware-in-the-loop smoke tests.

Gate 10 – Extended Function Codes + ASCII
-----------------------------------------

* **Deliverables:** Helpers for FC 01/02/04/05/0F/17 alongside validation and
  regression tests, plus Modbus ASCII framing with LRC validation.
* **Gate:** Unit/integration tests cover the additional PDU helpers and ASCII
  transport, ensuring malformed requests raise the correct exceptions.

Gate 11 – Observability & Debug
-------------------------------

* **Deliverables:** Structured events (state enter/leave, transaction lifecycle),
  opt-in hex tracing, diagnostics counters grouped by function code and error
  bucket (``mb_diag_*`` API).
* **Gate:** Integration tests capture events while inducing failures, and diag
  counters reflect errors/timeouts accurately.

Gate 12 – Documentation & API Stability
---------------------------------------

* **Deliverables:** Ports guide, migration notes, client/server cookbook,
  Sphinx/Doxygen refresh and SemVer 1.0.0 declaration (stable umbrella header
  ``<modbus/modbus.h>``).
* **Gate:** Documentation builds without warnings, examples compile, and the
  API surface is frozen under SemVer guarantees.

Gate 13 – Hardening & Release
-----------------------------

* **Deliverables:** clang-tidy security profile, MISRA-C checklist, compiler
  hardening flags (``-fno-common``, ``-fstack-protector-strong``,
  ``_FORTIFY_SOURCE=2``) and ELF RELRO/Now when available.
* **Gate:** CI runs sanitizers, fuzzers, coverage and linting; release tags are
  cut from green pipelines.
