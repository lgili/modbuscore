/**
 * @file hello_write.c
 * @brief Simplest write example - sets one register to 1234.
 *
 * Build: gcc hello_write.c -lmodbus -o hello_write
 * Run:   ./hello_write
 */

#include <modbus/mb_host.h>
#include <stdio.h>

int main(void) {
    // Connect to Modbus TCP server
    mb_host_client_t *client = mb_host_tcp_connect("127.0.0.1:502");
    if (!client) { return 1; }

    // Write value 1234 to holding register 100
    mb_err_t err = mb_host_write_single_register(client, 1, 100, 1234);

    // Print result
    printf("Write result: %s\n", mb_host_error_string(err));

    // Cleanup
    mb_host_disconnect(client);
    return (err == MB_OK) ? 0 : 1;
}
