// master_rtu_example.c

#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include "uart_windows.h"
#include <modbus/modbus.h>
#include <modbus/log.h>

uart_handle_t uart;
modbus_context_t ctx;

void my_console_logger(log_level_t severity, char *msg) {
    SYSTEMTIME t;
    GetSystemTime(&t); // or GetLocalTime(&t)
    // printf("The system time is: %02d:%02d:%02d.%03d\n", 
    //     t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
     printf("%02d:%02d:%02d.%03d [%s]: %s\n",
         t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,    // user defined function
         log_level_name(severity),
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
        LOG_ERROR("Falha ao escrever na UART.");
    } else {
        LOG_DEBUG("Escrito %d bytes na UART.", result);
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
        LOG_DEBUG("Receiving data on uart. State %s\n", client->fsm.current_state->name);
        for (size_t i = 0; i < size_read; i++) {
            modbus_client_receive_data_event(&client->fsm, data[i]);
        }
    }
    
}

// interrupt from uart
int on_byte_received(uint8_t data) {
    LOG_DEBUG("Master recebeu byte: %d", data);
    modbus_client_data_t *client = (modbus_client_data_t *)ctx.user_data;
    modbus_client_receive_data_event(&client->fsm, data);
    return 0;
}

int main(void) {
    // Initialize logging
    LOG_INIT();
    LOG_SUBSCRIBE(my_console_logger, LOG_TRACE_LEVEL);
    LOG_INFO("Initializing Modbus Master RTU Example...\n");

    // Initialize UART
    
    const char *com_port = "COM18"; // Replace with your COM port
    uint16_t baud_rate = 19200;

    if (uart_init(&uart, com_port, (int)baud_rate) != 0) {
        LOG_ERROR("UART initialization failed.\n");
        return 1;
    }
    // uart_set_callback(&uart, on_byte_received);

    // Define transport with user_data as the UART handle
    modbus_transport_t transport = {
        .write = transport_write,
        .read  = transport_read,
        .get_reference_msec = get_current_time_ms,
        .measure_time_msec   = measure_elapsed_time,
    };

    // Initialize Modbus Master context    

    modbus_error_t error = modbus_client_create(&ctx, &transport, &baud_rate);
    if (error != MODBUS_ERROR_NONE) {
        LOG_ERROR("Failed to initialize Modbus Master. Error code: %d\n", error);
        uart_close(&uart);
        return 1;
    }

    LOG_INFO("Modbus Master initialized successfully.\n");

    // Set response timeout
    modbus_client_set_timeout(&ctx, 500); // 500 ms
    LOG_INFO("Response timeout set to 500 ms.\n");

    // Example: Read Holding Registers from Slave
    uint8_t slave_id = 1;
    uint16_t start_address = 0x0000;
    uint16_t quantity = 1;

    LOG_INFO("Sending Read Holding Registers request to Slave ID %d...\n", slave_id);
    error = modbus_client_read_holding_registers(&ctx, slave_id, start_address, quantity);
    if (error != MODBUS_ERROR_NONE) {
        LOG_ERROR("Failed to send Read Holding Registers request. Error code: %d\n", error);
    } else {
        LOG_INFO("Read Holding Registers request sent successfully.\n");
    }

    // Main polling loop
    LOG_INFO("Entering main polling loop. Press Ctrl+C to exit.\n");
    while (1) {
        uart_interrupt();
        modbus_client_poll(&ctx);

        int16_t data_buffer[16];
        uint16_t regs_read = modbus_client_get_read_data(&ctx, data_buffer, quantity);
        if (regs_read > 0) {
            LOG_INFO("Recebido %d registers do Slave ID %d:\n", regs_read, slave_id);
            for (int i = 0; i < regs_read; i++) {
                LOG_INFO("  Register %d: %d\n", start_address + i, data_buffer[i]);
            }
            Sleep(100); // Espera 1 segundo antes de enviar outra solicitação

            // Enviar nova solicitação de leitura
            LOG_INFO("Enviando nova solicitação de leitura para Slave ID %d...\n", slave_id);
            error = modbus_client_read_holding_registers(&ctx, slave_id, start_address, quantity);
            if (error != MODBUS_ERROR_NONE) {
                LOG_ERROR("Falha ao enviar solicitação de leitura. Código de erro: %d\n", error);
            } else {
                LOG_INFO("Solicitação de leitura enviada com sucesso.\n");
            }
        }

        // Sleep(10); // Reduz o uso da CPU
    }

    // Cleanup (unreachable in this example)
    // modbus_client_destroy(&ctx);
    uart_close(&uart);
    return 0;
}
