/**
 * @file full_example.c
 * @brief Complete example showcasing the mb_simple API
 *
 * This demonstrates:
 * - Connection with custom options
 * - Reading multiple types of data
 * - Writing registers and coils
 * - Error handling
 * - Using convenience macros
 *
 * Build:
 *   gcc -DMB_USE_PROFILE_SIMPLE full_example.c -lmodbus -o full_example
 */

#define MB_USE_PROFILE_SIMPLE
#include <modbus/mb_simple.h>
#include <modbus/mb_err.h>
#include <stdio.h>
#include <stdlib.h>

/* Helper function demonstrating MB_CHECK macro */
static mb_err_t write_configuration(mb_t *mb)
{
    printf("\n--- Writing Configuration ---\n");

    /* MB_CHECK automatically handles errors and returns */
    MB_CHECK(mb_write_register(mb, 1, 100, 1234), "Failed to write register 100");
    MB_CHECK(mb_write_register(mb, 1, 101, 5678), "Failed to write register 101");
    MB_CHECK(mb_write_coil(mb, 1, 10, true), "Failed to write coil 10");

    printf("✓ Configuration written successfully\n");
    return MB_OK;
}

int main(int argc, char **argv)
{
    const char *endpoint = (argc > 1) ? argv[1] : "127.0.0.1:502";

    printf("=== ModbusCore Simple API - Full Example ===\n");
    printf("Connecting to %s...\n", endpoint);

    /* Create connection with custom options */
    mb_options_t opts;
    mb_options_init(&opts);
    opts.timeout_ms = 2000;       /* 2 second timeout */
    opts.max_retries = 5;         /* Retry 5 times */
    opts.enable_logging = true;   /* Enable debug logs */

    MB_AUTO(mb, mb_create_tcp_ex(endpoint, &opts));

    if (!mb) {
        fprintf(stderr, "✗ Connection failed\n");
        return EXIT_FAILURE;
    }

    printf("✓ Connected!\n");

    /* ========== READING DATA ========== */

    printf("\n--- Reading Holding Registers ---\n");
    uint16_t holding_regs[10];
    mb_err_t err = mb_read_holding(mb, 1, 0, 10, holding_regs);

    if (err == MB_OK) {
        printf("✓ Read successful:\n");
        for (int i = 0; i < 10; i++) {
            printf("  Holding[%d] = %u (0x%04X)\n", i, holding_regs[i], holding_regs[i]);
        }
    } else {
        fprintf(stderr, "✗ Read failed: %s\n", mb_error_string(err));
        if (err == MB_ERR_EXCEPTION) {
            uint8_t exc = mb_last_exception(mb);
            fprintf(stderr, "  Exception code: 0x%02X\n", exc);
        }
    }

    printf("\n--- Reading Input Registers ---\n");
    uint16_t input_regs[5];
    err = mb_read_input(mb, 1, 0, 5, input_regs);

    if (err == MB_OK) {
        printf("✓ Read successful:\n");
        for (int i = 0; i < 5; i++) {
            printf("  Input[%d] = %u\n", i, input_regs[i]);
        }
    } else {
        /* Using MB_LOG_ERROR - logs but doesn't return */
        MB_LOG_ERROR(err, "Read input registers failed");
    }

    printf("\n--- Reading Coils ---\n");
    uint8_t coils[2]; /* 16 coils = 2 bytes */
    err = mb_read_coils(mb, 1, 0, 16, coils);

    if (err == MB_OK) {
        printf("✓ Read successful:\n");
        for (int i = 0; i < 16; i++) {
            bool coil = (coils[i / 8] >> (i % 8)) & 1;
            printf("  Coil[%d] = %s\n", i, coil ? "ON" : "OFF");
        }
    } else {
        MB_LOG_ERROR(err, "Read coils failed");
    }

    /* ========== WRITING DATA ========== */

    printf("\n--- Writing Single Register ---\n");
    err = mb_write_register(mb, 1, 100, 1234);
    if (err == MB_OK) {
        printf("✓ Wrote 1234 to register 100\n");

        /* Verify by reading back */
        uint16_t verify;
        if (mb_read_holding(mb, 1, 100, 1, &verify) == MB_OK) {
            printf("✓ Verified: register 100 = %u\n", verify);
        }
    } else {
        MB_LOG_ERROR(err, "Write register failed");
    }

    printf("\n--- Writing Multiple Registers ---\n");
    uint16_t write_values[] = {100, 200, 300, 400, 500};
    err = mb_write_registers(mb, 1, 200, 5, write_values);
    if (err == MB_OK) {
        printf("✓ Wrote 5 registers starting at address 200\n");

        /* Verify */
        uint16_t read_back[5];
        if (mb_read_holding(mb, 1, 200, 5, read_back) == MB_OK) {
            printf("✓ Verified:\n");
            for (int i = 0; i < 5; i++) {
                printf("  Register[%d] = %u\n", 200 + i, read_back[i]);
            }
        }
    } else {
        MB_LOG_ERROR(err, "Write multiple registers failed");
    }

    printf("\n--- Writing Coil ---\n");
    err = mb_write_coil(mb, 1, 10, true);
    if (err == MB_OK) {
        printf("✓ Set coil 10 to ON\n");
    } else {
        MB_LOG_ERROR(err, "Write coil failed");
    }

    /* ========== USING HELPER FUNCTION ========== */

    err = write_configuration(mb);
    if (err != MB_OK) {
        fprintf(stderr, "Configuration failed\n");
    }

    /* ========== CONFIGURATION ========== */

    printf("\n--- Configuration ---\n");
    printf("Current timeout: %u ms\n", mb_get_timeout(mb));

    printf("Changing timeout to 5000ms...\n");
    mb_set_timeout(mb, 5000);
    printf("New timeout: %u ms\n", mb_get_timeout(mb));

    /* ========== CLEANUP ========== */

    printf("\n✓ All operations complete\n");
    printf("✓ Cleanup happens automatically (MB_AUTO)\n");

    /* mb_destroy(mb) called automatically here */
    return EXIT_SUCCESS;
}
