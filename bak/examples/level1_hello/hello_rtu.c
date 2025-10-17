/**
 * @file hello_rtu.c
 * @brief Simplest possible Modbus RTU client - reads one register.
 *
 * Build: gcc hello_rtu.c -lmodbus -o hello_rtu
 * Run:   ./hello_rtu
 */

#include <modbus/mb_host.h>
#include <stdio.h>

int main(void) {
    // Connect to RTU device (adjust device path as needed)
    mb_host_client_t *client = mb_host_rtu_connect("/dev/ttyUSB0", 115200);
    if (!client) { return 1; }

    // Read holding register 0 from unit 1
    uint16_t value;
    mb_err_t err = mb_host_read_holding(client, 1, 0, 1, &value);

    // Print result
    printf("Register 0: %u (%s)\n", value, mb_host_error_string(err));

    // Cleanup
    mb_host_disconnect(client);
    return (err == MB_OK) ? 0 : 1;
}
