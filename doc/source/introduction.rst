Introduction
============

This documentation describes the **Modbus C library 1.0.0**. The focus is on
an embedded-friendly, non-blocking implementation targeting Modbus RTU and
Modbus TCP transports with a clean separation between the PDU codec, transport
interfaces, and the client/server state machines. Gate 12 ratifies the public
API surface for long-term support: applications that include ``<modbus/modbus.h>``
can rely on SemVer guarantees going forward.

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
