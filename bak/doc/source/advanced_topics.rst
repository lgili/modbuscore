Advanced Topics
===============

RTU Timing
----------

Precise RTU timing is essential for interoperability.  The helper
``mb_rtu_silence_deadline`` computes the 3.5 byte silence interval based on the
configured baudrate.  For high baudrates (>19200) the Modbus specification uses
a fixed 1.75 ms window.

Recommendations:

* Ensure ``mb_transport_now`` has microsecond or millisecond resolution.
* Call ``mb_rtu_poll`` at least every 500 µs at 115200 baud to maintain the
  silence window.
* Use ``mb_port_posix_serial_open`` or the FreeRTOS/bare adapters which already
  configure non-blocking reads and flush the RX FIFO before initialisation.

Zero-Copy I/O
-------------

For systems where copying data is expensive you can build the library with
``MB_CONF_ENABLE_IOVEC_STATS=1`` and provide zero-copy buffers to the transport.
The client/server transaction structures expose ``request_view`` and
``response_view`` pointing to scratch buffers.  When the transport natively
supports scatter/gather I/O these pointers can be redirected to DMA regions.

* ``mb_client_txn_t.request_view`` – modify before calling ``mb_client_submit``
  if you want to supply external storage.
* ``mb_client_txn_t.response_view`` – inspect after completion.

The benchmark suite in ``benchmarks/`` compares memcpy-heavy and zero-copy paths.

Concurrency & Mutexes
---------------------

The library itself is single-threaded but safely callable from multiple tasks
when the application serialises access to ``mb_client_poll`` /
``mb_server_poll``.  ``MB_CONF_PORT_MUTEX`` activates lightweight mutex hooks in
the queue backpressure logic.  Alternatively, wrap Modbus calls inside a task or
event loop dedicated to protocol handling.

MISRA Compliance (Gate 23/24)
-----------------------------

The codebase follows a MISRA-C 2012 inspired style.  Deviations are tracked in
``docs/misra.rst``.  When building with ``-DMODBUS_ENABLE_MISRA=ON`` the build
system runs ``clang-tidy`` with the MISRA profile and enables extra warnings.

Embedding in Bootloaders
------------------------

Gate 24 introduced watchdog escalation logic.  When a transaction exceeds the
configured deadlines the FSM escalates to ``MB_ERR_TIMEOUT`` and optionally
notifies the power management layer.  In bootloaders you can set very short
timeouts and disable retransmission to minimise latency before entering firmware
update modes.

Extending Function Codes
------------------------

Adding custom function codes requires updating the PDU builder/parser tables and
enabling a new ``MB_CONF_ENABLE_FCxx`` flag.  Follow the pattern used by FC17:

1. Implement encoder/decoder helpers in ``modbus/src/pdu.c``.
2. Update bounds tables in ``modbus/src/pdu_limits.c``.
3. Expose the public API in ``modbus/include/modbus/pdu.h``.

The observability counters automatically start tracking the new function code as
soon as ``mb_diag_record_fc`` is called.
