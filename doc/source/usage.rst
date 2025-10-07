Usage Guide
===========

.. note::

    This walkthrough focuses on the host-side API and currently available
    helpers.  Embedded quickstarts are published under ``embedded/quickstarts``:
    the drop-in translation unit, an ESP-IDF component with UART transport, and
    a Zephyr module with a sockets-based Modbus/TCP bridge.  Keep an eye on
    :doc:`install_use` for additional examples (bare-metal loops, vendor HALs)
    as they land.

Initialisation
--------------

1. Include the umbrella header ``<modbus/modbus.h>`` (or the specific headers you need).
2. Set up a transport interface (RTU or TCP) implementing :ref:`mb_transport_if_t`.
3. Initialise client or server with the respective init call.
4. Register register regions (server) or prepare transaction pool (client).
5. Poll the state machine regularly (``mb_client_poll`` / ``mb_server_poll``).

Client Example (RTU)
--------------------

.. code-block:: c

   mb_client_t client;
   mb_client_txn_t tx_pool[4];
   mb_transport_if_t iface = make_rtu_iface(...);

   mb_client_init(&client, &iface, tx_pool, MB_COUNTOF(tx_pool));

   mb_client_request_t req = {
       .request.unit_id = 0x11U,
       .request.function = MB_PDU_FC_READ_HOLDING_REGISTERS,
       .request.payload = pdu_buffer,
       .request.payload_len = pdu_len,
       .timeout_ms = 100U,
       .max_retries = 2U,
       .callback = on_complete,
       .user_ctx = &user_ctx,
   };
   mb_client_submit(&client, &req, NULL);

   while (!done) {
       mb_client_poll(&client);
       do_other_work();
   }

Server Example
--------------

.. code-block:: c

   mb_server_t server;
   mb_server_region_t regions[4];
   mb_server_request_t requests[4];
   mb_transport_if_t iface = make_rtu_iface(...);
   uint16_t holding_regs[128] = {0};

   mb_server_init(&server,
                  &iface,
                  0x11U,
                  regions,
                  MB_COUNTOF(regions),
                  requests,
                  MB_COUNTOF(requests));
   mb_server_add_storage(&server, 0x0000U, MB_COUNTOF(holding_regs), false, holding_regs);

   for (;;) {
       mb_server_poll(&server);
       do_other_work();
   }

Runtime tuning
--------------

Gate 8 introduces explicit backpressure and telemetry controls so the server can
behave deterministically under bursty traffic:

.. code-block:: c

   /* Limit the number of in-flight requests (defaults to pool size). */
   mb_server_set_queue_capacity(&server, 8U);

   /* Override the default per-function timeout for slow handlers. */
   mb_server_set_fc_timeout(&server,
                            MB_PDU_FC_READ_HOLDING_REGISTERS,
                            500U /* ms */);

   /* Optional: drop everything still queued (e.g., on shutdown). */
   mb_server_submit_poison(&server);

   /* Periodically collect metrics for diagnostics. */
   mb_server_metrics_t metrics;
   mb_server_get_metrics(&server, &metrics);
   printf("responses=%llu dropped=%llu timeouts=%llu\n",
          (unsigned long long)metrics.responded,
          (unsigned long long)metrics.dropped,
          (unsigned long long)metrics.timeouts);

Use :func:`mb_server_pending` / :func:`mb_server_is_idle` to check whether all
requests have been drained, e.g. before entering low-power modes.  The helper
:func:`mb_server_inject_adu` is provided primarily for tests and simulations
where the transport path is bypassed.

Cooperative polling budgets
---------------------------

Gate 19 layered a micro-step scheduler on top of the existing poll loops.  The
new :func:`mb_client_poll_with_budget` and :func:`mb_server_poll_with_budget`
variants take an explicit ``steps`` parameter that caps how many FSM micro-steps
are executed per call.  Passing ``0`` keeps the legacy behaviour (run until the
machine idles), while any non-zero value bounds the work carried out during that
tick.

.. code-block:: c

   while (app_running) {
       mb_client_poll_with_budget(&client, 2U);
       mb_server_poll_with_budget(&server, 1U);
       do_other_work();
   }

Both FSMs enforce per-substate deadlines governed by
``MB_CONF_CLIENT_SUBSTATE_DEADLINE_MS`` and ``MB_CONF_SERVER_SUBSTATE_DEADLINE_MS``;
expiring a deadline triggers a deterministic retry/exception path rather than
wedging the scheduler.  Compile-time defaults for the budget can be provided via
``MB_CONF_CLIENT_POLL_BUDGET_STEPS`` / ``MB_CONF_SERVER_POLL_BUDGET_STEPS`` so
call sites can stick with the short form (`mb_client_poll` / `mb_server_poll`) if
desired.

Telemetry: the :c:type:`mb_client_metrics_t` and :c:type:`mb_server_metrics_t`
structs now include ``step_max_jitter_ms`` and ``step_avg_jitter_ms`` fields.  To
instrument individual phases, define ``MB_CONF_CLIENT_POLL_HOOK`` or
``MB_CONF_SERVER_POLL_HOOK`` and collect statistics or toggle GPIOs around each
micro-step without leaving the cooperative model.

POSIX transport helper
----------------------

