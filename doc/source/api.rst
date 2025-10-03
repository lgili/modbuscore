API Reference
=============

.. doxygenindex::
   :project: modbus

Clients
-------

.. doxygenstruct:: mb_client
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

Servers
-------

.. doxygenstruct:: mb_server
   :project: modbus
   :members:

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
