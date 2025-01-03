// slave_example.c

#include <stdio.h>
#include <stdint.h>

#include "uart_windows.h"
#include <modbus/modbus.h>


uart_handle_t uart;
modbus_context_t ctx;
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
int32_t transport_write(const uint8_t *data, uint16_t length) {
    int result = uart_write(&uart, data, length);
    if (result < 0) {
        LOG(LOG_LEVEL_ERROR, "Falha ao escrever na UART.");
    } else {
        LOG(LOG_LEVEL_DEBUG, "Escrito %d bytes na UART.", result);
    }
    return result;
}

// Transport read function
int transport_read(uint8_t *buffer, uint16_t length) {
    return uart_read(&uart, buffer, length);
}

// fake interrupt
void uart_interrupt() {
    modbus_server_data_t *server = (modbus_server_data_t *)ctx.user_data;
    uint8_t data[64];
    // Simulate RX interrupt if waiting for a response    
    // Read data from mock transport's rx buffer
    uint8_t size_read = server->ctx->transport.read(data, 64); // Adjust based on your mock_transport
    if(size_read > 0) {
        for (size_t i = 0; i < size_read; i++) {
            // printf("[Info] Receiving data on uart\n");
            LOG(LOG_LEVEL_DEBUG, "Receiving data on uart\n");
            modbus_server_receive_data_from_uart_event(&server->fsm, data[i]);
        }
    }
    
}

// interrupt from uart
int on_byte_received(uint8_t data) {
    LOG(LOG_LEVEL_DEBUG, "Slave recebeu byte: %d", data);
    modbus_server_data_t *server = (modbus_server_data_t *)ctx.user_data;
    modbus_server_receive_data_from_uart_event(&server->fsm, data);
    return 0;
}

int main(void) {
    // Initialize logging
    printf("Initializing Modbus Slave Example...\n");

    // Initialize UART
    
    const char *com_port = "COM15"; // Replace with your COM port
    uint16_t baud_rate = 19200;

    if (uart_init(&uart, com_port, (int)baud_rate) != 0) {
        printf("[Error] UART initialization failed.\n");
        return 1;
    }
    uart_set_callback(&uart, on_byte_received);

    // Define transport with user_data as the UART handle
    modbus_transport_t transport = {
        .write = transport_write,
        .read  = transport_read,
        .get_reference_msec = get_current_time_ms,
        .measure_time_msec   = measure_elapsed_time,
    };

    // Initialize Modbus context
    
    uint16_t device_address = 1;

    modbus_error_t error = modbus_server_create(&ctx, &transport, &device_address, &baud_rate);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to initialize Modbus Slave. Error code: %d\n", error);
        uart_close(&uart);
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
    const char vendor_name[] = "Embraco_Modbus_Slave";
    error = modbus_server_add_device_info(&ctx, "SECOP", 5);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to add device information. Error code: %d\n", error);
    }

    printf("[Info] Device information added successfully.\n");

    // Main polling loop
    printf("[Info] Entering main polling loop. Press Ctrl+C to exit.\n");
    int count =0;
    while (1) {       
        //uart_interrupt();
        modbus_server_poll(&ctx);
        // Additional application tasks can be performed here
        Sleep(10); // Sleep to reduce CPU usage
    }

    // Cleanup (unreachable in this example)
    uart_close(&uart);
    return 0;
}
