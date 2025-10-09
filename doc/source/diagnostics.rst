Diagnostics & Observability
===========================

Metrics
-------

Both client and server expose cumulative metrics:

.. code-block:: c

   mb_client_metrics_t metrics;
   mb_client_get_metrics(&client, &metrics);

   printf("completed=%llu timeouts=%llu bytes_tx=%llu\n",
          (unsigned long long)metrics.completed,
          (unsigned long long)metrics.timeouts,
          (unsigned long long)metrics.bytes_tx);

``mb_client_reset_metrics`` and ``mb_server_reset_metrics`` clear the counters.
Latency aggregation (``response_latency_total_ms``) combined with the completed
count provides average latency measurements.

Diagnostics Counters (Gate 22)
------------------------------

Enable ``MB_CONF_DIAG_ENABLE_COUNTERS`` to collect per-function and per-error
histograms:

.. code-block:: c

   mb_diag_counters_t diag;
   mb_server_get_diag(&server, &diag);

   printf("fc03 handled %llu times\n",
          (unsigned long long)diag.function[MB_PDU_FC_READ_HOLDING_REGISTERS]);
   printf("timeouts recorded: %llu\n",
          (unsigned long long)diag.error[MB_DIAG_ERR_SLOT_TIMEOUT]);

Call ``mb_client_reset_diag`` / ``mb_server_reset_diag`` after exporting the
snapshot.  Use ``mb_diag_err_slot_str`` to print bucket names.

Event Callbacks (Gate 28/29)
----------------------------

Structured events provide real-time insight into the state machine:

.. code-block:: c

   static void on_event(const mb_event_t *evt, void *user)
   {
       (void)user;
       if (evt->source == MB_EVENT_SOURCE_CLIENT &&
           evt->type == MB_EVENT_CLIENT_TX_COMPLETE) {
           printf("TX complete fc=%u status=%d\n",
                  evt->data.client_txn.function,
                  evt->data.client_txn.status);
       }
   }

   mb_client_set_event_callback(&client, on_event, NULL);

Event types include state transitions (enter/leave), transaction submission and
completion, and server request acceptance.  ``mb_event_t.timestamp`` contains the
``now`` time in milliseconds.

Hex Tracing
-----------

For on-device debugging you can enable hexadecimal frame dumps:

.. code-block:: c

   mb_client_set_trace_hex(&client, true);
   mb_server_set_trace_hex(&server, true);

Traces are emitted using ``MB_LOG_DEBUG`` (the logging backend selected by
``modbus/mb_log.h``).  Set ``MB_CONF_DIAG_TRACE_DEPTH`` to limit the number of
bytes recorded per frame.

Queue Inspection
----------------

Gate 29 adds helper accessors to inspect queue depth and jitter:

* ``mb_client_pending`` / ``mb_server_pending`` – number of in-flight
  transactions.
* ``mb_client_set_queue_capacity`` / ``mb_server_set_queue_capacity`` – adjust
  maximum outstanding transactions at runtime.
* ``mb_client_get_poll_jitter`` *(see ``mb_poll.h``)* – telemetry for poll-to-poll
  latency variance (useful when coupled with power management).

Integrating With External Telemetry
-----------------------------------

Many users export metrics via Prometheus, MQTT or proprietary telemetry stacks.
A typical pattern: poll metrics/diagnostics inside a fast timer task, export via
lightweight protocol, then reset the counters to start a new window.

.. code-block:: c

   static void telemetry_tick(void)
   {
       mb_client_metrics_t metrics;
       mb_client_get_metrics(&client, &metrics);

       send_metrics(metrics);  /* user-defined */
       mb_client_reset_metrics(&client);
   }

Consider building a small shim that redacts function codes or error classes if
telemetry leaves the device, to avoid leaking sensitive information.

Logging Backend
---------------

The default logging layer (``modbus/mb_log.h``) provides hooks for MCU, POSIX
and Windows hosts.  Define ``MB_LOG_ENABLED`` and supply ``mb_log_impl`` to route
logs to UART, SEGGER RTT or systemd.  Hex tracing and power management warnings
use this facility.
