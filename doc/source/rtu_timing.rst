RTU Timing Pitfalls
===================

Tight control over the silent intervals between Modbus RTU frames keeps mixed
baud links reliable.  This guide summarises the T1.5 (inter-character guard)
and T3.5 (frame silence) thresholds for the baud/parity combinations most
commonly deployed in the field and offers practical tips for configuring
hardware timers.

How the timing budget is built
------------------------------

An RTU "character" spans the start bit, the 8 data bits, an optional parity bit
and one stop bit.  The character time $t_{\text{char}}$ therefore changes with
parity configuration:

.. math::

   t_{\text{char}} = \frac{N_{\text{bits}}}{\text{baud}}, \qquad N_{\text{bits}} =
   \begin{cases}
      10 & \text{no parity (1 start, 8 data, 1 stop)} \\
      11 & \text{even/odd parity (extra parity bit)}
   \end{cases}

Once $t_{\text{char}}$ is known, the standard Modbus guard times follow
immediately:

.. math::

   T1.5 = 1.5 \times t_{\text{char}}, \qquad T3.5 = 3.5 \times t_{\text{char}}

Round the resulting values up to the next microsecond (or timer tick) to avoid
inadvertently shortening the silent interval.

Reference tables
----------------

The tables below list the computed guard times for several popular baud rates.
All values are in microseconds and already rounded up for safety.

.. list-table:: No parity (10 bits per character)
   :header-rows: 1

   * - Baud
     - Character
     - T1.5
     - T3.5
   * - 9600
     - 1,042
     - 1,563
     - 3,646
   * - 19200
     - 521
     - 782
     - 1,823
   * - 38400
     - 261
     - 391
     - 912
   * - 57600
     - 174
     - 261
     - 608
   * - 115200
     - 87
     - 131
     - 304

.. list-table:: Even/Odd parity (11 bits per character)
   :header-rows: 1

   * - Baud
     - Character
     - T1.5
     - T3.5
   * - 9600
     - 1,146
     - 1,719
     - 4,011
   * - 19200
     - 573
     - 860
     - 2,006
   * - 38400
     - 287
     - 430
     - 1,003
   * - 57600
     - 191
     - 287
     - 669
   * - 115200
     - 96
     - 144
     - 335

Timer configuration tips
------------------------

* Configure the free-running timer used by ``modbus_stm32_idle`` (or custom
  ports) to tick at 1 MHz.  T1.5/T3.5 then map directly to timer counts.
* When using cooperative delays, keep the polling cadence below T1.5 to avoid
  deferring frame validation past the guard interval.
* On Cortex-M devices the DWT cycle counter offers a convenient fallback when a
  dedicated hardware timer is unavailable.
* If the UART clock source changes at runtime (e.g. low-power modes), reapply
  the baud rate and recompute the timer divisor before resuming traffic.

Practical implementation checklist
----------------------------------

* Update ``mb_rtu_set_silence_timeout`` when operating below 9600 baud or when
  parity changes at runtime.  The helper in
  ``embedded/quickstarts/ports/stm32-ll-dma-idle`` accepts a millisecond value;
  simply round ``T3.5`` up to the next millisecond before passing it in.
* Populate ``baudrate``, ``data_bits``, ``parity_enabled`` and ``stop_bits`` in
  ``modbus_stm32_idle_config_t`` so the quickstart derives the guard intervals
  for you.  Override ``t15_guard_us`` / ``t35_guard_us`` only when timing
  validation on the oscilloscope shows a different budget is required.
* For RS-485 systems, add a short guard (~50 µs) after dropping DE to account
  for transceiver turn-off characteristics.
* Enable UART overrun detection and clear the flag inside the DMA/IDLE ISR –
  entering the ISR late is a strong indicator that T3.5 was not respected.
:orphan:
