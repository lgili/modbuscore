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

Windows socket adapter
----------------------

``mb_port_win_socket_*`` mirrors the POSIX helper while targeting Winsock 2. It
bootstraps the underlying subsystem (via
:func:`mb_port_win_socket_global_init`), installs non-blocking mode through
``ioctlsocket`` and surfaces :ref:`mb_transport_if_t` compatible callbacks. The
workflow is identical to the POSIX variant:

#. Call :func:`mb_port_win_tcp_client` (or wrap an accepted socket with
   :func:`mb_port_win_socket_init`).
#. Retrieve the transport using :func:`mb_port_win_socket_iface` and feed it to
   :func:`mb_client_init` / :func:`mb_server_init`.
#. Tear down with :func:`mb_port_win_socket_close` and release the reference to
   Winsock via :func:`mb_port_win_socket_global_cleanup` when you are done.

The helper adopts the same ``mb_err_t`` mappings, so your Modbus FSM reacts the
same way on macOS/Linux and Windows.

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

STM32 LL DMA + IDLE reference
-----------------------------

Gate 20 introduces a ready-to-use reference that glues the Modbus RTU client to
STM32 UART peripherals configured with circular DMA reception and IDLE-line
interrupts.  The drop-in implementation lives under
``embedded/quickstarts/ports/stm32-ll-dma-idle`` with a dedicated header
(``modbus_stm32_idle.h``) and is accompanied by detailed hardware notes in
``embedded/ports/stm32-ll-dma-idle/README.md``.

Highlights:

* Zero-copy consumption of the DMA buffer by tracking producer/consumer
   indices.
* ISR hooks that reconcile the UART IDLE flag and DMA transfer-complete events
   to mark frame boundaries reliably.
* Microsecond timing callbacks so the helper can enforce T1.5/T3.5 guard times
   without busy-waiting longer than necessary.
* Configuration fields covering baud rate, framing (data/parity/stop bits) and
  optional guard overrides so deployments can derive compliant silence periods
  directly from firmware settings.

Pair the reference with the :doc:`rtu_timing` tables when adjusting silence
timeouts for alternative baud/parity combinations.

Troubleshooting tips
--------------------

* Ensure the tick source monotonicity: transport timeouts rely on it heavily.
* On FreeRTOS keep ``configUSE_16_BIT_TICKS`` disabled; use 32-bit ticks for
  longer watchdog windows.
* For low-power MCUs consider gating UART clocks while the poll loop is idle –
  the state machines recover on the next activity.

See also :doc:`usage` for code snippets that exercise each adapter and
:doc:`cookbook` for end-to-end recipes.
