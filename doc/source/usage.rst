Usage
=====

Logging
-------

Gate 1 introduces the `mb_log.h` façade so applications rely on `MB_LOG_*`
macros instead of the legacy `LOG_*` names.  The helper below wires the
default stdout sink and keeps thresholds consistent across platforms.

.. code-block:: c

   #include <modbus/mb_log.h>

   void app_init_logging(void) {
       mb_log_bootstrap_defaults();
       MB_LOG_INFO("Modbus logging ready");
   }

Compile-time toggles control how the façade behaves.  They can be passed via
your build system (for example `-DMB_LOG_ENABLE_STDIO=0`) before including any
Modbus headers:

============================ ===============================================
Macro                         Purpose
============================ ===============================================
``MB_LOG_ENABLE_STDIO``       Enable the stdio sink (default ``1``)
``MB_LOG_ENABLE_SEGGER_RTT``  Enable the SEGGER RTT sink when the SDK is found
``MB_LOG_DEFAULT_THRESHOLD``  Set the initial minimum severity (default INFO)
``MB_LOG_STDOUT_SYNC_FLUSH``  Flush after each stdio log (default ``1``)
``MB_LOG_RTT_CHANNEL``        Select the RTT down-channel (default ``0``)
============================ ===============================================

With these defines you can, for example, ship a silent release build that only
logs to RTT when the SEGGER probe is present:

.. code-block:: c

   target_compile_definitions(app PRIVATE
       MB_LOG_ENABLE_STDIO=0
       MB_LOG_ENABLE_SEGGER_RTT=1
       MB_LOG_DEFAULT_THRESHOLD=MB_LOG_WARNING_LEVEL)

Utilities
---------

Gate 1 also adds heap-free utilities that can be mixed and matched depending on
the target.

Ring buffer (single producer / single consumer)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   #include <modbus/ringbuf.h>

   #define UART_RX_CAP 128U
   static uint8_t uart_rx_storage[UART_RX_CAP];
   static mb_ringbuf_t uart_rx_fifo;

   void uart_init_fifo(void)
   {
       MB_STATIC_ASSERT(MB_IS_POWER_OF_TWO(UART_RX_CAP), "capacity must be power of two");
       mb_ringbuf_init(&uart_rx_fifo, uart_rx_storage, UART_RX_CAP);
   }

   void uart_rx_isr(uint8_t byte)
   {
       mb_ringbuf_push(&uart_rx_fifo, byte);
   }

   bool uart_rx_pop(uint8_t *out)
   {
       return mb_ringbuf_pop(&uart_rx_fifo, out);
   }

Fixed-size memory pool
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   #include <modbus/mempool.h>

   enum { BLOCKS = 8, BLOCK_BYTES = 32 };
   static uint8_t pool_storage[BLOCKS][MB_ALIGN_UP(BLOCK_BYTES, sizeof(void*))];
   static mb_mempool_t pool;

   void app_pool_init(void)
   {
       mb_mempool_init(&pool, pool_storage, sizeof pool_storage[0], BLOCKS);
   }

   void *app_alloc(void)
   {
       return mb_mempool_acquire(&pool);
   }

   void app_free(void *ptr)
   {
       mb_mempool_release(&pool, ptr);
   }

PDU helpers (Gate 2)
--------------------

The `modbus/pdu.h` façade keeps the Modbus protocol codecs in one place.  Gate 2
starts with Function Codes 0x03, 0x06 and 0x10, covering register reads and
writes.

.. code-block:: c

   #include <modbus/pdu.h>

   mb_u8 request[MB_PDU_MAX];
   mb_pdu_build_read_holding_request(request, sizeof request, 0x0000, 4);

   // later, parse the response
   const mb_u8 *payload = NULL;
   mb_u16 register_count = 0;
   if (mb_pdu_parse_read_holding_response(response_pdu, response_len,
                                          &payload, &register_count) == MODBUS_ERROR_NONE) {
       // payload now points to 2 * register_count bytes with big-endian values
   }

