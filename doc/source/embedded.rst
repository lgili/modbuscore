Embedded Integration Toolkit
=============================

This chapter tracks the assets under ``embedded/`` intended to streamline
firmware bring-up:

* **Quickstarts** for bare-metal, FreeRTOS, Zephyr, and ESP-IDF targets.
* **Vendor ports** that expose DMA + IDLE UART recipes, timers, and HAL glue.
* **Checklists and porting wizard** content copied from the roadmap.

Gate 17 already ships the drop-in amalgamated sources plus ESP-IDF and Zephyr
components.  Bare-metal and vendor-specific ports remain in-flight under the
next milestones.

.. todo::

	 Expand the quickstarts with bare-metal loop and FreeRTOS task recipes and
	 publish the vendor HAL ports:

	 * ``quickstarts/`` – bare-metal loop and FreeRTOS task wiring examples.
	 * ``ports/stm32-ll-dma-idle/`` – circular DMA + IDLE line walkthrough.
	 * ``ports/nxp-lpuart-idle/`` – low-power friendly LPUART integration.

	 Each guide will link back to :doc:`install_use` once the assets are ready.
