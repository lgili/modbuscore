Overview
========

The library is organised around three layers:

Non-blocking state machines
    ``mb_client_t`` and ``mb_server_t`` implement polling finite-state machines
    for Modbus RTU and Modbus TCP.  They manage retry policies, watchdog
    deadlines and transaction queues without allocating memory at runtime.

Transport adapters
    All I/O is funnelled through :c:type:`mb_transport_if_t`, a minimal set of
    callbacks for ``send``, ``recv``, ``now`` and (optionally) ``yield``.  The
    core ships adapters for POSIX sockets/serial (:doc:`transports`), FreeRTOS
    stream buffers and bare-metal ISR-driven UARTs.

Shared infrastructure
    Observability (:doc:`diagnostics`), power management hooks
    (:doc:`power_management`), queue/priority utilities, and compile-time
    configuration (:doc:`configuration`) are common to client and server.

Feature Gates
-------------

The project roadmap is captured as “gates”, incremental milestones that can be
enabled independently.  Gates 17–32 cover higher-level capabilities such as
transport ports, observability, low-power operation and RTOS integrations.  The
table below lists the most relevant gates that are available in this release.

.. list-table:: Gate summary (selected milestones)
   :widths: 12 60 12
   :header-rows: 1

   * - Gate
     - Focus
     - Status
   * - 17
     - Extended PDU helpers (FC 01/02/04/05/0F/10/17) and bounds checking
     - ✅
   * - 18
     - Robust queue management: priorities, poison pill, per-function timeouts
     - ✅
   * - 19
     - Pool-backed server transaction table with broadcast awareness
     - ✅
   * - 20
     - POSIX port (TCP + serial) built on :c:type:`mb_transport_if_t`
     - ✅
   * - 21
     - ISR-friendly UART ingest using :c:type:`mb_ringbuf_t`
     - ✅
   * - 22
     - Unified diagnostics counters and ``mb_err_str`` mapping
     - ✅
   * - 23
     - Observability events, hex tracing and metrics API for client & server
     - ✅
   * - 24
     - Power-off resilience: watchdog escalation, queue draining policies
     - ✅
   * - 27
     - Power-management/idle hints for RTOS and bare-metal targets
     - ✅
   * - 28
     - Structured telemetry export (events + diagnostics)
     - ✅
   * - 29
     - Developer tooling: queue inspection, poll jitter tracking
     - ✅
   * - 30
     - Zephyr integration examples (RTU client/server)
     - ✅
   * - 31
     - FreeRTOS transport helpers and reference applications
     - ✅
   * - 32
     - Benchmarks, interop fixtures and automated validation
     - ✅

Design Principles
-----------------

* **Deterministic execution** – all operations are non-blocking and bounded by
  the configured queue size.  Timeouts, retries and watchdogs are expressed in
  milliseconds and do not rely on dynamic allocation.
* **Portability first** – the core is ISO C11 and can fall back to C99.  Backend
  code is isolated in ``modbus/port`` and ``modbus/transport``.
* **Compile-time feature selection** – every optional feature is guarded by an
  ``MB_CONF_*`` macro.  Applications can disable unused function codes, transports
  or diagnostics to minimise footprint.
* **Observability as a first-class citizen** – metrics, diagnostics counters, hex
  tracing and structured events are available on both client and server.
* **Test-driven** – GoogleTest suites cover the state machines, transports and
  ports.  The ``interop`` directory holds fixtures exercised in CI against
  reference Modbus stacks.

The remaining sections dive into practical usage, configuration and extension
points.
