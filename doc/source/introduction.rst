Introduction
============

This documentation describes the current generation of the Modbus C library.
The focus is on an embedded-friendly, non-blocking implementation targeting
Modbus RTU and Modbus TCP transports with a clean separation between the PDU
codec, transport interfaces, and the client/server state machines.

**Key design goals**

* Pure C11 (with C99 fallback).
* No dynamic allocation by default; queue and buffer pools are fixed at init.
* Non-blocking APIs suitable for cooperative schedulers.
* Strong validation and UB-safe code paths (bounds checks, sanitizer friendly).
* First-class observability (log hooks, eventual metrics) and testability
  (unit + integration + fuzzing).

This guide reflects the API introduced in Gates 5–7 (client FSM, server core,
RTU/TCP transports). Older examples and legacy headers were removed to keep the
focus on the supported surface.

See :doc:`usage` for a practical walk-through and :doc:`api` for complete API
reference.
