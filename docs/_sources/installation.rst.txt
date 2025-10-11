Installation
============

.. note::

   This page focuses on the current "build from source" workflow.  For a
   bird's-eye view, including placeholders for upcoming prebuilt archives and
   package feeds, see :doc:`install_use`.

Prerequisites
-------------

* C11-capable toolchain (tested with GCC, Clang, MSVC, MinGW).
* CMake 3.20 or newer.
* Ninja (recommended) or another generator.
* Optional: Doxygen + Sphinx for documentation, sanitizers, etc.

Build Presets
-------------

Use the provided CMake presets (``CMakePresets.json``):

* ``host-debug`` – Debug build with unit tests.
* ``host-release`` – Optimised build without tests.
* ``host-asan`` – Debug build with Address/Undefined sanitizers.
* ``mingw-*`` – Windows cross builds via MinGW.
* ``docs`` – Documentation (Doxygen + Sphinx).

Example (Linux/macOS):

.. code-block:: bash

   cmake --preset host-debug
   cmake --build --preset host-debug
   ctest --output-on-failure --test-dir build/host-debug

To generate docs:

.. code-block:: bash

   cmake --preset host-docs
   cmake --build --preset docs

Installation
------------

After building, install by running:

.. code-block:: bash

   cmake --install build/host-debug --prefix /desired/install/prefix

Adjust the preset (``host-release``) and prefix as required.

Using the installed package
---------------------------

The install step exports both CMake and pkg-config metadata:

* ``find_package(ModbusCore CONFIG REQUIRED)`` yields the ``Modbus::modbus``
   target. Linking against it automatically propagates include directories and
   compile definitions.
* ``pkg-config --cflags --libs modbuscore`` prints the compiler and linker flags
   for build systems that rely on pkg-config.

Generated files live under ``${prefix}/lib/cmake/Modbus`` and
``${prefix}/lib/pkgconfig``. Custom install prefixes are honoured.
:orphan:
