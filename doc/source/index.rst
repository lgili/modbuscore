ModbusCore
==========

**The Core of Modern Modbus Communication**

ModbusCore is a production-ready Modbus protocol stack designed for embedded
systems. From tiny 8KB microcontrollers to powerful gateways, ModbusCore adapts
to your needs with configurable profiles (TINY, LEAN, FULL).

This modern, zero-allocation implementation of Modbus RTU, TCP, and ASCII
provides non-blocking clients and servers, transport adapters for a variety of
targets, observability hooks, and compile-time configurability with Kconfig/menuconfig
support.

.. _doc-quick-links:

Quick Links
-----------

* :doc:`getting_started` – install, build presets, and a minimal client example.
* :doc:`configuration` – reference for the ``MB_CONF_*`` options and gate map.
* :doc:`transports` – how to bind RTU/TCP/ASCII and the provided POSIX, FreeRTOS
  and bare-metal ports.
* :doc:`diagnostics` – metrics, event callbacks, trace facilities, and queue
  introspection.
* :doc:`examples` – reference applications for RTU/TCP across Zephyr, FreeRTOS
  and POSIX hosts.

.. toctree::
   :maxdepth: 2
   :caption: User Guide

   overview
   getting_started
   configuration
   transports
   diagnostics
   power_management
   examples
   advanced_topics

.. toctree::
   :maxdepth: 2
   :caption: Developer Reference

   developer_guide
   api
   roadmapping
   misra
   contributing
   migration
   cookbook
   license

Indices and tables
------------------

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
