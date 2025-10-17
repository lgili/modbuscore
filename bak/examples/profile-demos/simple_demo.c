/**
 * @file simple_demo.c
 * @brief Demonstration of SIMPLE profile for desktop/testing
 *
 * This example shows how to use the SIMPLE profile for quick desktop development.
 * All features are enabled for maximum ease of use and learning.
 *
 * Build:
 *   cmake --preset profile-simple
 *   cmake --build --preset profile-simple
 *   ./build/profile-simple/examples/profile-demos/simple_demo
 *
 * Or with gcc:
 *   gcc -DMB_USE_PROFILE_SIMPLE simple_demo.c -lmodbus -o simple_demo
 */

/* STEP 1: Select profile BEFORE including any modbus headers */
#define MB_USE_PROFILE_SIMPLE

/* STEP 2: Include modbus - profile is automatically configured */
#include <modbus/mb_host.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    printf("=== ModbusCore SIMPLE Profile Demo ===\n");
    printf("Profile: SIMPLE (Desktop/Testing)\n");
    printf("Features: All enabled, easy to use\n\n");

    /* Parse command line */
    const char *endpoint = (argc > 1) ? argv[1] : "127.0.0.1:502";

    printf("Connecting to %s...\n", endpoint);

    /* With SIMPLE profile, mb_host_tcp_connect is available */
    mb_host_client_t *client = mb_host_tcp_connect(endpoint);
    if (!client) {
        fprintf(stderr, "ERROR: Failed to connect to %s\n", endpoint);
        fprintf(stderr, "Make sure a Modbus TCP server is running\n");
        return EXIT_FAILURE;
    }

    printf("✓ Connected successfully\n\n");

    /* Enable logging to see what's happening (available in SIMPLE profile) */
    mb_host_enable_logging(client, true);

    /* Read holding registers */
    printf("Reading 10 holding registers starting at address 0...\n");
    uint16_t registers[10];
    mb_err_t err = mb_host_read_holding(client, 1, 0, 10, registers);

    if (err == MB_OK) {
        printf("✓ Read successful:\n");
        for (int i = 0; i < 10; i++) {
            printf("  Register %d: %u (0x%04X)\n", i, registers[i], registers[i]);
        }
    } else {
        fprintf(stderr, "✗ Read failed: %s\n", mb_host_error_string(err));
        if (err == MB_ERR_EXCEPTION) {
            uint8_t exc = mb_host_last_exception(client);
            fprintf(stderr, "  Exception code: 0x%02X\n", exc);
        }
    }

    printf("\n");

    /* Write a single register */
    printf("Writing value 1234 to register 100...\n");
    err = mb_host_write_single_register(client, 1, 100, 1234);

    if (err == MB_OK) {
        printf("✓ Write successful\n");

        /* Read it back to verify */
        uint16_t value;
        err = mb_host_read_holding(client, 1, 100, 1, &value);
        if (err == MB_OK) {
            printf("✓ Verification read: register 100 = %u\n", value);
        }
    } else {
        fprintf(stderr, "✗ Write failed: %s\n", mb_host_error_string(err));
    }

    printf("\n");

    /* Write multiple registers */
    printf("Writing 5 registers starting at address 200...\n");
    uint16_t write_values[] = {100, 200, 300, 400, 500};
    err = mb_host_write_multiple_registers(client, 1, 200, 5, write_values);

    if (err == MB_OK) {
        printf("✓ Multiple write successful\n");

        /* Read them back */
        uint16_t read_back[5];
        err = mb_host_read_holding(client, 1, 200, 5, read_back);
        if (err == MB_OK) {
            printf("✓ Verification read:\n");
            for (int i = 0; i < 5; i++) {
                printf("  Register %d: %u\n", 200 + i, read_back[i]);
            }
        }
    } else {
        fprintf(stderr, "✗ Write failed: %s\n", mb_host_error_string(err));
    }

    /* Cleanup */
    printf("\nDisconnecting...\n");
    mb_host_disconnect(client);
    printf("✓ Disconnected\n");

    printf("\n=== Demo Complete ===\n");
    printf("This demo showed:\n");
    printf("  • Simple connection with mb_host_tcp_connect()\n");
    printf("  • Reading registers with mb_host_read_holding()\n");
    printf("  • Writing single register with mb_host_write_single_register()\n");
    printf("  • Writing multiple registers\n");
    printf("  • Error handling and logging\n");
    printf("\nThe SIMPLE profile makes all this easy!\n");

    return EXIT_SUCCESS;
}