For robustness testing, enable the optional libFuzzer target with
``-DMODBUS_BUILD_FUZZERS=ON`` (Clang required).  The harness exercises each
decoder (`mb_pdu_parse_*`) continuously, catching malformed frames early in the
pipeline.

Transport abstraction (Gate 3)
------------------------------

Gate 3 introduces a compact, non-blocking transport interface that both client
and server code can depend on without pulling in platform specifics.  The
``mb_transport_if_t`` façade groups ``send``/``recv`` callbacks together with a
monotonic ``now_ms`` helper and an optional ``yield`` hook.  Mock transports used
by the tests are also exposed through this interface, making it trivial to wire
integration scenarios.

.. code-block:: c

   #include <modbus/transport_if.h>
   #include <modbus/frame.h>

   static mb_err_t uart_send(void *ctx, const mb_u8 *buf, mb_size_t len,
                             mb_transport_io_result_t *out)
   {
       (void)ctx;
       const int32_t written = platform_uart_write(buf, (uint16_t)len);
       if (written < 0) {
           return MODBUS_ERROR_TRANSPORT;
       }
       if (out) {
           out->processed = (mb_size_t)written;
       }
       return (written == (int32_t)len) ? MODBUS_ERROR_NONE : MODBUS_ERROR_TRANSPORT;
   }

   static mb_err_t uart_recv(void *ctx, mb_u8 *buf, mb_size_t cap,
                             mb_transport_io_result_t *out)
   {
       (void)ctx;
       const int32_t read_count = platform_uart_read(buf, (uint16_t)cap);
       if (read_count < 0) {
           return MODBUS_ERROR_TRANSPORT;
       }
       if (out) {
           out->processed = (mb_size_t)read_count;
       }
       return (read_count > 0) ? MODBUS_ERROR_NONE : MODBUS_ERROR_TIMEOUT;
   }

   static mb_time_ms_t app_now(void *ctx)
   {
       (void)ctx;
       return platform_monotonic_ticks();
   }

   static const mb_transport_if_t uart_iface = {
       .ctx = NULL,
       .send = uart_send,
       .recv = uart_recv,
       .now = app_now,
       .yield = NULL,
   };

   // Encode a PDU into an RTU frame and push it through the transport
   const mb_u8 pdu_payload[] = {0x00, 0x02};
   const mb_adu_view_t adu = {
       .unit_id = 1,
       .function = MB_PDU_FC_READ_HOLDING_REGISTERS,
       .payload = pdu_payload,
       .payload_len = sizeof pdu_payload,
   };
   mb_u8 adu_buffer[16];
   mb_size_t adu_len = 0U;
   if (mb_frame_rtu_encode(&adu, adu_buffer, sizeof adu_buffer, &adu_len) == MODBUS_ERROR_NONE) {
       mb_transport_send(&uart_iface, adu_buffer, adu_len, NULL);
   }

``mb_frame_rtu_decode`` performs the inverse operation, validating the CRC and
returning an ``mb_adu_view_t`` that points directly into the received buffer.

## Initialization

Before utilizing the Master or Slave functionalities, it is essential to initialize the corresponding context with the appropriate transport configurations and device settings.

### Initializing the Modbus Slave

To initialize the Modbus Slave (Server), follow these steps:

```c
#include <modbus_slave.h>
#include <modbus_transport.h>

// Define transport functions
modbus_transport_t transport = {
    .write = uart_write_function,         // User-defined write function
    .read = uart_read_function,           // User-defined read function
    .get_reference_msec = get_msec,       // Function to get current time in ms
    .measure_time_msec = measure_time     // Function to measure elapsed time
};

// Device address and baud rate
uint16_t device_address = 1;
uint16_t baudrate = 19200;

// Modbus context
modbus_context_t ctx;

// Initialize the Modbus Slave
modbus_error_t error = modbus_slave_create(&ctx, &transport, &device_address, &baudrate);
if (error != MODBUS_ERROR_NONE) {
    // Handle initialization error
}


### Initializing the Modbus Master

To initialize the Modbus Master (Client), follow these steps:

```c
#include <modbus_master.h>
#include <modbus_transport.h>