Gate 9 introduces the first transport/HAL helper. On POSIX platforms a thin
wrapper exposes a ready-to-use :c:type:`mb_transport_if_t` from a socket
descriptor:

.. code-block:: c

   mb_port_posix_socket_t tcp;
   if (mb_port_posix_tcp_client(&tcp, "192.0.2.10", 502, 1000U) != MB_OK) {
       /* handle connect error */
   }

   const mb_transport_if_t *iface = mb_port_posix_socket_iface(&tcp);
   mb_client_t client;
   mb_client_txn_t tx_pool[4];
   mb_client_init_tcp(&client, iface, tx_pool, MB_COUNTOF(tx_pool));

   /* ... issue requests ... */

   mb_port_posix_socket_close(&tcp);

FreeRTOS transport helper
-------------------------

Gate 9 also ships a FreeRTOS adapter that bridges stream buffers or queues to
an :c:type:`mb_transport_if_t`.  Wire the low-level primitives and the Modbus
client/server gains a non-blocking interface that cooperates with the scheduler:

.. code-block:: c

   mb_port_freertos_transport_t port;
   mb_port_freertos_transport_init(&port,
                                   tx_stream,
                                   rx_stream,
                                   xStreamBufferSend,
                                   xStreamBufferReceive,
                                   xTaskGetTickCount,
                                   taskYIELD,
                                   configTICK_RATE_HZ,
                                   pdMS_TO_TICKS(10));

   const mb_transport_if_t *iface = mb_port_freertos_transport_iface(&port);
   mb_client_init(&client, iface, tx_pool, MB_COUNTOF(tx_pool));

Bare-metal helper
-----------------

For MCUs without an RTOS use :c:type:`mb_port_bare_transport_t` to adapt your
HAL callbacks.  Provide non-blocking send/receive hooks plus a monotonic tick
source and the library handles the rest:

.. code-block:: c

   static mb_err_t uart_send(void *ctx,
                             const mb_u8 *buf,
                             mb_size_t len,
                             mb_transport_io_result_t *out)
   {
       /* drive DMA/interrupt UART; populate out->bytes */
   }

   static mb_err_t uart_recv(void *ctx,
                             mb_u8 *buf,
                             mb_size_t cap,
                             mb_transport_io_result_t *out)
   {
       /* peek RX FIFO; set out->status to MB_TRANSPORT_IO_AGAIN when empty */
   }

   static uint32_t ticks_now(void *ctx)
   {
       return timer_ticks();
   }

   mb_port_bare_transport_t port;
   mb_port_bare_transport_init(&port,
                               &uart_handle,
                               uart_send,
                               uart_recv,
                               ticks_now,
                               1000U, /* 1 kHz tick */
                               cpu_yield,
                               NULL);

   const mb_transport_if_t *iface = mb_port_bare_transport_iface(&port);
   mb_server_init(&server,
                  iface,
                  0x11U,
                  regions,
                  MB_COUNTOF(regions),
                  requests,
                  MB_COUNTOF(requests));

Observability hooks
-------------------

Gate 11 wires structured observability into both the client and the server. The
new ``modbus/observe.h`` header exposes three pillars:

``mb_diag_*`` counters
    Per-function and per-error histograms that can be sampled/reset at runtime.
    Useful for feeding watchdog dashboards.

Event callbacks
    ``mb_client_set_event_callback`` / ``mb_server_set_event_callback`` surface
    state transitions (enter/exit) and transaction lifecycle events. The payload
    carries function code, status and timestamps so you can pipe them to tracing
    backends.

Hex tracing
    ``mb_client_set_trace_hex`` / ``mb_server_set_trace_hex`` dump RTU/TCP PDUs
    through :c:macro:`MB_LOG_DEBUG` when logging is enabled – handy during
    bring-up and while diagnosing protocol mismatches.

Typical usage:

.. code-block:: c

   static void my_event_sink(const mb_event_t *evt, void *user)
   {
       (void)user;
       if (evt->source == MB_EVENT_SOURCE_CLIENT &&
           evt->type == MB_EVENT_CLIENT_TX_COMPLETE) {
           printf("tx done fc=%u status=%d\n",
                  evt->data.client_txn.function,
                  evt->data.client_txn.status);
       }
   }

   mb_diag_counters_t diag;
   mb_client_get_diag(client, &diag);
   printf("fc03=%llu timeouts=%llu\n",
          (unsigned long long)diag.function[MB_PDU_FC_READ_HOLDING_REGISTERS],
          (unsigned long long)diag.error[MB_DIAG_ERR_SLOT_TIMEOUT]);
   mb_client_reset_diag(client);

   mb_client_set_event_callback(client, my_event_sink, NULL);
   mb_client_set_trace_hex(client, true);

The same APIs exist for :c:type:`mb_server_t`. All hooks are optional and can be
compiled out simply by not enabling logging or not installing callbacks.

Advanced Features
-----------------

* TCP: use ``mb_client_init_tcp`` + ``mb_tcp_transport``.
* Multiple connections: ``mb_tcp_multi_*`` helpers.
* Timeouts/retries: configured per transaction.
* Watchdog: ``mb_client_set_watchdog``.

Refer to :doc:`api` for exhaustive definitions.

For more complete end-to-end walkthroughs visit the :doc:`cookbook`, and when
upgrading existing deployments consult the :doc:`migration` guide.
