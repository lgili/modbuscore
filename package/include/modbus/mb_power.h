/**
 * @file mb_power.h
 * @brief Power management and tickless real-time support for battery-powered applications
 *
 * This module provides idle callbacks and power-saving features for applications
 * that need to minimize power consumption, particularly in battery-powered devices
 * or tickless RTOS environments.
 *
 * Features:
 * - Idle detection (no pending transactions, no RX data)
 * - Callback when idle for > threshold time
 * - Zero busy-waiting
 * - Support for various sleep modes (light sleep, deep sleep, tickless idle)
 *
 * @section power_usage Usage Example
 * @code
 * uint32_t my_idle_callback(void *ctx, uint32_t sleep_ms) {
 *     if (sleep_ms > 100) {
 *         enter_deep_sleep(sleep_ms);
 *         return get_actual_sleep_time();
 *     } else if (sleep_ms > 10) {
 *         enter_light_sleep(sleep_ms);
 *         return sleep_ms;
 *     } else {
 *         __WFI();  // Wait For Interrupt
 *         return 1;
 *     }
 * }
 *
 * mb_client_set_idle_callback(&client, my_idle_callback, NULL, 5);
 * @endcode
 */

#ifndef MODBUS_MB_POWER_H
#define MODBUS_MB_POWER_H

#include <modbus/conf.h>
#include <modbus/mb_types.h>
#include <modbus/mb_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                                Types                                       */
/* ========================================================================== */

/**
 * @brief Idle callback function type
 *
 * This callback is invoked when the Modbus library is idle and can enter
 * a low-power state. The callback receives the suggested sleep duration
 * and should return the actual sleep duration achieved.
 *
 * The callback is called when:
 * - Client: No pending transactions and no active timeouts
 * - Server: No pending requests and RX buffer is empty
 * - Time until next event > threshold_ms
 *
 * @param user_ctx User context provided during registration
 * @param sleep_ms Suggested sleep duration in milliseconds
 *                 This is the time until the next expected event
 * @return Actual sleep duration in milliseconds
 *         Return 0 if sleep was not performed
 *
 * @warning This callback may be called from ISR context depending on
 *          the port implementation. Keep it short and ISR-safe.
 *
 * @note The library will recalculate timeouts based on the returned value
 */
typedef mb_u32 (*mb_idle_callback_t)(void *user_ctx, mb_u32 sleep_ms);

/**
 * @brief Idle callback configuration
 */
struct mb_idle_config {
    mb_idle_callback_t callback;  /**< Idle callback function */
    void *user_ctx;               /**< User context passed to callback */
    mb_u32 threshold_ms;          /**< Minimum idle time to trigger callback (ms) */
    bool enabled;                 /**< Whether idle detection is enabled */
};

typedef struct mb_idle_config mb_idle_config_t;

/* ========================================================================== */
/*                          Client Power Management                           */
/* ========================================================================== */

/**
 * @brief Register idle callback for client
 *
 * The callback will be invoked when the client is idle (no pending
 * transactions) and the time until the next expected event exceeds
 * the threshold.
 *
 * @param client Client instance
 * @param callback Idle callback function (NULL to disable)
 * @param user_ctx User context passed to callback
 * @param threshold_ms Minimum idle duration to trigger callback (ms)
 *                     Recommended: 5-10ms for most applications
 *                     Higher values reduce callback frequency
 *
 * @return MB_OK on success
 *         MB_ERR_INVALID_ARG if client is NULL
 *
 * @note Setting callback to NULL disables idle detection
 * @note The callback may be invoked multiple times during long idle periods
 */
mb_err_t mb_client_set_idle_callback(void *client,
                                      mb_idle_callback_t callback,
                                      void *user_ctx,
                                      mb_u32 threshold_ms);

/**
 * @brief Get current idle configuration for client
 *
 * @param client Client instance
 * @param config Output parameter for current configuration
 * @return MB_OK on success
 *         MB_ERR_INVALID_ARG if client or config is NULL
 */
mb_err_t mb_client_get_idle_config(const void *client,
                                    mb_idle_config_t *config);

/**
 * @brief Get time until next client event
 *
 * Returns the time until the next expected event (timeout, retry, etc.)
 * This can be used to determine how long the application can sleep.
 *
 * @param client Client instance
 * @return Time in milliseconds until next event
 *         0 if an event is pending immediately
 *         UINT32_MAX if no events are scheduled
 *
 * @note mb_client_is_idle() is declared in client.h
 */
mb_u32 mb_client_time_until_next_event(const void *client);

/* ========================================================================== */
/*                          Server Power Management                           */
/* ========================================================================== */

/**
 * @brief Register idle callback for server
 *
 * The callback will be invoked when the server is idle (no pending
 * requests, RX buffer empty) and the time until the next expected
 * event exceeds the threshold.
 *
 * @param server Server instance
 * @param callback Idle callback function (NULL to disable)
 * @param user_ctx User context passed to callback
 * @param threshold_ms Minimum idle duration to trigger callback (ms)
 *
 * @return MB_OK on success
 *         MB_ERR_INVALID_ARG if server is NULL
 *
 * @note For servers, idle time may be longer as they wait for requests
 */
