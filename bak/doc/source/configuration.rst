Configuration Reference
=======================

The library is configured entirely at compile time through ``MB_CONF_*``
macros.  This section catalogues the most relevant options and explains how they
map to the Gate feature set.

Quick Checklist
---------------

1. **Select a profile** :: ``MB_CONF_PROFILE`` (``TINY``, ``LEAN`` or ``FULL``)
2. **Choose transports** :: enable/disable RTU, ASCII, TCP
3. **Enable roles** :: build client, server, or both
4. **Tweak function codes** :: FC01–FC17 can be individually turned off
5. **Enable diagnostics & power management** as needed

Profiles
--------

.. list-table:: Default settings by profile
   :widths: 18 22 22 38
   :header-rows: 1

   * - Symbol
     - Tiny
     - Lean *(default)*
     - Full
   * - ``MB_CONF_BUILD_CLIENT``
     - ``1``
     - ``1``
     - ``1``
   * - ``MB_CONF_BUILD_SERVER``
     - ``0``
     - ``1``
     - ``1``
   * - ``MB_CONF_TRANSPORT_TCP``
     - ``0``
     - ``1``
     - ``1``
   * - ``MB_CONF_TRANSPORT_ASCII``
     - ``0``
     - ``0``
     - ``1``
   * - ``MB_CONF_ENABLE_FC0F`` / ``FC10`` / ``FC17``
     - ``0``
     - ``1``
     - ``1``
   * - Diagnostics / Power
     - ``0``
     - ``1``
     - ``1``

Set ``MB_CONF_PROFILE`` before including any Modbus headers.  You can override
individual flags after the profile definition to fine-tune footprint.

Core Options
------------

* ``MB_CONF_BUILD_CLIENT`` – compile the client FSM and related helpers.
* ``MB_CONF_BUILD_SERVER`` – compile the server FSM.
* ``MB_CONF_TRANSPORT_RTU`` / ``MB_CONF_TRANSPORT_ASCII`` /
  ``MB_CONF_TRANSPORT_TCP`` – include RTU/ASCII/TCP framing code respectively.
* ``MB_CONF_ENABLE_FCxx`` – enable specific function codes (read coils, holding
  registers, write multiple, etc.).  Disable unused FCs to reduce code size.

Diagnostics & Observability
---------------------------

* ``MB_CONF_DIAG_ENABLE_COUNTERS`` – enable :c:type:`mb_diag_counters_t` and the
  ``mb_client_get_diag`` / ``mb_server_get_diag`` APIs (Gate 22/28/29).
* ``MB_CONF_DIAG_ENABLE_TRACE`` – compile hex trace helpers for client/server.
* ``MB_CONF_DIAG_TRACE_DEPTH`` – maximum number of bytes captured per frame when
  tracing is enabled.
* ``MB_CONF_ENABLE_IOVEC_STATS`` – track memcpy vs zero-copy operations when
  using scatter/gather buffers.

Power Management (Gate 27)
--------------------------

* ``MB_CONF_ENABLE_POWER_MANAGEMENT`` – enable idle detection, poll jitter
  accounting and low-power callbacks.  Adds ~64 bytes per instance.
* ``MB_CONF_IDLE_DEFAULT_THRESHOLD_MS`` *(see ``mb_power.h``)* – timeout before
  triggering the idle callback.

Transport Ports
---------------

* ``MB_CONF_PORT_POSIX`` – compile the POSIX helpers
  (``mb_port_posix_socket_*``, ``mb_port_posix_serial_open``).
* ``MB_CONF_PORT_FREERTOS`` – include FreeRTOS transport glue.
* ``MB_CONF_PORT_BARE`` – include bare-metal ISR-friendly transport helpers.
* ``MB_CONF_PORT_MUTEX`` – provide lightweight mutex wrappers used by the queue
  backpressure logic on platforms without native primitives.

Build-Time Overrides
--------------------

Use conventional CMake defines to override configuration:

.. code-block:: bash

   cmake --preset host-debug \
     -DMB_CONF_PROFILE=MB_CONF_PROFILE_TINY \
     -DMB_CONF_TRANSPORT_TCP=0 \
     -DMB_CONF_DIAG_ENABLE_COUNTERS=0

For ad-hoc edits these macro definitions can also be set before including
``modbus/modbus.h`` in a translation unit.

Gate Coverage Map
-----------------

The table below highlights the compile-time switches most relevant to Gates 17–32.

.. list-table:: Gate to configuration mapping
   :widths: 12 26 60
   :header-rows: 1

   * - Gate
     - Macro(s)
     - Notes
   * - 17
     - ``MB_CONF_ENABLE_FC01`` … ``MB_CONF_ENABLE_FC17``
     - Disable unused FCs to reduce code size and validation tables.
   * - 18
     - ``MB_CONF_PORT_MUTEX``
     - Enables queue backpressure mutex adapters for RTOS/posix.
   * - 20/21
     - ``MB_CONF_PORT_POSIX``, ``MB_CONF_PORT_BARE``
     - Build transport helpers for POSIX sockets/serial and ISR pipelines.
   * - 22/28/29
     - ``MB_CONF_DIAG_ENABLE_COUNTERS``, ``MB_CONF_DIAG_ENABLE_TRACE``
     - Diagnostics counters, events and trace hex.
   * - 24
     - ``MB_CONF_CLIENT_QUEUE_DEPTH`` *(via profile)*
     - Configure queue size and retry limits to fit power-loss policies.
   * - 27
     - ``MB_CONF_ENABLE_POWER_MANAGEMENT``
     - Idle detection and low-power callbacks.
   * - 30/31
     - ``MB_CONF_PORT_ZEPHYR`` *(if available)*, ``MB_CONF_PORT_FREERTOS``
     - Build Zephyr and FreeRTOS adapters and reference apps.
   * - 32
     - ``MB_CONF_ENABLE_IOVEC_STATS`` / coverage options
     - Enables benchmarking/telemetry helpers used in the performance suite.

Memory Footprint Tips
---------------------

* Adjust transaction pool length to the smallest viable size.  The queue is a
  compile-time array of :c:type:`mb_client_txn_t` / :c:type:`mb_server_request_t`.
* Disable transports not needed in production (e.g., omit TCP on MCU targets).
* For RTU-only builds you can exclude ASCII and TCP entirely by setting the
  corresponding macros to ``0``.
* If power management or diagnostics are not required, set the respective flags
  to ``0`` to remove the associated bookkeeping.

Refer to the :doc:`api` section for detailed structure layouts and field-level
descriptions.
