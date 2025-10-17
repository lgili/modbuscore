# Renesas RL78 SCI Modbus RTU quickstart

This quickstart targets RL78 projects that rely on the Renesas Code Generator
(or Smart Configurator) UART drivers. It bridges the drop-in Modbus client to a
half-duplex RS-485 link by reusing the generated `R_UARTx_*` primitives and the
standard notification callbacks.

## What you get

- `modbus_rl78_sci.c`: portable glue around UARTx (default: UART0 on SAU0).
- Ring buffer driven by the `r_uartx_callback_receiveend()` interrupt.
- Automatic T3.5 framing using a user-supplied microsecond timer.
- Request helpers powered by `mb_embed.h` so you can queue traffic in one line.

## Before you start

1. Generate a UART instance with the Code Generator (8E1 or 8N1) and enable the
   receive/transmit end callbacks.
2. Configure a free-running timer that returns microseconds (e.g. TAU channel in
   interval mode) so the helper can detect frame silences.
3. Export the following driver symbols:
   - `R_UART0_Start`, `R_UART0_Send`, `R_UART0_Receive` (or the UART number you
     select).
   - Callback hooks `r_uart0_callback_receiveend` and `r_uart0_callback_sendend`
     which this quickstart overrides.

If you use a different UART channel, define `MODBUS_RL78_UART_CHANNEL` to 1 or 2
*before* including `modbus_rl78_sci.c`.

## Wiring it up

```c
#include "modbus_amalgamated.h"
#include "modbus_rl78_sci.h" // declare the helper API

static uint32_t board_now_us(void *ctx)
{
    (void)ctx;
    return TAU0_Channel0_CurrentCount(); // user function returning microseconds
}

static void board_delay_us(uint32_t usec, void *ctx)
{
    uint32_t start = board_now_us(ctx);
    while ((board_now_us(ctx) - start) < usec) {
        __nop();
    }
}

static void board_set_direction(bool is_tx, void *ctx)
{
    (void)ctx;
    if (is_tx) {
        P1 |= 0x01;   // drive DE high
    } else {
        P1 &= (uint8_t)~0x01;
    }
}

void app_modbus_init(void)
{
    static mb_client_txn_t txn_pool[MB_EMBED_CLIENT_POOL_MIN];

    R_UART0_Create();
    R_UART0_Start();

    modbus_rl78_sci_config_t cfg = {
        .baudrate = 115200,
        .parity_enabled = true,
        .two_stop_bits = false,
        .silence_timeout_ms = 5,
        .now_us = board_now_us,
        .delay_us = board_delay_us,
        .set_direction = board_set_direction,
        .user_ctx = NULL,
    };

    modbus_rl78_sci_init(&cfg, txn_pool, MB_COUNTOF(txn_pool));
}

void app_modbus_poll(void)
{
    modbus_rl78_sci_poll();
}
```

Call `modbus_rl78_sci_submit_read_inputs()` / `_submit_write_single()` to queue
traffic, or operate directly on the underlying `mb_client_t *` when you need
advanced control.

## ISR wiring

The helper exports the callback bodies, so make sure your project does **not**
provide its own `r_uart0_callback_receiveend()` / `r_uart0_callback_sendend()`.
They feed the ring buffer and toggle the DE/RE GPIO, respectively.

## Frame timing

The helper derives the Modbus T3.5 window from the configured baud rate and
parity setting. If you need a custom value (for example, when using a gap timer)
pass `silence_timeout_ms = 0` and call `mb_rtu_set_silence_timeout()` manually
on the returned client handle.

## Testing checklist

- Toggle the DE/RE GPIO with a logic analyser and verify it switches a few
  microseconds before the first byte and releases shortly after TX complete.
- Enable hex tracing (`mb_client_set_trace_hex(modbus_rl78_sci_client(), true)`)
  and confirm you see the expected PDUs.
- Stress test by spamming read requests; the ring buffer should never overflow
  (watch for warnings on the UART RX callback).

Happy hacking with the RL78!
```