Usage Guide
===========

Initialisation
--------------

1. Set up a transport interface (RTU or TCP) implementing :ref:`mb_transport_if_t`.
2. Initialise client or server with the respective init call.
3. Register register regions (server) or prepare transaction pool (client).
4. Poll the state machine regularly (``mb_client_poll`` / ``mb_server_poll``).

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
   mb_transport_if_t iface = make_rtu_iface(...);
   uint16_t holding_regs[128] = {0};

   mb_server_init(&server, &iface, 0x11U, regions, MB_COUNTOF(regions));
   mb_server_add_storage(&server, 0x0000U, MB_COUNTOF(holding_regs), false, holding_regs);

   for (;;) {
       mb_server_poll(&server);
       do_other_work();
   }

Advanced Features
-----------------

* TCP: use ``mb_client_init_tcp`` + ``mb_tcp_transport``.
* Multiple connections: ``mb_tcp_multi_*`` helpers.
* Timeouts/retries: configured per transaction.
* Watchdog: ``mb_client_set_watchdog``.

Refer to :doc:`api` for exhaustive definitions.
