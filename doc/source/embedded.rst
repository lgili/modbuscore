Embedded Integration Toolkit
=============================

This chapter tracks the assets under ``embedded/`` intended to streamline
firmware bring-up:

* **Quickstarts** for bare-metal, FreeRTOS, Zephyr, and ESP-IDF targets.
* **Vendor ports** that expose DMA + IDLE UART recipes, timers, and HAL glue.
* **Checklists and porting wizard** content copied from the roadmap.

The files are currently placeholders while Gates 17–20 are in progress, but
the directory structure and documentation references are ready for upcoming
drops.

.. todo::

	 Publish detailed quickstarts for:

	 * ``quickstarts/`` – bare-metal loop, FreeRTOS task wiring, Zephyr module,
		 and ESP-IDF component recipes.
	 * ``ports/stm32-ll-dma-idle/`` – circular DMA + IDLE line walkthrough.
	 * ``ports/nxp-lpuart-idle/`` – low-power friendly LPUART integration.

	 Each guide will link back to :doc:`install_use` once the assets are ready.
