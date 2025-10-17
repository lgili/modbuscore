# STM32 LL DMA + IDLE Reference Port

This reference shows how to pair the Modbus RTU client with an STM32 USART
configured for circular DMA reception and IDLE-line detection. It complements
the drop-in quickstart located at
``embedded/quickstarts/ports/stm32-ll-dma-idle`` by documenting the hardware
setup, timing knobs and ISR glue you need in production firmware.

## Hardware overview

- **USART configuration** – enable IDLE detection, RX DMA requests and, when
	driving 485/422 transceivers, half-duplex mode with DE/RE GPIO control.
- **DMA channel** – set up in circular mode with memory incrementing, peripheral
	fixed to ``USARTx->RDR`` and an interrupt on transfer complete so the ISR is
	notified even when the IDLE condition is missed.
- **Timer source** – a 1 MHz (or similar) free-running timer provides accurate
	microsecond timestamps for T1.5/T3.5 enforcement. Use the same timer for
	silence timeout calculations inside the Modbus RTU stack.

## Integration steps

1. Copy the quickstart bundle into your firmware tree:

	 - ``embedded/quickstarts/drop_in/modbus_amalgamated.c``
	 - ``embedded/quickstarts/drop_in/modbus_amalgamated.h``
	 - ``embedded/quickstarts/ports/stm32-ll-dma-idle/modbus_stm32_idle.c``
	 - ``embedded/quickstarts/ports/stm32-ll-dma-idle/modbus_stm32_idle.h``

2. Enable the peripheral clocks and initialise the USART/DMA pair in circular
	 reception mode. Example (G0 family using LL drivers):

	 ```c
	 LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);
	 LL_USART_Disable(USART1);
	 LL_USART_ConfigCharacter(USART1, LL_USART_DATAWIDTH_8B, LL_USART_PARITY_NONE, LL_USART_STOPBITS_1);
	 LL_USART_SetBaudRate(USART1, LL_RCC_GetUSARTClockFreq(LL_RCC_USART1_CLKSOURCE), LL_USART_OVERSAMPLING_16, 115200);
	 LL_USART_EnableDMAReq_RX(USART1);
	 LL_USART_EnableIT_IDLE(USART1);
	 LL_USART_Enable(USART1);

	 LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)&USART1->RDR);
	 LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, MODBUS_STM32_IDLE_RX_SIZE);
	 LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_HIGH);
	 LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);
	 LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
	 ```

3. Provide the microsecond timer, micro-sleep and transceiver callbacks via
	 ``modbus_stm32_idle_config_t`` and call ``modbus_stm32_idle_init`` once:

	 ```c
	 static uint32_t board_now_us(void *ctx) { (void)ctx; return LL_TIM_GetCounter(TIM2); }
	   static void board_delay_us(uint32_t us, void *ctx)
	   {
		   (void)ctx;
		   const uint32_t start = board_now_us(NULL);
		   while ((board_now_us(NULL) - start) < us) {
		   }
	   }
	 static void board_set_direction(bool tx, void *ctx) { (void)ctx; gpio_set_tx_enable(tx); }

	 static mb_client_txn_t txn_pool[MB_EMBED_CLIENT_POOL_MIN];

	 const modbus_stm32_idle_config_t cfg = {
			 .uart = USART1,
			 .dma = DMA1,
			 .dma_channel = LL_DMA_CHANNEL_1,
			 .silence_timeout_ms = 0U,            // keep library default
			 .baudrate = 115200U,
			 .data_bits = 8U,
			 .parity_enabled = false,
			 .stop_bits = 1U,
			 .t15_guard_us = 0U,
			 .t35_guard_us = 0U,
			 .now_us = board_now_us,
			 .delay_us = board_delay_us,
			 .set_direction = board_set_direction,
			 .user_ctx = NULL,
	 };

	 MB_CHECK_OK(modbus_stm32_idle_init(&cfg, txn_pool, MB_ARRAY_SIZE(txn_pool)));
	 ```

4. Forward the IDLE interrupt and DMA channel ISR to the helper:

	 ```c
	 void USART1_IRQHandler(void)
	 {
			 modbus_stm32_idle_usart_isr();
	 }

	 void DMA1_Channel1_IRQHandler(void)
	 {
			 modbus_stm32_idle_dma_isr();
	 }
	 ```

5. Drive ``modbus_stm32_idle_poll`` from your cooperative scheduler or main
	 loop. The helper keeps the DMA buffer “hot” and feeds the Modbus client
	 state machine without extra copies.

## Zero-copy DMA consumption

The circular buffer configured in ``modbus_stm32_idle_uart_start_rx`` is shared
between the DMA engine and the Modbus transport. Only the indices are updated
as new bytes arrive, so the RTU parser reads directly from the DMA memory.
No shuffling or memcpy into temporary buffers occurs—only the final delivery to
the Modbus core copies bytes into the ADU staging area required by
``mb_transport_if_t``.

## Enforcing T1.5/T3.5 guards

- Populate ``baudrate`` (and optionally ``data_bits``, ``parity_enabled`` and
	``stop_bits``) so the helper can derive the character time and emit
	standards-compliant T1.5/T3.5 delays.
- Override ``t15_guard_us`` / ``t35_guard_us`` for exotic baud rates or when you
	time-slice the Modbus traffic aggressively and want to pad extra silence.
- Microsecond timing requires ``now_us``; the guard waits re-use ``delay_us`` if
	provided, otherwise they fall back to a tight busy-wait using the same clock.

## Timing guidance

- Set ``silence_timeout_ms`` when your environment requires a different T3.5
	than the default 4 ms at 19200 baud. Values below 4 ms are automatically
	rounded up to maintain protocol compliance.
- Use a timer clocked independently from the UART peripheral to avoid drift
	when the APB clock tree changes (e.g., during low-power transitions).
- Refer to the timing guide in ``doc/source/rtu_timing.rst`` for T1.5/T3.5
	tables covering the most common baud/parity combinations.

## Testing checklist

- Loop back USART TX/RX pins and verify the helper echoes a 256-byte burst
	without gaps or overrun errors.
- Inject IDLE-line races (short delays between frames) to ensure the DMA ISR
	path still marks ``idle_flag`` correctly.
- With RS-485 hardware attached, scope the DE/RE GPIO to confirm the helper
	toggles the transceiver only when ``modbus_stm32_idle_uart_send`` runs.
- Validate T1.5/T3.5: capture the TX waveform at 19 200, 38 400 and 115 200 baud
	(8E1) and confirm the enforced silent gaps meet or exceed the RTU minimums.
- Stress test with injected noise (e.g., UART glitch generator) and measure
	frame acceptance rate—it should stay above 99.9% across the baud points.
- Follow the step-by-step hardware playbook in ``hil_validation.md`` to record
	reproducible evidence for the Gate 20 sign-off checklist.

Once these tests pass, the port is ready for Gate 20 validation: run the
``modbus_rtu_loop_demo`` from the host examples side-by-side with the MCU
firmware and confirm frame sequencing under sustained bursts.