// Define transport functions
modbus_transport_t transport = {
    .write = uart_write_function,         // User-defined write function
    .read = uart_read_function,           // User-defined read function
    .get_reference_msec = get_msec,       // Function to get current time in ms
    .measure_time_msec = measure_time     // Function to measure elapsed time
};

// Baud rate
uint16_t baudrate = 19200;

// Modbus context
modbus_context_t ctx;

// Initialize the Modbus Master
modbus_error_t error = modbus_master_create(&ctx, &transport, &baudrate);
if (error != MODBUS_ERROR_NONE) {
    // Handle initialization error
}


### Configuring the Modbus Slave

After initialization, configure the Modbus Slave by registering holding registers and adding device information.
#### Registering Holding Registers

You can register single or multiple holding registers that the Modbus Slave will manage.
Register a Single Holding Register

int16_t register_value = 1234;
modbus_error_t error = modbus_set_holding_register(&ctx, 0x0000, &register_value, false, NULL, NULL);
if (error != MODBUS_ERROR_NONE) {
    // Handle registration error
}

Register an Array of Holding Registers

int16_t register_array[10] = {0};
modbus_error_t error = modbus_set_array_holding_register(&ctx, 0x0010, 10, register_array, false, NULL, NULL);
if (error != MODBUS_ERROR_NONE) {
    // Handle registration error
}

Adding Device Information

Provide device information to support Function Code 0x2B for device information queries.
const char vendor_name[] = "ModbusMasterSlaveLibrary";
modbus_error_t error = modbus_slave_add_device_info(&ctx, vendor_name, sizeof(vendor_name) - 1);
if (error != MODBUS_ERROR_NONE) {
    // Handle addition error
}


Configuring the Modbus Master

After initialization, configure the Modbus Master by setting timeouts and sending read requests.
Setting Response Timeout

Define the timeout period for responses from Slave devices.
modbus_master_set_timeout(&ctx, 500); // 500 ms

Sending Read Holding Registers Request

Send a request to read holding registers from a Slave device.

modbus_error_t error = modbus_master_read_holding_registers(&ctx, 0x01, 0x0000, 2);
if (error != MODBUS_ERROR_NONE) {
    // Handle request error
}

Running the Main Loop

Both Master and Slave need to execute a polling loop to handle incoming and outgoing communication.
Modbus Slave Main Loop

while (1) {
    modbus_slave_poll(&ctx);
    // Other application tasks
}

Modbus Master Main Loop

while (1) {
    modbus_master_poll(&ctx);
    
    if (ctx.client_data.read_data_count > 0) {
        int16_t data_buffer[2];
        uint16_t regs_read = modbus_master_get_read_data(&ctx, data_buffer, 2);
        if (regs_read > 0) {
            // Process the read data
        }
    }
    
    // Other application tasks
}

Handling Received Data

Implement a function to handle bytes received from the transport layer (e.g., UART interrupt) and feed them into the Modbus FSM.
Modbus Slave Data Reception

void uart_receive_byte(uint8_t byte) {
    modbus_slave_receive_data_event(&ctx.fsm, byte);
}

Modbus Master Data Reception

void uart_receive_byte(uint8_t byte) {
    modbus_master_receive_data_event(&ctx.client_data.fsm, byte);
}

Summary

This section provides detailed instructions on how to initialize, configure, and utilize both Master and Slave functionalities of the Modbus Master/Slave Library in C. By following these steps, you can integrate Modbus communication seamlessly into your embedded systems projects.