Power Management (Gate 27)
==========================

Idle detection and poll jitter tracking are optional features controlled by
``MB_CONF_ENABLE_POWER_MANAGEMENT``.  When enabled, clients and servers collect
timing information that can be used to implement low-power strategies or detect
CPU starvation.

Idle Handling
-------------

``mb_power.h`` exposes ``mb_idle_config_t`` which is embedded in both
``mb_client_t`` and ``mb_server_t`` when power management is active.  Configure
the idle threshold and callback:

.. code-block:: c

   void on_idle(const mb_idle_state_t *state, void *user)
   {
       (void)user;
       if (state->pending == 0) {
           enter_low_power_mode();
       }
   }

   mb_idle_config_t cfg = {
       .threshold_ms = 50,
       .callback = on_idle,
       .user_ctx = NULL,
   };

   mb_client_configure_idle(&client, &cfg);

When the FSM observes no activity for ``threshold_ms`` it invokes the callback
with the number of pending transactions and poll jitter statistics.  Returning
from the callback resumes normal operation.

Poll Jitter Metrics
-------------------

Gate 29 introduces ``mb_poll_jitter_t`` which records the maximum and average
gap (in milliseconds) between consecutive calls to ``mb_client_poll`` /
``mb_server_poll``.  Retrieve it as part of the idle callback or with
``mb_client_get_poll_jitter``.

This information is useful to ensure that sporadic tasks (logging, telemetry)
do not starve Modbus processing or to dynamically adjust poll frequency.

Integrating With RTOS Tickless Mode
-----------------------------------

Combine idle callbacks with RTOS power features:

* FreeRTOS: call ``eTaskConfirmSleepModeStatus`` inside the idle handler to
  decide whether to enter tickless idle.
* Zephyr: leverage ``pm_system_suspend`` or custom power domains when the queue
  is empty.

For bare-metal targets you can gate UART RX/TX clocks or lower the CPU frequency
while the client is idle.

Runtime Adjustments
-------------------

The idle configuration can be changed on the fly:

.. code-block:: c

   mb_idle_config_t cfg;
   mb_client_get_idle_config(&client, &cfg);
   cfg.threshold_ms = 200;
   mb_client_configure_idle(&client, &cfg);

Setting ``callback = NULL`` disables idle notifications while keeping jitter
tracking active.

Memory Considerations
---------------------

Each instance stores ``mb_idle_config_t`` plus jitter counters (~64 bytes).  If
this overhead is undesirable set ``MB_CONF_ENABLE_POWER_MANAGEMENT=0``.  The
API macros in ``mb_power.h`` compile to no-ops in that case.