mb_err_t mb_server_set_idle_callback(void *server,
                                      mb_idle_callback_t callback,
                                      void *user_ctx,
                                      mb_u32 threshold_ms);

/**
 * @brief Get current idle configuration for server
 *
 * @param server Server instance
 * @param config Output parameter for current configuration
 * @return MB_OK on success
 *         MB_ERR_INVALID_ARG if server or config is NULL
 */
mb_err_t mb_server_get_idle_config(const void *server,
                                    mb_idle_config_t *config);

/**
 * @brief Get time until next server event
 *
 * @param server Server instance
 * @return Time in milliseconds until next event
 *         0 if an event is pending immediately
 *         UINT32_MAX if no events are scheduled (typical for servers)
 *
 * @note mb_server_is_idle() is declared in server.h
 */
mb_u32 mb_server_time_until_next_event(const void *server);

/* ========================================================================== */
/*                              Helper Macros                                 */
/* ========================================================================== */

/**
 * @brief Calculate sleep duration with margin
 *
 * Reduces the suggested sleep duration by a margin to ensure the application
 * wakes up before the actual event. This accounts for wake-up latency and
 * clock drift.
 *
 * @param sleep_ms Suggested sleep duration
 * @param margin_ms Margin to subtract (default: 1-2ms)
 */
#define MB_POWER_SLEEP_WITH_MARGIN(sleep_ms, margin_ms) \
    ((sleep_ms) > (margin_ms) ? ((sleep_ms) - (margin_ms)) : 0U)

/**
 * @brief Check if sleep duration is worth entering low-power mode
 *
 * Some sleep modes have overhead (e.g., clock reconfiguration). This macro
 * helps determine if the sleep duration justifies the mode switch.
 *
 * @param sleep_ms Sleep duration
 * @param min_ms Minimum duration for mode switch (e.g., 5ms for STOP mode)
 */
#define MB_POWER_SLEEP_IS_WORTH(sleep_ms, min_ms) \
    ((sleep_ms) >= (min_ms))

/* ========================================================================== */
/*                         Platform-Specific Hints                            */
/* ========================================================================== */

/**
 * @brief Recommended sleep modes by duration
 *
 * These are guidelines for typical ARM Cortex-M MCUs. Adjust based on
 * your specific platform's power modes and wake-up latencies.
 *
 * Duration (ms)  | Recommended Mode           | Typical Wake-up
 * ---------------|----------------------------|------------------
 * 0-1            | WFI/WFE                   | < 10 µs
 * 1-10           | Sleep mode                 | < 100 µs
 * 10-100         | Stop mode (light sleep)    | < 1 ms
 * 100-1000       | Stop mode (deep sleep)     | < 10 ms
 * > 1000         | Standby/Shutdown           | < 100 ms
 *
 * @note Always consider:
 * - UART RX: Must wake on IDLE interrupt or DMA
 * - Timers: May need RTC for long sleeps
 * - Clock: May need reconfiguration after wake-up
 * - Peripherals: May need to be reconfigured
 */

/**
 * @brief Example idle callbacks for different platforms
 *
 * @code
 * // STM32 - Simple WFI
 * uint32_t stm32_wfi_idle(void *ctx, uint32_t ms) {
 *     __WFI();
 *     return 1;
 * }
 *
 * // STM32 - STOP mode with RTC wake-up
 * uint32_t stm32_stop_idle(void *ctx, uint32_t ms) {
 *     HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, ms);
 *     HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
 *     SystemClock_Config();  // Reconfigure clocks
 *     return HAL_RTCEx_GetWakeUpTimer(&hrtc);
 * }
 *
 * // FreeRTOS - Automatic tickless idle
 * uint32_t freertos_idle(void *ctx, uint32_t ms) {
 *     vTaskDelay(pdMS_TO_TICKS(ms));
 *     return ms;
 * }
 *
 * // Zephyr - Built-in low-power management
 * uint32_t zephyr_idle(void *ctx, uint32_t ms) {
 *     k_sleep(K_MSEC(ms));
 *     return ms;
 * }
 *
 * // ESP32 - Light sleep
 * uint32_t esp32_idle(void *ctx, uint32_t ms) {
 *     esp_sleep_enable_timer_wakeup(ms * 1000);
 *     esp_light_sleep_start();
 *     return ms;
 * }
 * @endcode
 */

/* ========================================================================== */
/*                         Internal Functions (for library use)               */
/* ========================================================================== */

/**
 * @brief Internal function to invoke client idle callback
 * 
 * This is called automatically by mb_client_poll() when the client is idle.
 * Application code should not call this directly.
 * 
 * @param client Client instance
 * @return Actual sleep time in ms, or 0 if callback not invoked
 * @private
 */
mb_u32 mb_client_invoke_idle_callback_internal(void *client);

/**
 * @brief Internal function to invoke server idle callback
 * 
 * This is called automatically by mb_server_poll() when the server is idle.
 * Application code should not call this directly.
 * 
 * @param server Server instance
 * @return Actual sleep time in ms, or 0 if callback not invoked
 * @private
 */
mb_u32 mb_server_invoke_idle_callback_internal(void *server);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_POWER_H */
