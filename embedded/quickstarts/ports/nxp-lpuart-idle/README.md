# NXP LPUART IDLE detection Modbus RTU port

This quickstart demonstrates how to pair the Modbus drop-in client with an
NXP MCU using the LPUART peripheral in IDLE-detect mode. Hardware-specific
glue is injected via `modbus_nxp_lpuart_idle_config_t`, so you keep full control
over clocks, timers, and RS-485 transceivers.

## What you get

- `modbus_nxp_lpuart_idle.c`: reference glue adaptable to MCUXpresso or bare
  SDK projects.
- Helpers for submitting read/write requests without touching the lower-level
  Modbus API.
- ISR hooks that translate MUART interrupt events into transport callbacks.

## Bringing it into your project

1. Regenerate the drop-in sources when you update the library:

   ```sh
   python3 scripts/amalgamate.py
   ```

2. Copy the following files into your firmware tree:

   - `embedded/quickstarts/drop_in/modbus_amalgamated.c`
   - `embedded/quickstarts/drop_in/modbus_amalgamated.h`
   - `embedded/quickstarts/ports/nxp-lpuart-idle/modbus_nxp_lpuart_idle.c`

3. Initialize the helper early in `main()`:

   ```c
   mb_client_txn_t txn_pool[MB_EMBED_CLIENT_POOL_MIN];
   modbus_nxp_lpuart_idle_config_t cfg = {
      .uart = LPUART1,
      .src_clock_hz = CLOCK_GetFreq(kCLOCK_Osc0ErClk),
      .baudrate = 115200,
      .parity = kLPUART_ParityDisabled,
      .stop_bits = kLPUART_OneStopBit,
      .silence_timeout_ms = 5,
      .now_us = board_now_us,
      .delay_us = board_delay_us,
      .set_direction = board_set_direction,
      .user_ctx = NULL,
   };
   modbus_nxp_lpuart_idle_init(&cfg, txn_pool, MB_COUNTOF(txn_pool));
   ```

4. Call `modbus_nxp_lpuart_idle_poll()` from your cooperative scheduler or main
   superloop.

5. Forward UART interrupts to the provided handler:

   ```c
   void LPUART1_IRQHandler(void)
   {
       modbus_nxp_lpuart_idle_isr();
   }
   ```

6. Submit Modbus requests using
   `modbus_nxp_lpuart_idle_submit_read_inputs()` /
   `_submit_write_single()` or operate directly on the exposed
   `mb_client_t *`.

### Callback stubs

```c
static uint32_t board_now_us(void *ctx)
{
   (void)ctx;
   return PIT_GetCurrentTimerCount(PIT, kPIT_Chnl_0) / (CLOCK_GetFreq(kCLOCK_BusClk) / 1000000U);
}

static void board_delay_us(uint32_t usec, void *ctx)
{
   uint32_t start = board_now_us(ctx);
   while ((board_now_us(ctx) - start) < usec) {
   }
}

static void board_set_direction(bool is_tx, void *ctx)
{
   (void)ctx;
   GPIO_WritePinOutput(GPIOB, 5U, is_tx);
}
```

Adjust the pinmux, clock selector and GPIO helpers to suit your board. The
implementation keeps the MCUXpresso API calls obvious so you can port it to
whichever SDK revision you're targeting.
