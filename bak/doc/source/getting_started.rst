Getting Started
===============

Prerequisites
-------------

* CMake 3.20 or newer (presets are provided).
* A C11-capable toolchain.  The CI matrix covers GCC 12, Clang 17 and MSVC
  19.44.
* Ninja (recommended) or any other CMake generator.
* Python 3.8+ with ``sphinx`` if you want to build the HTML documentation.

Optional tooling:

* ``clang-format`` and ``clang-tidy`` for style and static analysis.
* ``lcov``/``genhtml`` to generate coverage reports.

Obtaining the Sources
---------------------

Clone the repository and initialise submodules:

.. code-block:: bash

   git clone https://github.com/lgili/modbus.git
   cd modbus
   git submodule update --init --recursive

Out-of-tree builds are recommended.  All CMake commands below assume the
workspace root (``modbus/``) as the working directory.

Building the Library
--------------------

Use the provided presets to configure and build:

.. code-block:: bash

   cmake --preset host-debug
   cmake --build --preset host-debug
   ctest --preset all --output-on-failure

The ``host-debug`` preset enables both client and server, RTU and TCP transports,
and the complete test suite.  Other useful presets:

* ``host-release`` – optimised build without tests.
* ``host-asan`` – AddressSanitizer + UndefinedBehaviourSanitizer.
* ``docs`` – builds the HTML documentation (``build/docs/html``).

Minimal Client
--------------

The snippet below demonstrates how to enqueue a Modbus TCP request using the
POSIX transport helper.

.. literalinclude:: ../../examples/tcp_client_cli.c
   :language: c
   :lines: 1-120

Key points:

1. ``mb_port_posix_tcp_client`` creates a non-blocking socket and returns an
   :c:type:`mb_transport_if_t` wrapper.
2. ``mb_client_init_tcp`` initialises the client with a transaction pool.
3. ``mb_client_submit`` enqueues the request; ``mb_client_poll`` drives the FSM.

The library ships with ready-to-run examples under ``examples/`` – see
:doc:`examples` for details.

Working on Windows
------------------

MSVC is fully supported.  Use the ``host-msvc`` preset (available via CMake) or
configure manually:

.. code-block:: powershell

   cmake --preset msvc-debug
   cmake --build --preset msvc-debug
   ctest --output-on-failure --preset all

When building inside GitHub Actions, the pipeline uses the same presets.  The
Windows port file (``modbus/src/port/win.c``) provides the required
implementations for ``mb_transport_if_t``.

Next Steps
----------

* Customise compile-time options: :doc:`configuration`
* Choose the appropriate transport/port: :doc:`transports`
* Enable diagnostics, tracing and telemetry: :doc:`diagnostics`

With these blocks in place you can tailor the library to your specific device
profile or gateway role.
