# Gate 20 HIL validation playbook

This checklist closes the Gate 20 acceptance criteria by validating the
STM32 LL DMA + IDLE reference on real hardware. Run the suite whenever you
port the helper to a new board or tweak guard-time parameters.

## Equipment

- STM32 Nucleo or custom board with UART exposed (tested on STM32G0B1 Nucleo).
- RS-485 transceiver (MAX3485 or equivalent) wired for half-duplex operation.
- Logic analyser or oscilloscope capable of sampling at ≥10 MSa/s.
- Host PC running the Modbus examples (`examples/rtu_loop_demo.c`).
- Optional: digital pattern/noise generator (UART glitcher) to inject bursts.

## Firmware setup

1. Copy the quickstart bundle (`modbus_amalgamated.c/.h` plus the
   `modbus_stm32_idle.[ch]` pair) into your firmware tree.
2. Populate `modbus_stm32_idle_config_t` with the target baud/frame settings and
   leave `t15_guard_us` / `t35_guard_us` at zero so the helper derives the guard
   times.
3. Enable the free-running 1 MHz timer used by the callbacks and verify the
   logic analyser sees the expected T1.5/T3.5 gaps on a single frame before
   proceeding.
4. Build the firmware with optimisation enabled (Release/RelWithDebInfo) and
   flash it to the board.

## Host loop demo

1. Build the loop demo on the host:

   ```bash
   cmake -S examples -B build/examples
   cmake --build build/examples --target rtu_loop_demo
   ```

2. Connect the host UART adapter to the RS-485 transceiver and start the demo:

   ```bash
   ./build/examples/rtu_loop_demo --port /dev/ttyUSB0 --baud 115200 --parity even
   ```

3. Confirm the MCU responds to requests for at least one minute without CRC or
   timeout errors.

## Test matrix

Repeat the following sequence for each baud rate/parity combination:

| Baud (bps) | Parity | Stop bits | Expectation                                    |
|-----------:|:-------|:----------|:-----------------------------------------------|
| 19 200     | Even   | 1         | No overruns, guard gaps ≥ 860 µs, error rate <0.1% |
| 38 400     | Even   | 1         | Guard gaps ≥ 430 µs, error rate <0.1%          |
| 115 200    | Even   | 1         | Guard gaps ≥ 144 µs, error rate <0.1%          |

For each row:

1. Update the UART initialiser and host demo arguments to the target baud.
2. Capture at least 1 000 request/response pairs with the logic analyser.
3. Measure the silent interval after each transmitted frame – it must meet or
   exceed the T3.5 value in the table above.
4. Inspect the RX buffer statistics (optional: instrument `mb_diag` counters) to
   ensure no overrun events were raised.

## Noise injection

1. With the baud rate set to 115 200 8E1, inject ±1 byte of random noise using
   the glitcher during idle periods.
2. Observe that `modbus_stm32_idle_isr` recovers cleanly (no stuck DMA) and that
   the Modbus client rejects the corrupted frames while continuing to service
   valid requests.
3. Target an error rate below 0.1% across 10 000 frames.

## Sign-off

- [ ] Guard times measured ≥ expected thresholds for all baud rates.
- [ ] No DMA overruns or USART framing errors observed.
- [ ] Client transaction success rate ≥ 99.9% with injected noise.
- [ ] Results recorded in the project wiki or engineering logbook.

Once all boxes are ticked, Gate 20’s HIL gate is considered green.
