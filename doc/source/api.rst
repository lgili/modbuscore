API Reference
=============

.. doxygenindex::
   :project: modbus

Clients
-------

.. doxygenstruct:: mb_client
   :project: modbus
   :members:

.. doxygenstruct:: mb_client_metrics_t
   :project: modbus
   :members:

.. doxygenfunction:: mb_client_init
   :project: modbus

.. doxygenfunction:: mb_client_init_tcp
   :project: modbus

.. doxygenfunction:: mb_client_submit
   :project: modbus

.. doxygenfunction:: mb_client_poll
   :project: modbus

.. doxygenfunction:: mb_client_get_metrics
   :project: modbus

.. doxygenfunction:: mb_client_reset_metrics
   :project: modbus

.. doxygenfunction:: mb_client_get_diag
   :project: modbus

.. doxygenfunction:: mb_client_reset_diag
   :project: modbus

.. doxygenfunction:: mb_client_set_event_callback
   :project: modbus

.. doxygenfunction:: mb_client_set_trace_hex
   :project: modbus

Servers
-------

.. doxygenstruct:: mb_server
   :project: modbus
   :members:

.. doxygenenum:: mb_server_state_t
   :project: modbus

.. doxygenstruct:: mb_server_request
   :project: modbus
   :members:

.. doxygenstruct:: mb_server_metrics_t
   :project: modbus
   :members:

.. doxygenfunction:: mb_server_init
   :project: modbus

.. doxygenfunction:: mb_server_add_storage
   :project: modbus

.. doxygenfunction:: mb_server_poll
   :project: modbus

.. doxygenfunction:: mb_server_set_queue_capacity
   :project: modbus

.. doxygenfunction:: mb_server_queue_capacity
   :project: modbus

.. doxygenfunction:: mb_server_set_fc_timeout
   :project: modbus

.. doxygenfunction:: mb_server_pending
   :project: modbus

.. doxygenfunction:: mb_server_is_idle
   :project: modbus

.. doxygenfunction:: mb_server_submit_poison
   :project: modbus

.. doxygenfunction:: mb_server_get_metrics
   :project: modbus

.. doxygenfunction:: mb_server_reset_metrics
   :project: modbus

.. doxygenfunction:: mb_server_inject_adu
   :project: modbus

.. doxygenfunction:: mb_server_get_diag
   :project: modbus

.. doxygenfunction:: mb_server_reset_diag
   :project: modbus

.. doxygenfunction:: mb_server_set_event_callback
   :project: modbus

.. doxygenfunction:: mb_server_set_trace_hex
   :project: modbus

Observability
-------------

.. doxygenstruct:: mb_event
   :project: modbus
   :members:

.. doxygenenum:: mb_event_source_t
   :project: modbus

.. doxygenenum:: mb_event_type_t
   :project: modbus

.. doxygenstruct:: mb_diag_counters_t
   :project: modbus
   :members:

.. doxygenenum:: mb_diag_err_slot_t
   :project: modbus

.. doxygenfunction:: mb_diag_reset
   :project: modbus

.. doxygenfunction:: mb_diag_record_fc
   :project: modbus

.. doxygenfunction:: mb_diag_record_error
   :project: modbus

.. doxygenfunction:: mb_diag_err_slot_str
   :project: modbus

Transports
----------

.. doxygenstruct:: mb_rtu_transport
   :project: modbus
   :members:

.. doxygenstruct:: mb_tcp_transport
   :project: modbus
   :members:

.. doxygenstruct:: mb_tcp_multi_transport
   :project: modbus
   :members:
