Reference Examples
==================

The ``examples`` directory contains self-contained applications covering
different transports and operating systems.  All examples build against the
library headers and are exercised in CI.

POSIX
-----

* ``tcp_client_cli.c`` – interactive Modbus TCP client CLI.
* ``rtu_bridge.c`` *(if enabled)* – serial-to-TCP bridge demonstrating mixed
  transports.

Build using the ``host-debug-examples`` preset:

.. code-block:: bash

   cmake --preset host-debug-examples
   cmake --build --preset host-debug-examples --target tcp_client_cli

FreeRTOS (Gate 31)
------------------

``examples/freertos_rtu_client`` is a reference RTU client running on
FreeRTOS/ESP32.  It exercises the FreeRTOS transport helper and idle callbacks.

* ``main.c`` – application entry point
* ``FreeRTOSConfig.h`` – tuned configuration (tickless idle compatible)

Use the vendor toolchain (idf.py) or the provided CMake toolchain files to
compile and flash.

Zephyr (Gate 30)
---------------

``examples/zephyr_rtu_client`` targets Nordic and STM32 development kits.  Board
overlays configure UART, GPIO and timing.  Build with west:

.. code-block:: bash

   west build -b nucleo_f411re examples/zephyr_rtu_client

Interop / Benchmarks (Gate 32)
------------------------------

* ``interop/`` – scripts and fixtures used to test against third-party Modbus
  stacks (``modpoll``, ``mbtget``).  The GitHub Actions workflow runs these tests
  in Docker.
* ``benchmarks/`` – contains latency/bandwidth micro-benchmarks.  Enable with
  ``-DMODBUS_ENABLE_BENCHMARKS=ON``.

Quick Test Loop
---------------

To run the CLI client against the mock server:

.. code-block:: bash

   cmake --build --preset host-debug --target test_modbus_server
   ./build/host-debug/tests/test_modbus_server  # starts the mock server
   ./build/host-debug/examples/tcp_client_cli --host 127.0.0.1 --port 1502 \
       --unit 1 --register 0 --count 4

Refer to :doc:`transports` for details on hooking real UART or TCP endpoints.
