// slave_tcp_example.c

#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include "tcp_windows.h"
#include <modbus/modbus.h>

tcp_handle_t tcp;
// Function to get the current time in milliseconds
uint16_t get_current_time_ms(void) {
    return (uint16_t)(GetTickCount() & 0xFFFF);
}

// Function to measure elapsed time since start_time
uint16_t measure_elapsed_time(uint16_t start_time) {
    uint16_t current_time = get_current_time_ms();
    return (current_time - start_time) & 0xFFFF;
}

// Transport write function
int transport_write(uint8_t *data, uint16_t length) {
    tcp_handle_t *tcp = (tcp_handle_t *)transport->user_data;
    return tcp_write(&tcp, data, length);
}

// Transport read function
int transport_read(uint8_t *buffer, uint16_t length) {
    return tcp_read(&tcp, buffer, length);
}

int main(void) {
    // Initialize logging
    printf("Initializing Modbus Slave TCP Example...\n");

    // Initialize TCP
    int port = 502; // Standard Modbus TCP port

    if (tcp_init(&tcp, port) != 0) {
        printf("[Error] TCP initialization failed.\n");
        return 1;
    }

    // Define transport with user_data as the TCP handle
    modbus_transport_t transport = {
        .write = transport_write,
        .read  = transport_read,
        .get_reference_msec = get_current_time_ms,
        .measure_time_msec   = measure_elapsed_time,
    };

    // Initialize Modbus context
    modbus_context_t ctx;
    uint16_t device_address = 1; // Not used in Modbus TCP, but kept for consistency

    modbus_error_t error = modbus_slave_create(&ctx, &transport, device_address, port);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to initialize Modbus Slave. Error code: %d\n", error);
        tcp_close(&tcp);
        return 1;
    }

    printf("[Info] Modbus Slave initialized successfully.\n");

    // Register holding registers
    int16_t reg1 = 100;
    int16_t reg2 = 200;

    error = modbus_set_holding_register(&ctx, 0x0000, &reg1, false, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to register holding register 0x0000. Error code: %d\n", error);
    }

    error = modbus_set_holding_register(&ctx, 0x0001, &reg2, false, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to register holding register 0x0001. Error code: %d\n", error);
    }

    printf("[Info] Holding registers registered successfully.\n");

    // Add device information
    const char vendor_name[] = "Embraco_Modbus_Slave_TCP";
    error = modbus_slave_add_device_info(&ctx, vendor_name, sizeof(vendor_name) - 1);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to add device information. Error code: %d\n", error);
    }

    printf("[Info] Device information added successfully.\n");

    // Main polling loop
    printf("[Info] Entering main polling loop. Press Ctrl+C to exit.\n");
    while (1) {
        modbus_slave_poll(&ctx);
        // Additional application tasks can be performed here
        Sleep(10); // Sleep to reduce CPU usage
    }

    // Cleanup (unreachable in this example)
    tcp_close(&tcp);
    return 0;
}
