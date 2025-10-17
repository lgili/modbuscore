# Quickstart playbooks

Gate 17 bundles a full set of entry points so firmware teams can embed the
Modbus client without touching the rest of the source tree. Everything under
`embedded/quickstarts/` assumes the amalgamated drop-in pair is available and
leans on the helpers from `modbus/mb_embed.h`.

## What’s ready today

- [`drop_in/`](./drop_in/) – single translation unit (`modbus_amalgamated.c/.h`)
	containing the client FSM, RTU transport and supporting utilities. Copy two
	files into any build system and you are ready to compile.
- [`components/esp-idf/modbus/`](./components/esp-idf/modbus/) – ESP-IDF
	component that wires the drop-in client to the UART driver with Kconfig
	toggles for pins, baud rate and queue depth.
- [`components/zephyr/`](./components/zephyr/) – Zephyr module with a
	sockets-based Modbus/TCP transport and an example `hello_tcp` sample.
- [`ports/stm32-ll-dma-idle/`](./ports/stm32-ll-dma-idle/) – STM32 reference
	glue for circular DMA reception plus IDLE-line framing.
- [`ports/nxp-lpuart-idle/`](./ports/nxp-lpuart-idle/) – NXP LPUART helper tuned
	for MCUXpresso-style projects and IDLE detection.
- [`ports/renesas-rl78-sci/`](./ports/renesas-rl78-sci/) – Renesas RL78 SCI
	adapter targeting Code Generator projects with half-duplex RS-485 links.

Each quickstart README walks through setup, ISR hooks and request helpers while
the [porting wizard](../porting-wizard.md) keeps the broader checklist in one
place.

## Working with the bundle

1. Regenerate the amalgamated drop-in whenever you change the core library:

	 ```sh
	 python3 scripts/amalgamate.py
	 ```

2. Pick the quickstart that matches your firmware stack and follow the local
	 README to supply timers, UART handles and DE/RE GPIO hooks.
3. Use the `mb_embed_*` helpers to queue requests while staying within the
	 non-blocking client API.

## Validation status

- ✅ Drop-in pair builds with the host presets and serves as the foundation for
	every quickstart.
- ✅ ESP-IDF component and Zephyr module configure and build following their
	step-by-step guides (hardware smoke tests tracked separately).
- ✅ STM32, NXP and Renesas ports demonstrate DMA/IDLE-style transports with
	ready-to-wire callbacks.
- ⏳ Hardware validation: run the ESP-IDF hello-RTU example and Zephyr
	hello-TCP sample on target boards, capturing notes for the Gate 17 report.

Follow the individual READMEs for deeper integration tips and next steps.
