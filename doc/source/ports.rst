Port & HAL Guide
================

This guide centralises the transport adapters introduced in Gate 9 so you can
wire the library into different environments without reimplementing the
:ref:`mb_transport_if_t` contract.

.. contents:: Sections
   :local:

POSIX socket adapter
--------------------

``mb_port_posix_socket_*`` wraps TCP sockets, handling connect/accept, I/O and
non-blocking behaviour. Typical flow:

#. Call :func:`mb_port_posix_tcp_client` or :func:`mb_port_posix_tcp_server`
   to establish the endpoint.
#. Retrieve the adapter using :func:`mb_port_posix_socket_iface` and feed it to
   :func:`mb_client_init` / :func:`mb_server_init`.
#. Periodically call :func:`mb_port_posix_socket_poll` if you enabled optional
   keepalive features.
#. Close with :func:`mb_port_posix_socket_close`.

The adapter internally maps POSIX error codes to ``mb_err_t`` so Modbus state
machines can react deterministically.

FreeRTOS transport adapter
--------------------------

``mb_port_freertos_transport_init`` bridges stream buffers (or queues) to the
Modbus FSM. Provide the send/receive function pointers plus a tick source and
optional yield hook. The helper keeps track of the RTOS tick period so
watchdogs and timeouts continue to operate in milliseconds.

Bare-metal adapter
------------------

``mb_port_bare_transport_t`` is designed for MCUs without an operating system.
You supply minimal callbacks:

* ``send`` – kick DMA/interrupt based UART/TCP peripherals.
* ``recv`` – poll the RX FIFO and mark ``MB_TRANSPORT_IO_AGAIN`` when empty.
* ``now`` – return a millisecond tick from a hardware timer.
* ``yield`` – optional; can idle the CPU or feed a watchdog.

The adapter offers compile-time configuration to keep everything in `.bss`
when ``MB_TRANSPORT_IF_STATIC`` is defined.

Troubleshooting tips
--------------------

* Ensure the tick source monotonicity: transport timeouts rely on it heavily.
* On FreeRTOS keep ``configUSE_16_BIT_TICKS`` disabled; use 32-bit ticks for
  longer watchdog windows.
* For low-power MCUs consider gating UART clocks while the poll loop is idle –
  the state machines recover on the next activity.

See also :doc:`usage` for code snippets that exercise each adapter and
:doc:`cookbook` for end-to-end recipes.
