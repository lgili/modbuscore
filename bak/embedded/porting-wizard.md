# Embedded porting wizard

The aim of Gate 17 is “integration in minutes”. Use this single-page wizard to
graft the Modbus client onto your firmware with predictable effort. Each step
links back to the relevant quickstart or reference implementation inside
`embedded/quickstarts/`.

---

## 1. Pick the right bundle

| Scenario | Start here | Notes |
| --- | --- | --- |
| Minimal RTU client on any MCU | [`quickstarts/drop_in`](quickstarts/drop_in/README.md) | Two files (`modbus_amalgamated.c/.h`) with zero external deps. |
| ESP-IDF project | [`components/esp-idf/modbus`](quickstarts/components/esp-idf/modbus/README.md) | Menuconfig toggles defaults to LEAN profile and UART helper. |
| Zephyr networking device | [`components/zephyr`](quickstarts/components/zephyr/README.md) | Ships a TCP helper + `hello_tcp` sample application. |
| STM32 LL + DMA/IDLE | [`ports/stm32-ll-dma-idle`](quickstarts/ports/stm32-ll-dma-idle/README.md) | Circular DMA + IDLE ISR with DE/RE GPIO control. |
| NXP LPUART + IDLE | [`ports/nxp-lpuart-idle`](quickstarts/ports/nxp-lpuart-idle/README.md) | Interrupt-driven ring buffer over the SDK’s LPUART driver. |
| Renesas RL78 (SCI) | [`ports/renesas-rl78-sci`](quickstarts/ports/renesas-rl78-sci/README.md) | Ready for Code Generator projects with full clock / IRQ glue. |

When in doubt, start with the drop-in amalgamation and upgrade to the
platform-specific helper later.

---

## 2. Configure transport primitives

✅ Clock source

- Provide a microsecond timer for timeouts (hardware timer, DWT, or SDK helper).
- Ensure the timer wraps at ≥65 ms to keep T3.5 calculations accurate.
- Verify the timer continues running in low-power modes if the UART does.

✅ UART / serial engine

- Enable DMA (preferred) or interrupt-driven receive with a scratch buffer ≥256 B.
- Turn on IDLE detection (RTU) or ensure TCP socket is non-blocking (Zephyr helper).
- Calibrate baud rate and oversampling — a ±1.5 % drift can truncate frames.
- For RS-485, expose DE/RE GPIOs; default polarity is active-high but wireability is configurable in each helper.

✅ Delay primitive

- Provide `delay_us(usec)` either via a busy-wait tied to the same timer as
	`now_us()` or via the RTOS ticker (FreeRTOS `vTaskDelayUntil` coarse delays plus
	busy wait for the tail).

---

## 3. Initialise the Modbus client

1. Allocate a transaction pool (4–8 slots covers most applications). For static
	 builds, embed `mb_client_txn_t pool[MB_EMBED_POOL_MIN]` in `.bss`.
2. Call the transport initialiser provided by your quickstart:
	 - `modbus_esp_quickstart_init()` (ESP-IDF).
	 - `modbus_zephyr_client_init()` (Zephyr TCP helper).
	 - `modbus_stm32_idle_init()` / `modbus_nxp_lpuart_idle_init()` /
		 `modbus_rl78_sci_init()` for bare-metal UART ports.
3. Feed the helper with your UART handles, DMA channels, timers and DE/RE pins
	 (see the README tables for required arguments).

Use `mb_embed_client_config_t` (from `include/modbus/mb_embed.h`) to tie these
 pieces together with a single call.

---

## 4. Drive the finite-state machine

- Poll the client from your main loop or RTOS task at ≥1 kHz (RTU) or whenever
	sockets are readable (TCP). The helpers expose `*_poll()` functions that wrap
	`mb_client_poll()` and account for ISR handoffs.
- For power-sensitive devices, call `mb_client_is_idle()` after each poll and
	drop to sleep until the next UART/TCP interrupt.
- Use `mb_client_set_watchdog()` to bound wedges caused by a broken link.

---

## 5. Submit traffic the ergonomic way

`mb_embed.h` exposes inline shims:

- `mb_embed_submit_read_holding()` / `_inputs()` / `_write_single()` helpers for
	the most common FCs.
- `mb_embed_request_opts_t` to tweak timeout, retries and queue priority without
	touching the raw `mb_client_request_t` fields.
- Convenience builders such as `mb_embed_build_write_multiple()` that hand back
	ADU views for advanced batching.

These helpers compose with the quickstarts—each port simply forwards to the
`mb_embed_*` wrappers after translating IRQ events.

---

## 6. Validate and monitor

- Enable logging via `mb_client_set_trace_hex()` during bring-up to watch raw
	PDUs.
- Snapshot diagnostics with `mb_client_get_diag_snapshot()` before and after a
	test run; watch for CRC errors (wiring) and timeouts (baud drift).
- Use the provided “hello” samples:
	- ESP-IDF: `idf.py -p <port> flash monitor` in
		`components/esp-idf/modbus/examples/hello_rtu`.
	- Zephyr: `west build -p auto -b <board> zephyr/samples/hello_tcp`.
	- STM32 / NXP / Renesas: supplied `main.c` fragments in each port README.

When the sample runs without modifying core sources, Gate 17’s success
criterion is met.

---

## 7. Troubleshooting cheat sheet

| Symptom | Check | Fix |
| --- | --- | --- |
| Frames truncated | Timer not monotonic or IDLE flag not cleared | Use 32-bit timer, clear IDLE before starting DMA, ensure buffer ≥256 B |
| Occasional CRC errors | RS-485 DE/RE toggling late | Drive the GPIO as soon as TX starts, hold for 2 byte-times after last byte |
| Client queue stalling | `mb_client_pending()` never drops | Increase poll frequency or reduce retries; set watchdog to abort wedged XS |
| TCP helper never connects | Zephyr network interface down | Enable DHCP in `prj.conf` or give static IP, ensure `CONFIG_NET_SOCKETS=y` |

Document issues you fix locally back into the quickstart README so the next
integration is even smoother.
