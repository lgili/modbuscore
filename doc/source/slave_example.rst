Slave Example
=============

This example demonstrates how to set up and run a Modbus Slave (Server) using the **Modbus Master/Slave Library in C**. It includes initializing the slave context, configuring holding registers, and handling incoming requests.

```c
#include <modbus_slave.h>
#include <modbus_transport.h>
#include <modbus/utils.h>

// Define transport functions (e.g., UART)
void uart_write(uint8_t *data, uint16_t length) {
    // Implement UART write functionality
}

bool uart_read(uint8_t *data, uint16_t length) {
    // Implement UART read functionality
    return true;
}

uint16_t get_msec(void) {
    // Implement a function to get the current time in milliseconds
}

uint16_t measure_time(uint16_t start_time) {
    // Implement a function to measure elapsed time since start_time
    return get_msec() - start_time;
}

int main(void) {
    modbus_context_t ctx;
    modbus_transport_t transport = {
        .write = uart_write,
        .read = uart_read,
        .get_reference_msec = get_msec,
        .measure_time_msec = measure_time
    };
    
    uint16_t device_address = 1;
    uint16_t baudrate = 19200;
    
    // Initialize the Modbus Slave
    modbus_error_t error = modbus_server_create(&ctx, &transport, &device_address, &baudrate);
    if (error != MODBUS_ERROR_NONE) {
        // Handle initialization error
    }
    
    // Register holding registers
    int16_t reg1 = 100;
    int16_t reg2 = 200;
    modbus_set_holding_register(&ctx, 0x0000, &reg1, false, NULL, NULL);
    modbus_set_holding_register(&ctx, 0x0001, &reg2, false, NULL, NULL);
    
    // Add device information
    const char vendor_name[] = "ModbusMasterSlaveLibrary";
    modbus_server_add_device_info(&ctx, vendor_name, sizeof(vendor_name) - 1);
    
    // Main loop
    while (1) {
        modbus_server_poll(&ctx);
        // Other application tasks
    }
    
    return 0;
}
