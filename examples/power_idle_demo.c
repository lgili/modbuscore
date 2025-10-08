/**
 * @file power_idle_demo.c
 * @brief Demonstration of power management idle callbacks
 *
 * This example shows how to use idle callbacks to implement power-saving
 * features in battery-powered devices or tickless RTOS environments.
 *
 * Build:
 *   gcc -o power_idle_demo power_idle_demo.c -I../modbus/include -L../build/host-debug/modbus -lmodbusd
 *
 * Run:
 *   ./power_idle_demo
 */

#include <modbus/client.h>
#include <modbus/mb_power.h>
#include <modbus/mb_log.h>
#include <stdio.h>
#include <unistd.h>

/* ========================================================================== */
/*                           Platform-Specific Sleep                          */
/* ========================================================================== */

/**
 * @brief Simple idle callback using WFI-style sleep
 *
 * This demonstrates the simplest form of power saving - just wait for
 * interrupt. On real hardware, this would be __WFI() or similar.
 */
static uint32_t simple_wfi_callback(void *ctx, uint32_t sleep_ms)
{
    (void)ctx;
    
    printf("  [POWER] Idle detected, sleeping for ~%u ms (using usleep)\n", sleep_ms);
    
    // On real hardware, this would be __WFI() or enter sleep mode
    // For demo purposes, we use usleep
    usleep(sleep_ms * 1000);
    
    return sleep_ms;
}

/**
 * @brief Multi-level power saving callback
 *
 * This demonstrates selecting different power modes based on sleep duration.
 * Longer sleeps justify deeper power modes despite higher wake-up overhead.
 */
static uint32_t multi_level_callback(void *ctx, uint32_t sleep_ms)
{
    (void)ctx;
    
    if (sleep_ms > 100) {
        // Deep sleep for long idle periods
        printf("  [POWER] Deep sleep for %u ms\n", sleep_ms);
        usleep(sleep_ms * 1000);
        return sleep_ms;
    } else if (sleep_ms > 10) {
        // Light sleep for medium idle periods
        printf("  [POWER] Light sleep for %u ms\n", sleep_ms);
        usleep(sleep_ms * 1000);
        return sleep_ms;
    } else {
        // Just WFI for short idle periods
        printf("  [POWER] Quick WFI (%u ms)\n", sleep_ms);
        usleep(1000); // Just 1ms
        return 1;
    }
}

/* ========================================================================== */
/*                              Demo Functions                                */
/* ========================================================================== */

static void demo_basic_idle_callback(void)
{
    printf("\n=== Demo 1: Basic Idle Callback ===\n");
    
    // Create a Modbus TCP client
    mb_client_t client;
    mb_tcp_transport_t tcp;
    mb_client_txn_t txn_pool[4];
    
    mb_client_init(&client, &tcp, txn_pool, 4);
    
    // Register simple idle callback with 5ms threshold
    printf("Registering idle callback (threshold: 5ms)...\n");
    mb_err_t err = mb_client_set_idle_callback(&client, simple_wfi_callback, NULL, 5);
    if (err == MB_OK) {
        printf("✓ Idle callback registered successfully\n");
    }
    
    // Query the configuration
    mb_idle_config_t config;
    mb_client_get_idle_config(&client, &config);
    printf("  Callback enabled: %s\n", config.enabled ? "yes" : "no");
    printf("  Threshold: %u ms\n", config.threshold_ms);
    
    // Simulate some poll cycles
    printf("\nSimulating poll cycles (client is idle)...\n");
    for (int i = 0; i < 3; i++) {
        printf("Poll cycle %d:\n", i + 1);
        mb_client_poll(&client);
        usleep(10000); // 10ms between polls
    }
    
    // Disable callback
    printf("\nDisabling idle callback...\n");
    mb_client_set_idle_callback(&client, NULL, NULL, 0);
    printf("✓ Callback disabled\n");
}

static void demo_multi_level_power(void)
{
    printf("\n=== Demo 2: Multi-Level Power Saving ===\n");
    
    mb_client_t client;
    mb_tcp_transport_t tcp;
    mb_client_txn_t txn_pool[4];
    
    mb_client_init(&client, &tcp, txn_pool, 4);
    
    // Register multi-level callback
    printf("Registering multi-level power callback...\n");
    mb_client_set_idle_callback(&client, multi_level_callback, NULL, 5);
    
    // The callback will automatically choose the appropriate power mode
    // based on how long the system can sleep
    printf("\nSimulating various idle durations...\n");
    for (int i = 0; i < 3; i++) {
        printf("Poll cycle %d:\n", i + 1);
        mb_client_poll(&client);
        usleep(20000); // 20ms between polls
    }
}

static void demo_query_idle_state(void)
{
    printf("\n=== Demo 3: Querying Idle State ===\n");
    
    mb_client_t client;
    mb_tcp_transport_t tcp;
    mb_client_txn_t txn_pool[4];
    
    mb_client_init(&client, &tcp, txn_pool, 4);
    
    // Check if client is idle (without callback)
    bool is_idle = mb_client_is_idle(&client);
    printf("Client idle state: %s\n", is_idle ? "IDLE" : "BUSY");
    
    // Get time until next event
    uint32_t time_until_event = mb_client_time_until_next_event(&client);
    if (time_until_event == UINT32_MAX) {
        printf("Time until next event: No events scheduled\n");
    } else {
        printf("Time until next event: %u ms\n", time_until_event);
    }
}

/* ========================================================================== */
/*                                   Main                                     */
/* ========================================================================== */

int main(void)
{
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║       Modbus Power Management Demo (Gate 27)              ║\n");
    printf("║                                                            ║\n");
    printf("║  This demonstrates idle callbacks for power-efficient     ║\n");
    printf("║  operation in battery-powered and tickless RTOS systems.  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Run demos
    demo_basic_idle_callback();
    demo_multi_level_power();
    demo_query_idle_state();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                      Demo Complete!                        ║\n");
    printf("║                                                            ║\n");
    printf("║  In production code, replace usleep() with:               ║\n");
    printf("║  • STM32: HAL_PWR_EnterSTOPMode()                         ║\n");
    printf("║  • FreeRTOS: vTaskDelay()                                 ║\n");
    printf("║  • Zephyr: k_sleep()                                      ║\n");
    printf("║  • ESP32: esp_light_sleep_start()                         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
