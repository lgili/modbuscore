// slave_example.c

#include <stdio.h>
#include <stdint.h>

#include "uart_windows.h"
#include <modbus/modbus.h>
#include <modbus/mb_log.h>


uart_handle_t uart;
modbus_context_t ctx;
modbus_server_data_t server;
uint16_t baudrate = 19200;

void my_console_logger(mb_log_level_t severity, char *msg) {
    SYSTEMTIME t;
    GetSystemTime(&t); // or GetLocalTime(&t)
    // printf("The system time is: %02d:%02d:%02d.%03d\n", 
    //     t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
     printf("%02d:%02d:%02d.%03d [%s]: %s\n",
         t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,    // user defined function
         MB_LOG_LEVEL_NAME(severity),
         msg);
}

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
    MB_LOG_DEBUG("tamanho enviado para uart %d", length);
    int result = uart_write(&uart, data, length);
    if (result < 0) {
        MB_LOG_ERROR("Falha ao escrever na UART.");
    } else {
        MB_LOG_DEBUG("Escrito %d bytes na UART.", result);
    }
    modbus_server_data_t *server = (modbus_server_data_t *)ctx.user_data;
    //ctx.tx_reference_time = ctx.transport.get_reference_msec();
    
    return result;
}

// Transport read function
int transport_read(uint8_t *buffer, uint16_t length) {
    // int size  = uart_has_data(&uart);
    // if(size <= 0) return size;
    return uart_read(&uart, buffer, length);
}
int init_uart() {
    const char *com_port = "COM18"; // Replace with your COM port
    uint16_t baud_rate = baudrate;

    if (uart_init(&uart, com_port, (int)baud_rate) != 0) {
    MB_LOG_ERROR("UART initialization failed.\n");
        return 1;
    }
    return 0;
}

void restart_uart() {
    uart_close(&uart);
    init_uart();
}

// fake interrupt
void uart_interrupt() {
    modbus_server_data_t *server = (modbus_server_data_t *)ctx.user_data;
    uint8_t data[64];
    // Simulate RX interrupt if waiting for a response    
    // Read data from mock transport's rx buffer
    int size  = uart_has_data(&uart);
    if(size > 0) {
        uint8_t size_read = server->ctx->transport.read(data, 64); // get data from DMA buffer, here is a example
        modbus_server_receive_buffer_from_uart_event(&server->fsm, data, size_read);
        if(size_read > 0) {
            printf("[Info] Receiving data on uart\n");
            for (size_t i = 0; i < size_read; i++) {
                MB_LOG_DEBUG("Data %d", data[i]);
                // modbus_server_receive_data_from_uart_event(&server->fsm, data[i]);
            }
            printf("\n");
        }
    }
    
}

// interrupt from uart
int on_byte_received(uint8_t *data, uint16_t lenght) {
    MB_LOG_DEBUG("Slave recebeu byte: %d %d %d %d %d", data[0], data[1], data[2], data[3], data[4]);
    modbus_server_data_t *server = (modbus_server_data_t *)ctx.user_data;
    // modbus_server_receive_data_from_uart_event(&server->fsm, data);
    modbus_server_receive_buffer_from_uart_event(&server->fsm, data, lenght);
    return 0;
}

int main(void) {    
    MB_LOG_INIT();
    MB_LOG_SUBSCRIBE(my_console_logger, MB_LOG_TRACE_LEVEL);
    // LOG_TRACE("Critical, arg=%d", arg); 
    // LOG_DEBUG("Critical, arg=%d", arg);
    // LOG_INFO("Critical, arg=%d", arg);
    // LOG_WARNING("Critical, arg=%d", arg);
    // LOG_ERROR("Critical, arg=%d", arg);
    // LOG_CRITICAL("Critical, arg=%d", arg);
    
    // Initialize logging
    MB_LOG_INFO("Initializing Modbus Slave Example...\n");

    // Initialize UART
    init_uart();
    
    // uart_set_callback(&uart, on_byte_received);

    // Define transport with user_data as the UART handle
    modbus_transport_t transport = {
        .write = transport_write,
        .read  = transport_read,
        .get_reference_msec = get_current_time_ms,
        .measure_time_msec   = measure_elapsed_time,
    };

    // Initialize Modbus context
    
    uint16_t device_address = 1;

    modbus_error_t error = modbus_server_create(&ctx, &transport, &device_address, &baudrate, &server);
    if (error != MODBUS_ERROR_NONE) {
    MB_LOG_ERROR("Failed to initialize Modbus Slave. Error code: %d\n", error);
        uart_close(&uart);
        return 1;
    }

    MB_LOG_INFO("Modbus Slave initialized successfully.\n");

    // Register holding registers
    int16_t reg1 = 100;
    int16_t reg2 = 200;
    int16_t reg3 = 300;
    int16_t reg4 = 400;
    int16_t reg5 = 500;

    error = modbus_set_holding_register(&ctx, 0x0000, &reg1, true, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
    MB_LOG_ERROR("Failed to register holding register 0x0000. Error code: %d\n", error);
    }

    error = modbus_set_holding_register(&ctx, 0x0005, &reg1, true, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
    MB_LOG_ERROR("Failed to register holding register 0x0000. Error code: %d\n", error);
    }

    error = modbus_set_holding_register(&ctx, 0x0001, &reg2, false, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
    MB_LOG_ERROR("Failed to register holding register 0x0001. Error code: %d\n", error);
    }

    error = modbus_set_holding_register(&ctx, 0x0002, &reg3, false, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
    MB_LOG_ERROR("Failed to register holding register 0x0001. Error code: %d\n", error);
    }

    error = modbus_set_holding_register(&ctx, 0x0003, &reg4, false, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
    MB_LOG_ERROR("Failed to register holding register 0x0001. Error code: %d\n", error);
    }

    error = modbus_set_holding_register(&ctx, 0x0004, &reg5, false, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
    MB_LOG_ERROR("Failed to register holding register 0x0001. Error code: %d\n", error);
    }

    MB_LOG_INFO("Holding registers registered successfully.\n");

    // Add device information    
    error = modbus_server_add_device_info(&ctx, "SECOP", 5);
    if (error != MODBUS_ERROR_NONE) {
    MB_LOG_ERROR("Failed to add device information. Error code: %d\n", error);
    }

    MB_LOG_INFO("Device information added successfully.\n");

    // Main polling loop
    MB_LOG_INFO("Entering main polling loop. Press Ctrl+C to exit.\n");    
    while (1) {       
        uart_interrupt(); // manual calling
        modbus_server_poll(&ctx);
        // Additional application tasks can be performed here
        // Sleep(100); // Sleep to reduce CPU usage
    }

    // Cleanup (unreachable in this example)
    uart_close(&uart);
    return 0;
}
