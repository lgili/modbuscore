Usage
=====

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