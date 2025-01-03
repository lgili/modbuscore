#ifndef UART_WINDOWS_H
#define UART_WINDOWS_H

#include <stdint.h>
#include <windows.h>

// UART Handle structure

typedef struct {
    HANDLE hSerial;
    HANDLE hThread;
    int (*onDataReceived)(uint8_t data); // Callback para dado recebido
    volatile BOOL stopThread;            // Flag para sinalizar a thread a encerrar
} uart_handle_t;

// Initializes the UART and opens the specified COM port
// Returns 0 on success, non-zero on failure
int uart_init(uart_handle_t *uart, const char *port_name, int baud_rate);

// Writes data to the UART
// Returns number of bytes written, or -1 on failure
int uart_write(uart_handle_t *uart,const uint8_t *data, uint16_t length);

// Reads data from the UART
// Returns number of bytes read, or -1 on failure
int uart_read(uart_handle_t *uart, uint8_t *buffer, uint16_t length);

// Closes the UART
void uart_close(uart_handle_t *uart);

int uart_set_callback(uart_handle_t *uart, int (*callback)(uint8_t data)); // Configura callback

#endif // UART_WINDOWS_H
