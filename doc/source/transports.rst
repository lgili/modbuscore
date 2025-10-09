Transports & Ports
==================

All Modbus I/O flows through :c:type:`mb_transport_if_t`.  Clients and servers
only require the following callbacks:

* ``send(void *ctx, const uint8_t *buf, size_t len, mb_transport_io_result_t *out)``
* ``recv(void *ctx, uint8_t *buf, size_t cap, mb_transport_io_result_t *out)``
* ``now(void *ctx)`` returning milliseconds since boot (monotonic)
* ``yield(void *ctx)`` *(optional)* – cooperative hint for RTOS/host platforms

``mb_transport_io_result_t`` carries the number of bytes processed, allowing the
callbacks to return ``MB_ERR_TIMEOUT`` when no data is available without
blocking the FSM.

POSIX Helper (Gate 20)
----------------------

``modbus/port/posix.h`` exposes two convenience wrappers:

* ``mb_port_posix_tcp_client`` – connect to a TCP endpoint and obtain a transport.
* ``mb_port_posix_serial_open`` – configure a serial device (termios) for RTU.

.. code-block:: c

   mb_port_posix_socket_t tcp;
   MB_ASSERT_OK(mb_port_posix_tcp_client(&tcp, host, port, 1000));
   const mb_transport_if_t *iface = mb_port_posix_socket_iface(&tcp);

   mb_client_t client;
   mb_client_txn_t pool[4];
   MB_ASSERT_OK(mb_client_init_tcp(&client, iface, pool, MB_COUNTOF(pool)));

The helpers make sockets non-blocking, provide ``now`` using ``clock_gettime``
and implement ``yield`` with ``sched_yield``.  Serial support (``mb_port_posix_serial_open``)
accepts baudrate, parity, data bits and stop bits.

FreeRTOS Transport (Gate 31)
----------------------------

``mb_port_freertos_transport_init`` bridges FreeRTOS stream buffers or queues to
``mb_transport_if_t``.  Provide function pointers to ``xStreamBufferSend`` /
``xStreamBufferReceive`` equivalents plus the tick getter (``xTaskGetTickCount``):

.. code-block:: c

   mb_port_freertos_transport_t port;
   MB_ASSERT_OK(mb_port_freertos_transport_init(
       &port,
       tx_stream,
       rx_stream,
       xStreamBufferSend,
       xStreamBufferReceive,
       xTaskGetTickCount,
       vPortYield,
       configTICK_RATE_HZ,
       pdMS_TO_TICKS(50)));

   const mb_transport_if_t *iface = mb_port_freertos_transport_iface(&port);

You can adjust the maximum block time at runtime using
``mb_port_freertos_transport_set_block_ticks``.

Bare-metal ISR Path (Gate 21)
-----------------------------

The bare-metal adapter uses ``mb_ringbuf_t`` (single-producer/single-consumer)
buffers to decouple ISR byte reception from the polling FSM.  Typical usage:

.. code-block:: c

   static uint8_t rx_storage[128];
   static mb_ringbuf_t rx_ring;
   mb_ringbuf_init(&rx_ring, rx_storage, sizeof(rx_storage));

   mb_port_bare_transport_t port;
   MB_ASSERT_OK(mb_port_bare_transport_init(
       &port,
       /* user ctx */ NULL,
       uart_send_bytes,
       uart_recv_bytes_from_ring,
       systick_now_ticks,
       tick_rate_hz,
       uart_yield,
       /* clock ctx */ NULL));

   const mb_transport_if_t *iface = mb_port_bare_transport_iface(&port);

Within the UART RX ISR call ``mb_ringbuf_push`` to feed the ring.  The polling
side retrieves bytes through the ``recv_fn`` registered during initialisation.

Transport Selection Checklist
-----------------------------

1. Choose the appropriate helper or implement :c:type:`mb_transport_if_t` yourself.
2. Ensure ``now`` uses a monotonic clock – never a wall clock.
3. Do not block inside ``send``/``recv``.  Return ``MB_ERR_TIMEOUT`` if the
   operation would block.
4. For RTU/ASCII transports, call ``mb_rtu_poll``/``mb_ascii_poll`` as part of
   your main loop.
5. When sending from multiple tasks, protect the transport with a mutex
   (``MB_CONF_PORT_MUTEX``) or serialize access at a higher layer.

ASCII Transport
---------------

The ASCII framing layer is optional (``MB_CONF_TRANSPORT_ASCII``).  When enabled,
use ``mb_ascii_transport_t`` similarly to the RTU helpers.  Because ASCII is less
common in modern deployments the library keeps the feature behind a compile-time
switch to avoid code size bloat.

Custom Transports
-----------------

For specialised hardware (DMA UARTs, CAN gateways, proprietary links) reuse the
utilities provided in ``modbus/transport``:

* ``mb_transport_bind_legacy`` – adapt legacy ``modbus_transport_t`` shims to the
  new interface.
* ``mb_transport_elapsed_since`` – convenience helper to compute deadlines from
  the monotonic clock.

Ensure your callbacks never call back into the Modbus FSM synchronously (to
avoid recursion) and honour the non-blocking contract.
