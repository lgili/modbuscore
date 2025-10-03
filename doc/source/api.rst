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

.. doxygenfunction:: mb_server_init
   :project: modbus

.. doxygenfunction:: mb_server_add_storage
   :project: modbus

.. doxygenfunction:: mb_server_poll
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
