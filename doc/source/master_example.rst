Master Example
==============

This example demonstrates how to set up and run a Modbus Master (Client) using the **Modbus Master/Slave Library in C**. It includes initializing the master context, sending read requests to the slave, and processing responses.

```c
#include <modbus_master.h>
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

// Function to handle received bytes (e.g., from UART interrupt)
void uart_receive_byte(uint8_t byte) {
    modbus_client_receive_data_event(&ctx.client_data.fsm, byte);
}

int main(void) {
    modbus_context_t ctx;
    modbus_transport_t transport = {
        .write = uart_write,
        .read = uart_read,
        .get_reference_msec = get_msec,
        .measure_time_msec = measure_time
    };
    
    uint16_t baudrate = 19200;
    
    // Initialize the Modbus Master
    modbus_error_t error = modbus_client_create(&ctx, &transport, &baudrate);
    if (error != MODBUS_ERROR_NONE) {
        // Handle initialization error
    }
    
    // Set response timeout
    modbus_client_set_timeout(&ctx, 500); // 500 ms
    
    // Send a read holding registers request
    modbus_client_read_holding_registers(&ctx, 0x01, 0x0000, 2);
    
    // Main loop
    while (1) {
        modbus_client_poll(&ctx);
        
        if (ctx.client_data.read_data_count > 0) {
            int16_t data_buffer[2];
            uint16_t regs_read = modbus_client_get_read_data(&ctx, data_buffer, 2);
            if (regs_read > 0) {
                // Process the read data
            }
        }
        
        // Other application tasks
    }
    
    return 0;
}
