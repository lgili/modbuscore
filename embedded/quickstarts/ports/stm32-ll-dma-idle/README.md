# STM32 LL DMA + IDLE-line Modbus RTU port

This quickstart shows how to glue the Modbus drop-in client to an STM32 UART
configured with circular DMA reception and the IDLE-line interrupt. Copy
`modbus_stm32_idle.c` into your firmware and provide the hardware callbacks via
`modbus_stm32_idle_config_t` – no TODO markers left behind.

## Highlights

- Circular DMA receives into a scratch buffer while the CPU sleeps.
- The UART IDLE interrupt marks frame boundaries and schedules the Modbus FSM
  on the main loop thread.
- Transmit operations reuse the same UART handle and rely on half-duplex DE/RE
  toggling when required.
- The helper derives and enforces T1.5/T3.5 guard times from your baud-rate
   settings, with optional microsecond overrides when you need bespoke silence
   windows.

## Integration steps

1. Regenerate the amalgamated quickstart sources if you made library changes:

   ```sh
   python3 scripts/amalgamate.py
   ```

2. Drop the following files into your project:

   - `embedded/quickstarts/drop_in/modbus_amalgamated.c`
   - `embedded/quickstarts/drop_in/modbus_amalgamated.h`
   - `embedded/quickstarts/ports/stm32-ll-dma-idle/modbus_stm32_idle.c`
   - `embedded/quickstarts/ports/stm32-ll-dma-idle/modbus_stm32_idle.h`

3. `#include "modbus_stm32_idle.h"` in the module that performs the
    initialization and call `modbus_stm32_idle_init()` once during boot,
    supplying:
    - `uart`, `dma` and `dma_channel` already configured for circular RX.
    - `silence_timeout_ms` (optional, defaults to the library value of 5 ms).
    - UART framing info (`baudrate`, `data_bits`, `parity_enabled`,
       `stop_bits`). Zero/`false` values fall back to 8 data bits, no parity and a
       single stop bit; you must set `baudrate` if you want the helper to derive
       T1.5/T3.5 automatically.
    - Guard overrides `t15_guard_us` / `t35_guard_us` when you want to force
       specific microsecond values. If left at zero they’re derived from the
       baud-rate settings using the Modbus formulae.
    - Callback trio (`now_us`, `delay_us`, `set_direction`) that hook into your
       timer and RS-485 transceiver control. You can pass `NULL` for
       `set_direction` on full-duplex links.
    - A transaction pool (`mb_client_txn_t pool[MB_EMBED_CLIENT_POOL_MIN]`).

4. From the main loop (or a cooperative scheduler task) call
   `modbus_stm32_idle_poll()` which in turn drives `mb_client_poll()`.

5. Bridge the provided ISR hooks into your vector table:

   ```c
   void USART1_IRQHandler(void)
   {
       modbus_stm32_idle_usart_isr();
   }

   void DMA1_Channel5_IRQHandler(void)
   {
       modbus_stm32_idle_dma_isr();
   }
   ```

6. Submit Modbus requests through
   `modbus_stm32_idle_submit_read_inputs()` / `_submit_write_single()` (wrappers
   around the `mb_embed_*` helpers) or obtain the underlying `mb_client_t *` to
   use the full client API.

### Callback cheat sheet

```c
static uint32_t board_now_us(void *ctx)
{
   (void)ctx;
   return (uint32_t)LL_TIM_GetCounter(TIM2); // TIM2 ticking at 1 MHz
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
   if (is_tx) {
      LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_1); // DE high
   } else {
      LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_1);
   }
}
```

Pass the trio above (or your RTOS equivalents) in the configuration struct to
enable microsecond timing and DE/RE control without touching the helper.
