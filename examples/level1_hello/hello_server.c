/**
 * @file hello_server.c
 * @brief Minimal Modbus TCP server using convenience API.
 *
 * This example demonstrates the simplified server setup API.
 * Compare with tcp_server_demo.c (~200 lines) vs this (~30 lines).
 */

#include <modbus/mb_server_convenience.h>
#include <modbus/server.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

int main(void)
{
    printf("Modbus TCP Server - Convenience API Demo\n");
    printf("=========================================\n\n");

    /* Install signal handlers for clean shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 1. Allocate data arrays (user owns memory) */
    uint16_t holding_registers[100];
    uint16_t input_registers[50];
    
    /* Initialize with test data */
    for (int i = 0; i < 100; i++) {
        holding_registers[i] = i * 10;
    }
    for (int i = 0; i < 50; i++) {
        input_registers[i] = i * 100;
    }

    /* 2. Create server with ONE function call */
    mb_server_t server;
    mb_err_t err = mb_server_create_tcp(&server, 502, 1);
    
    if (err != MB_OK) {
        printf("ERROR: Failed to create TCP server: %d\n", err);
        printf("Note: This is a placeholder implementation.\n");
        printf("      See tcp_server_demo.c for fully working example.\n");
        return 1;
    }

    /* 3. Register data regions with simple one-liners */
    err = mb_server_add_holding(&server, 0, holding_registers, 100);
    if (err != MB_OK) {
        printf("ERROR: Failed to register holding registers: %d\n", err);
        goto cleanup;
    }

    err = mb_server_add_input(&server, 0, input_registers, 50);
    if (err != MB_OK) {
        printf("ERROR: Failed to register input registers: %d\n", err);
        goto cleanup;
    }

    printf("Server configured:\n");
    printf("  - Port: 502\n");
    printf("  - Unit ID: 1\n");
    printf("  - Holding registers: 0-99\n");
    printf("  - Input registers: 0-49\n");
    printf("\nPress Ctrl+C to stop...\n\n");

    /* 4. Event loop (application's responsibility) */
    while (running) {
        /* Poll for incoming requests */
        err = mb_server_poll(&server);
        
        /* Small sleep to avoid busy-waiting */
        usleep(1000); /* 1ms */
    }

    printf("\nShutting down...\n");

cleanup:
    /* 5. Cleanup */
    mb_server_convenience_destroy(&server);
    
    printf("Server stopped.\n");
    return 0;
}
