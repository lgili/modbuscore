Migration guide
===============

Guidance for keeping projects up to date with the 1.x series. Each section lists
behavioural changes, API adjustments, and the recommended steps to adopt them.

.. contents:: Version bumps
   :local:

Upgrading to 1.0.0 (from 0.x)
-----------------------------

The 1.0.0 release finalises the Gate roadmap, promotes semantic versioning, and
locks in the public API surface. Expecting source compatibility, but a few
categories require manual attention:

Observability layer
^^^^^^^^^^^^^^^^^^^

* New optional header ``modbus/observe.h`` exposes instrumentation hooks. Link
  ``modbus_observe`` if you consume ``mb_event_t`` helpers. Existing builds that
  do not enable events require no changes.
* ``mb_server_set_event_callback`` and ``mb_client_set_event_callback`` extend the
  server/client surfaces. If you previously registered logging callbacks through
  custom wrappers, switch to the built-in hook to avoid duplicate notifications.

Diagnostics counters
^^^^^^^^^^^^^^^^^^^^

* ``mb_diag_counters_t`` is now iterable through ``mb_server_get_diag`` and
  ``mb_client_get_diag``. The structure grew new fields for transport specific
  errors (timeouts, parser faults). Initialising the struct to zero and calling
  ``mb_*_reset_diag`` is the recommended pattern when sampling.

Configuration knobs
^^^^^^^^^^^^^^^^^^^

* ``mb_client_init_tcp`` and ``mb_server_init_tcp`` macros are deprecated. Use
  ``mb_client_init``/``mb_server_init`` with the transport returned by
  ``mb_tcp_iface`` or ``mb_tcp_multi_iface``.
* ``mb_client_set_watchdog`` now takes milliseconds instead of ticks. If you
  were passing RTOS ticks, multiply by the tick period (``portTICK_PERIOD_MS``).

Transport helpers
^^^^^^^^^^^^^^^^^

* Gate 10 introduced convenience factories under :doc:`ports`. Align your code
  towards those helpers (``mb_port_posix_serial_config`` et al.) instead of
  constructing ``mb_transport_if_t`` manually.
* Bare-metal builds may need the new ``MB_TRANSPORT_IF_STATIC`` macro to keep
  the interface in flash across modules.

Testing layout
^^^^^^^^^^^^^^

* The `tests/` tree now contains dedicated integration harnesses. To keep your
  CI configuration working, switch to the CTest targets: ``ctest -R core`` for
  unit suites, ``ctest -R integ`` for client/server transport checks.

Build-system updates
^^^^^^^^^^^^^^^^^^^^

* ``target_link_libraries`` entries should prefer the new interface target
  ``modbus::modbus`` in lieu of the legacy ``modbus`` alias. This ensures the
  include directories and compile definitions flow correctly.
* CMake requires 3.20 now. Regenerate your build directories when bumping.

Compatibility helpers (Gate 15)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Replace `modbus_mapping_new_start_address` usage with
  :c:func:`mb_server_mapping_init` or :c:func:`mb_server_mapping_apply`. The new
  helpers accept user-provided storage (heap-free by default) and wire regions
  into the cooperative server in one call.
* Installation exports now include both ``ModbusConfig.cmake`` (for
  ``find_package``) and ``modbus.pc`` (for ``pkg-config``). Update build scripts
  to prefer the upstream packages over hand-rolled include/lib hints.
* The `modbus_unit_test_loop_demo` example mirrors libmodbus' unit-test client
  and server flows, easing parity testing when porting applications.

Adoption checklist
^^^^^^^^^^^^^^^^^^

* [ ] Regenerate build files using CMake >= 3.20.
* [ ] Update transport initialisation to use the Gate 10 helpers if applicable.
* [ ] Adjust watchdog timeout units to milliseconds.
* [ ] Link against ``modbus::modbus`` and ``modbus::observe`` where needed.
* [ ] Integrate diagnostics sampling or explicitly reset counters at boot.
* [ ] Enable event callbacks to replace ad-hoc tracing glue.
