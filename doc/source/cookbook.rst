Cookbook
========

Quick recipes that showcase common integration scenarios. All snippets assume
``#include <modbus/modbus.h>`` and build on top of the abstractions introduced
in Gates 5–11.

.. contents:: Recipes
   :local:

Running the bundled demos
-------------------------

Build the examples preset once to compile all helper binaries:

.. code-block:: bash

    cmake --preset host-debug-examples
    cmake --build --preset host-debug-examples --target \
      modbus_tcp_server_demo \
      modbus_tcp_client_cli \
            modbus_rtu_loop_demo \
            modbus_rtu_serial_server \
            modbus_rtu_serial_client

The executables are emitted under ``build/host-debug-examples/examples``.

Modbus TCP client/server pair
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Launch the server in one terminal (add ``--trace`` for verbose framing logs):

.. code-block:: bash

    ./build/host-debug-examples/examples/modbus_tcp_server_demo \
      --port 1502 \
      --unit 17 \
      --trace

Drive it with the CLI from another terminal, adjusting host/unit/registers to
match your target device:

.. code-block:: bash

    ./build/host-debug-examples/examples/modbus_tcp_client_cli \
      --host 127.0.0.1 \
      --port 1502 \
      --unit 17 \
      --register 0 \
      --count 4

Includes an optional ``--expect v1,v2,...`` flag to assert the returned holding
register values.

RTU in-memory loopback
~~~~~~~~~~~~~~~~~~~~~~

Exercise the RTU client/server FSMs without any external hardware:

.. code-block:: bash

    ./build/host-debug-examples/examples/modbus_rtu_loop_demo

The demo prints both client and server state transitions and dumps the register
values pulled through the loopback transport.

RTU serial client/server pair
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use the portable serial helper to talk to real COM/TTY devices (USB adapters or
virtual pairs).

**1. Optional – create a virtual serial link**

- *Windows*: install `com0com <https://sourceforge.net/projects/com0com/>`_ and
    create a linked pair such as ``COM5`` ↔ ``COM6``.
- *macOS/Linux*: spawn two pseudo-terminals via ``socat`` and keep the process
    running:

    .. code-block:: bash

         socat -d -d pty,raw,echo=0,link=./ttyV0 pty,raw,echo=0,link=./ttyV1

    The devices become available as ``/dev/ttyV0`` and ``/dev/ttyV1``.

**2. Start the RTU server**

.. code-block:: bash

     ./build/host-debug-examples/examples/modbus_rtu_serial_server \
         --device /dev/ttyV0 \
         --baud 115200 \
         --unit 17 \
         --trace

**3. Poll registers with the RTU client**

.. code-block:: bash

     ./build/host-debug-examples/examples/modbus_rtu_serial_client \
         --device /dev/ttyV1 \
         --baud 115200 \
         --unit 17 \
         --interval 1000 \
         --trace

Swap the device paths for your actual USB-to-RS485 adapter when targeting real
hardware. The server keeps a small region of holding registers updated, while
the client prints the values returned by each poll.

Minimal RTU client loop
-----------------------

Bring up an RTU client that reads two holding registers from unit ``0x11``.

.. code-block:: c

   static void on_complete(mb_client_t *client,
                           const mb_client_txn_t *txn,
                           mb_err_t status,
                           const mb_adu_view_t *response,
                           void *user_ctx)
   {
       (void)client;
       (void)user_ctx;
       if (status != MB_OK || response == NULL) {
           printf("transaction failed: %d\n", status);
           return;
       }

       const mb_u8 *bytes = response->payload;
       const mb_size_t len = response->payload_len;
       for (mb_size_t i = 1; i < len; i += 2U) {
           const uint16_t value = (uint16_t)((bytes[i] << 8) | bytes[i + 1U]);
           printf("reg[%zu] = 0x%04X\n", (size_t)((i - 1U) / 2U), value);
       }
   }

   void app_main(void)
   {
       mb_client_t client;
       mb_client_txn_t pool[4];
       const mb_transport_if_t *iface = make_rtu_iface();

       MB_ERR_CHECK(mb_client_init(&client, iface, pool, MB_COUNTOF(pool)));

       mb_client_request_t req = {
           .request.unit_id = 0x11U,
           .request.function = MB_PDU_FC_READ_HOLDING_REGISTERS,
           .request.payload = my_pdu_buffer,
           .request.payload_len = build_fc03_payload(my_pdu_buffer, 0x0000, 2U),
           .timeout_ms = 250U,
           .max_retries = 1U,
           .callback = on_complete,
       };

       MB_ERR_CHECK(mb_client_submit(&client, &req, NULL));

       while (!app_should_stop()) {
           MB_ERR_CHECK(mb_client_poll(&client));
           app_do_other_work();
       }
   }

