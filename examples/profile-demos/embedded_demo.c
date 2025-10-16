/**
 * @file embedded_demo.c
 * @brief Demonstration of EMBEDDED profile for MCU/IoT
 *
 * This example shows how to use the EMBEDDED profile for resource-constrained systems.
 * Minimal footprint, essential features only.
 *
 * Features:
 * - ~26KB code, ~1.5KB RAM
 * - Client only (server disabled to save memory)
 * - RTU transport only
 * - Essential function codes only
 * - Power management enabled
 *
 * Build:
 *   cmake --preset profile-embedded
 *   cmake --build --preset profile-embedded
 *   ./build/profile-embedded/examples/profile-demos/embedded_demo
 *
 * Or with gcc:
 *   gcc -DMB_USE_PROFILE_EMBEDDED embedded_demo.c -lmodbus -o embedded_demo
 */

/* STEP 1: Select EMBEDDED profile for minimal footprint */
#define MB_USE_PROFILE_EMBEDDED

/* STEP 2: Include only what we need */
#include <modbus/client_sync.h>
#include <modbus/mb_err.h>
#include <stdio.h>
#include <stdlib.h>

/* Simulated serial port for demo (on real MCU, this would be actual UART) */
#include "../common/demo_serial_port.h"

int main(int argc, char **argv)
{
    printf("=== ModbusCore EMBEDDED Profile Demo ===\n");
    printf("Profile: EMBEDDED (MCU/IoT)\n");
    printf("Features: Minimal footprint (~26KB code, ~1.5KB RAM)\n");
    printf("Client only, RTU only, essential FCs only\n\n");

    /* On real embedded system, this would be your serial port */
    const char *device = (argc > 1) ? argv[1] : "/dev/ttyUSB0";
    uint32_t baudrate = 115200;

    printf("Opening serial port %s at %u baud...\n", device, baudrate);

    /* Initialize serial port (simulated for demo) */
    demo_serial_port_t serial_ctx;
    mb_err_t status = demo_serial_port_open(&serial_ctx, device, baudrate);
    if (!mb_err_is_ok(status)) {
        fprintf(stderr, "ERROR: Failed to open serial port\n");
        return EXIT_FAILURE;
    }

    printf("✓ Serial port opened\n\n");

    /* Get transport interface */
    const mb_transport_if_t *iface = demo_serial_port_iface(&serial_ctx);

    /* Initialize client with minimal resources */
    mb_client_t client;
    mb_client_txn_t txn_pool[4];  /* Small pool to save RAM */
    status = mb_client_init(&client, iface, txn_pool, 4);
    if (!mb_err_is_ok(status)) {
        fprintf(stderr, "ERROR: Failed to initialize client\n");
        demo_serial_port_close(&serial_ctx);
        return EXIT_FAILURE;
    }

    printf("✓ Client initialized (4 transaction pool)\n\n");

    /* Read holding registers (FC03 - enabled in EMBEDDED) */
    printf("Reading 4 holding registers from slave 1...\n");
    uint16_t registers[4];
    mb_err_t err = mb_client_read_holding_sync(&client, 1, 0, 4, registers, NULL);

    if (mb_err_is_ok(err)) {
        printf("✓ Read successful:\n");
        for (int i = 0; i < 4; i++) {
            printf("  Reg[%d] = %u\n", i, registers[i]);
        }
    } else {
        fprintf(stderr, "✗ Read failed: %s\n", mb_err_str(err));
    }

    printf("\n");

    /* Write single register (FC06 - enabled in EMBEDDED) */
    printf("Writing value 100 to register 10...\n");
    err = mb_client_write_register_sync(&client, 1, 10, 100, NULL);

    if (mb_err_is_ok(err)) {
        printf("✓ Write successful\n");
    } else {
        fprintf(stderr, "✗ Write failed: %s\n", mb_err_str(err));
    }

    printf("\n");

    /* Demonstrate power management (CRITICAL for battery devices) */
    #if MB_CONF_ENABLE_POWER_MANAGEMENT
    printf("Power management: ENABLED\n");
    printf("(On real MCU, this would put the device to sleep when idle)\n");

    /* Example: Set idle callback (on real MCU, this would enter sleep mode) */
    /* mb_client_set_idle_callback(&client, my_sleep_callback, NULL, 5); */
    #else
    printf("Power management: DISABLED\n");
    #endif

    printf("\n");

    /* Memory footprint info */
    printf("=== Memory Usage ===\n");
    printf("Client structure: %zu bytes\n", sizeof(mb_client_t));
    printf("Transaction pool: %zu bytes (4 slots)\n", sizeof(txn_pool));
    printf("Total static RAM: ~%zu bytes\n", sizeof(client) + sizeof(txn_pool));

    printf("\n");

    /* Feature comparison */
    printf("=== EMBEDDED Profile Features ===\n");
    printf("Client:           %s\n", MB_CONF_BUILD_CLIENT ? "✓" : "✗");
    printf("Server:           %s\n", MB_CONF_BUILD_SERVER ? "✓" : "✗");
    printf("RTU Transport:    %s\n", MB_CONF_TRANSPORT_RTU ? "✓" : "✗");
    printf("TCP Transport:    %s\n", MB_CONF_TRANSPORT_TCP ? "✓" : "✗");
    printf("ASCII Transport:  %s\n", MB_CONF_TRANSPORT_ASCII ? "✓" : "✗");
    printf("Power Management: %s\n", MB_CONF_ENABLE_POWER_MANAGEMENT ? "✓" : "✗");
    printf("ISR Mode:         %s\n", MB_CONF_ENABLE_ISR_MODE ? "✓" : "✗");
    printf("QoS:              %s\n", MB_CONF_ENABLE_QOS ? "✓" : "✗");

    printf("\n");

    /* Cleanup */
    printf("Closing...\n");
    demo_serial_port_close(&serial_ctx);
    printf("✓ Closed\n");

    printf("\n=== Demo Complete ===\n");
    printf("The EMBEDDED profile optimizes for:\n");
    printf("  • Minimal code size (~26KB)\n");
    printf("  • Minimal RAM usage (~1.5KB)\n");
    printf("  • Essential features only\n");
    printf("  • Power management for battery devices\n");
    printf("  • Perfect for STM32, ESP32, nRF52\n");

    return EXIT_SUCCESS;
}
