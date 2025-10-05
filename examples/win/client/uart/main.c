/* Legacy Windows Modbus sample removed. Use the cross-platform demos under `examples/`. */

#if 0
// master_rtu_example.c

#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include "uart_windows.h"
#include <modbus/modbus.h>
#include <modbus/mb_log.h>

uart_handle_t uart;
modbus_context_t ctx;
modbus_client_data_t client;

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
    int result = uart_write(&uart, data, length);
    if (result < 0) {
        MB_LOG_ERROR("Falha ao escrever na UART.");
    } else {
        MB_LOG_DEBUG("Escrito %d bytes na UART.", result);
    }
    return result;
}

// Transport read function
int transport_read(uint8_t *buffer, uint16_t length) {
    int size  = uart_has_data(&uart);
    if(size <= 0) return size;
    return uart_read(&uart, buffer, length);
}

// fake interrupt
void uart_interrupt() {
    modbus_client_data_t *client = (modbus_client_data_t *)ctx.user_data;
    uint8_t data[64];
    // Simulate RX interrupt if waiting for a response    
    // Read data from mock transport's rx buffer
    uint8_t size_read = client->ctx->transport.read(data, 64); // Adjust based on your mock_transport
    if(size_read > 0) {
    MB_LOG_DEBUG("Receiving data on uart. State %s\n", client->fsm.current_state->name);
        for (size_t i = 0; i < size_read; i++) {
            modbus_client_receive_data_event(&client->fsm, data[i]);
        }
    }
    
}

// interrupt from uart
int on_byte_received(uint8_t data) {
    MB_LOG_DEBUG("Master recebeu byte: %d", data);
    modbus_client_data_t *client = (modbus_client_data_t *)ctx.user_data;
    modbus_client_receive_data_event(&client->fsm, data);
    return 0;
}

int main(void) {
    return 0;
}

#endif /* LEGACY_WINDOWS_MODBUS_CLIENT */
