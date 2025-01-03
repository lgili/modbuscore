API Reference
=============

This section provides comprehensive details about all the functions, structures, and enumerations available in the **Modbus Master/Slave Library in C**. It serves as a complete reference for developers to understand and utilize the library's capabilities effectively.

.. toctree::
   :maxdepth: 2
   :caption: API Reference Contents

   classes
   functions

## Overview

The **Modbus Master/Slave Library in C** offers a robust set of APIs designed to facilitate seamless Modbus communication in embedded systems. This reference is organized into two main categories:

- **Classes:** Detailed descriptions of all data structures used in the library.
- **Functions:** Comprehensive information on all available functions, including their purpose, parameters, return values, and usage examples.

## Classes

The `classes.rst` file contains detailed information about the primary data structures utilized within the library. These structures are fundamental to managing the state and configuration of both Master and Slave devices.

### Key Structures

- **modbus_context_t:** Represents the core context for Modbus operations, holding configuration and state information.
- **modbus_transport_t:** Defines the transport layer functions required for communication (e.g., read, write, timing functions).
- **modbus_client_data_t:** Manages client-specific data, including FSM state and read data buffers.

## Functions

The `functions.rst` file provides in-depth information about all the functions available in the library. Each function entry includes its purpose, parameters, return values, and example usage to assist developers in integrating Modbus functionalities into their projects.

### Categories of Functions

- **Modbus Slave (Server) Functions:**
  - **modbus_server_create:** Initializes the Modbus Slave context.
  - **modbus_server_poll:** Polls the server FSM to process incoming requests and send responses.
  - **modbus_set_holding_register:** Registers a single holding register.
  - **modbus_set_array_holding_register:** Registers an array of holding registers.
  - **modbus_server_add_device_info:** Adds device information for Read Device Information requests.

- **Modbus Master (Client) Functions:**
  - **modbus_client_create:** Initializes the Modbus Master context.
  - **modbus_client_poll:** Polls the Master FSM to handle requests and responses.
  - **modbus_client_read_holding_registers:** Sends a read holding registers request to a slave device.
  - **modbus_client_receive_data_event:** Feeds received bytes into the Master FSM.
  - **modbus_client_set_timeout:** Sets the response timeout for requests.
  - **modbus_client_get_read_data:** Retrieves data read from the last request.

### Example Function Documentation

#### `modbus_server_create`

**Description:**
Initializes the Modbus Slave context with the specified transport configurations, device address, and baud rate.

**Prototype:**
```c
modbus_error_t modbus_server_create(modbus_context_t *ctx, const modbus_transport_t *transport, uint16_t device_address, uint16_t baudrate);

Parameters:

    ctx: Pointer to the Modbus context to be initialized.
    transport: Pointer to the transport layer configuration.
    device_address: Modbus device address.
    baudrate: Communication baud rate.

Returns:

    MODBUS_ERROR_NONE on success.
    Appropriate error code on failure.

Example:

modbus_context_t ctx;
modbus_transport_t transport = {
    .write = uart_write_function,
    .read = uart_read_function,
    .get_reference_msec = get_msec,
    .measure_time_msec = measure_time
};

uint16_t device_address = 1;
uint16_t baudrate = 19200;

modbus_error_t error = modbus_server_create(&ctx, &transport, device_address, baudrate);
if (error != MODBUS_ERROR_NONE) {
    // Handle initialization error
}

modbus_client_read_holding_registers

Description: Sends a request to read holding registers from a specified slave device.

Prototype:

modbus_error_t modbus_client_read_holding_registers(modbus_context_t *ctx, uint8_t slave_id, uint16_t start_address, uint16_t quantity);

Parameters:

    ctx: Pointer to the Modbus Master context.
    slave_id: ID of the slave device to communicate with.
    start_address: Starting address of the holding registers to read.
    quantity: Number of holding registers to read.

Returns:

    MODBUS_ERROR_NONE on success.
    Appropriate error code on failure.

Example:

modbus_error_t error = modbus_client_read_holding_registers(&ctx, 0x01, 0x0000, 2);
if (error != MODBUS_ERROR_NONE) {
    // Handle request error
}

Navigation

Use the table of contents above to navigate through the API Reference. Click on the links under Classes and Functions to access detailed documentation for each component.

For further assistance or questions regarding the API, refer to the Installation and Examples sections of the documentation.