Key points:

* Prepare a transaction pool sized for your concurrency.
* Build the PDU payload ahead of submission (helpers live in ``modbus/pdu.h``).
* Use callbacks (or poll ``txn->status``) to consume responses without blocking.

Server with observability hooks
-------------------------------

Expose a holding-register region backed by static storage while broadcasting
observable events to a metrics collector.

.. code-block:: c

   static uint16_t holding_regs[64];

   static void event_sink(const mb_event_t *evt, void *user)
   {
       (void)user;
       if (evt->source == MB_EVENT_SOURCE_SERVER &&
           evt->type == MB_EVENT_SERVER_REQUEST_COMPLETE) {
           printf("fc=%u status=%d\n",
                  evt->data.server_req.function,
                  evt->data.server_req.status);
       }
   }

   void bring_up_server(const mb_transport_if_t *iface)
   {
       mb_server_t server;
       mb_server_region_t regions[2];
       mb_server_request_t requests[4];

       MB_ERR_CHECK(mb_server_init(&server,
                                   iface,
                                   0x11U,
                                   regions,
                                   MB_COUNTOF(regions),
                                   requests,
                                   MB_COUNTOF(requests)));

       MB_ERR_CHECK(mb_server_add_storage(&server,
                                          0x0000U,
                                          MB_COUNTOF(holding_regs),
                                          false,
                                          holding_regs));

       mb_server_set_event_callback(&server, event_sink, NULL);
       mb_server_set_trace_hex(&server, true);

       for (;;) {
           MB_ERR_CHECK(mb_server_poll(&server));
           feed_watchdog();

           static uint32_t tick;
           if ((tick++ % 1000U) == 0U) {
               mb_diag_counters_t diag;
               mb_server_get_diag(&server, &diag);
               printf("fc03=%llu errors=%llu\n",
                      (unsigned long long)diag.function[MB_PDU_FC_READ_HOLDING_REGISTERS],
                      (unsigned long long)diag.error[MB_DIAG_ERR_SLOT_TIMEOUT]);
               mb_server_reset_diag(&server);
           }
       }
   }

Recipe highlights:

* ``mb_server_add_storage`` wires a contiguous region without custom callbacks.
* Diagnostics can be sampled and reset opportunistically (e.g. every N ticks).
* Hex tracing feeds into the existing logging sink (``MB_LOG_DEBUG`` level).

Switching to Modbus TCP with multiple sockets
---------------------------------------------

Re-use the same client FSM while serving several TCP connections.

.. code-block:: c

   mb_tcp_multi_transport_t multi;
   mb_tcp_multi_init(&multi,
                     socket_array,
                     socket_count,
                     my_accept_callback,
                     my_close_callback);

   const mb_transport_if_t *iface = mb_tcp_multi_iface(&multi);

   mb_client_t client;
   mb_client_txn_t pool[8];
   MB_ERR_CHECK(mb_client_init_tcp(&client, iface, pool, MB_COUNTOF(pool)));

   /* submit transactions as usual – the multi transport handles TID routing */

   for (;;) {
       MB_ERR_CHECK(mb_client_poll(&client));
       MB_ERR_CHECK(mb_tcp_multi_poll(&multi));
   }

When load increases, grow the transaction pool and queue capacity:

.. code-block:: c

   mb_client_set_queue_capacity(&client, 16U);
   mb_client_set_watchdog(&client, 1000U);

This keeps retries and watchdog handling consistent while the multi-transport
backs additional sockets behind the same FSM.

Testing with the mock transport
-------------------------------

For unit or integration tests you can bypass real transports using the mock
helpers under ``tests``. The C++ harness exposes ``mock_transport_get_iface``
so a test can drive the FSM while injecting raw ADUs.

.. code-block:: cpp

   extern "C" {
   const mb_transport_if_t *mock_transport_get_iface(void);
   int mock_inject_rx_data(const uint8_t *data, uint16_t length);
   uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen);
   }

   TEST_F(MbClientTest, CustomScenario)
   {
       const mb_transport_if_t *iface = mock_transport_get_iface();
       ASSERT_EQ(MB_OK, mb_client_init(&client_, iface, pool_, pool_len_));

       /* ... submit requests and assert on mock_get_tx_data output ... */
   }

The same helpers power ``test_modbus_client`` and ``test_modbus_server``.

Further reading:

* :doc:`usage` – high-level walkthrough for initialisation and telemetry.
* :doc:`ports` – detailed HAL adapter guidelines.
* :doc:`migration` – notes on upgrading to the 1.0.0 API surface.
