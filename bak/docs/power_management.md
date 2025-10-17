# Power Management & Low-Power Mode

This guide explains how to use the power management features to reduce energy consumption in battery-powered applications.

## Overview

ModbusCore provides idle callbacks that allow your application to enter low-power modes when the protocol stack is idle. This is essential for:

- **Battery-powered devices** - Extend battery life significantly
- **Tickless RTOS** - Integrate with FreeRTOS, Zephyr, ThreadX tickless modes
- **Energy-efficient systems** - Reduce power consumption during idle periods
- **Industrial IoT** - Meet strict power budgets

## Key Features

- ✅ **Zero busy-waiting** - No CPU cycles wasted polling
- ✅ **Configurable thresholds** - Fine-tune when to enter sleep
- ✅ **Platform-agnostic API** - Works with any MCU or RTOS
- ✅ **Zero overhead when disabled** - Compile-time option
- ✅ **Safe wake-up** - No lost frames or data corruption

## Quick Start

### 1. Enable Power Management (Optional)

Power management is enabled by default. To disable it:

```c
// In your project's configuration file
#define MB_CONF_ENABLE_POWER_MANAGEMENT 0
```

### 2. Implement Idle Callback

```c
#include <modbus/mb_power.h>

// Simple WFI (Wait For Interrupt) callback
uint32_t my_idle_callback(void *user_ctx, uint32_t sleep_ms) {
    // Enter low-power mode for suggested duration
    __WFI();  // ARM Cortex-M Wait For Interrupt
    return 1; // We slept for ~1ms (interrupt latency)
}
```

### 3. Register Callback

```c
// For Modbus client
mb_client_set_idle_callback(&client, my_idle_callback, NULL, 5);
//                                    callback          ctx   threshold_ms

// For Modbus server
mb_server_set_idle_callback(&server, my_idle_callback, NULL, 5);
```

### 4. Poll as Normal

```c
// The callback is automatically invoked when idle
while (running) {
    mb_client_poll(&client);  // Callback called if idle > 5ms
    // or
    mb_server_poll(&server);
}
```

## Platform Examples

### STM32 - STOP Mode

Enter STM32 STOP mode for deep sleep with RTC wake-up:

```c
#include "stm32f4xx_hal.h"

uint32_t stm32_stop_idle(void *ctx, uint32_t sleep_ms) {
    if (sleep_ms < 10) {
        // Too short for STOP mode overhead
        __WFI();
        return 1;
    }
    
    // Configure RTC wake-up timer
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, sleep_ms, 
                                RTC_WAKEUPCLOCK_RTCCLK_DIV16);
    
    // Enter STOP mode (lowest power with wake-up)
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    
    // After wake-up, reconfigure clocks
    SystemClock_Config();
    
    // Return actual sleep time
    uint32_t actual = HAL_RTCEx_GetWakeUpTimer(&hrtc);
    return actual;
}

// Register with client
mb_client_set_idle_callback(&client, stm32_stop_idle, &hrtc, 10);
```

### FreeRTOS - Tickless Idle

Integrate with FreeRTOS automatic tickless idle:

```c
#include "FreeRTOS.h"
#include "task.h"

uint32_t freertos_idle(void *ctx, uint32_t sleep_ms) {
    // Let FreeRTOS handle sleep automatically
    vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    return sleep_ms;
}

mb_client_set_idle_callback(&client, freertos_idle, NULL, 5);
```

### Zephyr - Low-Power Mode

Use Zephyr's power management subsystem:

```c
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>

uint32_t zephyr_idle(void *ctx, uint32_t sleep_ms) {
    // Zephyr automatically selects best power state
    k_sleep(K_MSEC(sleep_ms));
    return sleep_ms;
}

mb_client_set_idle_callback(&client, zephyr_idle, NULL, 5);
```

### ESP32 - Light Sleep

ESP32 light sleep with automatic wake-up:

```c
#include "esp_sleep.h"
#include "driver/rtc_io.h"

uint32_t esp32_light_sleep(void *ctx, uint32_t sleep_ms) {
    if (sleep_ms < 5) {
        // Too short for sleep overhead
        return 0;
    }
    
    // Configure timer wake-up
    esp_sleep_enable_timer_wakeup(sleep_ms * 1000);  // µs
    
    // Enter light sleep
    esp_light_sleep_start();
    
    return sleep_ms;
}

mb_client_set_idle_callback(&client, esp32_light_sleep, NULL, 5);
```

## Multi-Level Power Saving

Implement different sleep modes based on duration:

```c
typedef struct {
    RTC_HandleTypeDef *rtc;
    uint32_t wfi_count;
    uint32_t sleep_count;
    uint32_t stop_count;
} power_context_t;

uint32_t multi_level_idle(void *user_ctx, uint32_t sleep_ms) {
    power_context_t *ctx = (power_context_t *)user_ctx;
    
    if (sleep_ms < 2) {
        // Very short - just WFI
        __WFI();
        ctx->wfi_count++;
        return 1;
    } 
    else if (sleep_ms < 20) {
        // Short - Sleep mode (peripherals on)
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
        ctx->sleep_count++;
        return sleep_ms;
    } 
    else {
        // Long - STOP mode (deep sleep)
        HAL_RTCEx_SetWakeUpTimer_IT(ctx->rtc, sleep_ms, 
                                     RTC_WAKEUPCLOCK_RTCCLK_DIV16);
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        SystemClock_Config();
        ctx->stop_count++;
        return sleep_ms;
    }
}

// Usage
power_context_t power_ctx = { .rtc = &hrtc };
mb_client_set_idle_callback(&client, multi_level_idle, &power_ctx, 2);
```

## API Reference

### Register Idle Callback

```c
mb_err_t mb_client_set_idle_callback(
    void *client,                    // Client instance
    mb_idle_callback_t callback,     // Callback function (NULL to disable)
    void *user_ctx,                  // User context passed to callback
    uint32_t threshold_ms            // Minimum idle time to invoke callback
);

mb_err_t mb_server_set_idle_callback(
    void *server,                    // Server instance
    mb_idle_callback_t callback,     // Callback function (NULL to disable)
    void *user_ctx,                  // User context passed to callback
    uint32_t threshold_ms            // Minimum idle time to invoke callback
);
```

**Parameters:**
- `callback` - Function called when idle. Pass `NULL` to disable.
- `user_ctx` - User data passed to callback (e.g., hardware handles).
- `threshold_ms` - Minimum idle duration before invoking callback.
  - Recommended: `5-10ms` for most applications
  - Lower values = more frequent callbacks
  - Higher values = longer continuous idle periods

**Returns:** `MB_OK` on success, `MB_ERR_INVALID_ARGUMENT` if client/server is NULL.

### Callback Function Signature

```c
typedef uint32_t (*mb_idle_callback_t)(void *user_ctx, uint32_t sleep_ms);
```

**Parameters:**
- `user_ctx` - User context from registration
- `sleep_ms` - Suggested sleep duration (time until next event)

**Returns:** Actual sleep duration in milliseconds
- Return `0` if sleep was not performed
- Return actual time slept if different from suggested

### Query Idle Configuration

```c
mb_err_t mb_client_get_idle_config(
    const void *client,
    mb_idle_config_t *config         // Output: current configuration
);

mb_err_t mb_server_get_idle_config(
    const void *server,
    mb_idle_config_t *config
);
```

### Check Idle Status

```c
// Check if client/server is currently idle
bool mb_client_is_idle(const mb_client_t *client);
bool mb_server_is_idle(const mb_server_t *server);

// Get time until next scheduled event
uint32_t mb_client_time_until_next_event(const void *client);
uint32_t mb_server_time_until_next_event(const void *server);
```

## Best Practices

### ✅ Do's

1. **Match threshold to your power mode overhead**
   ```c
   // WFI: Very low overhead, can use 1-2ms
   mb_client_set_idle_callback(&client, wfi_callback, NULL, 2);
   
   // STOP mode: Higher overhead, use 10-20ms
   mb_client_set_idle_callback(&client, stop_callback, NULL, 10);
   ```

2. **Return actual sleep time**
   ```c
   uint32_t my_callback(void *ctx, uint32_t sleep_ms) {
       uint32_t start = get_timestamp();
       enter_sleep(sleep_ms);
       return get_timestamp() - start;  // Actual time
   }
   ```

3. **Handle interrupts correctly**
   ```c
   // Ensure Modbus RX interrupt can wake from sleep
   HAL_UART_Receive_IT(&huart, rx_buffer, 1);
   ```

4. **Use context for statistics**
   ```c
   typedef struct {
       uint32_t sleep_count;
       uint32_t total_sleep_ms;
   } stats_t;
   
   uint32_t callback(void *ctx, uint32_t ms) {
       stats_t *stats = (stats_t *)ctx;
       stats->sleep_count++;
       stats->total_sleep_ms += ms;
       // ... enter sleep
   }
   ```

### ❌ Don'ts

1. **Don't sleep too long without wake-up mechanism**
   ```c
   // BAD: No way to wake up for incoming data
   uint32_t bad_callback(void *ctx, uint32_t sleep_ms) {
       HAL_Delay(sleep_ms);  // Blocks everything!
       return sleep_ms;
   }
   ```

2. **Don't forget to reconfigure clocks after STOP**
   ```c
   // BAD: Clocks corrupted after STOP mode
   HAL_PWR_EnterSTOPMode(...);
   // GOOD: Reconfigure
   SystemClock_Config();
   ```

3. **Don't use very low thresholds with high-overhead modes**
   ```c
   // BAD: STOP mode overhead > 1ms, wastes energy
   mb_client_set_idle_callback(&client, stop_callback, NULL, 1);
   ```

## Troubleshooting

### Callback never invoked

**Problem:** Idle callback is not being called.

**Solutions:**
1. Check threshold is not too high:
   ```c
   mb_client_set_idle_callback(&client, callback, NULL, 5);  // Not 5000!
   ```

2. Verify client/server is actually idle:
   ```c
   if (mb_client_is_idle(&client)) {
       printf("Client is idle\n");
   }
   ```

3. Ensure callback is enabled:
   ```c
   mb_idle_config_t config;
   mb_client_get_idle_config(&client, &config);
   printf("Enabled: %d\n", config.enabled);
   ```

### System not waking up

**Problem:** Device enters sleep but doesn't wake for Modbus traffic.

**Solutions:**
1. Enable UART RX interrupt wake-up:
   ```c
   HAL_UART_Receive_IT(&huart, rx_buffer, 1);
   ```

2. Configure GPIO wake-up if using RS-485:
   ```c
   HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_RX);
   ```

3. Check RTC/timer wake-up is configured:
   ```c
   HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, sleep_ms, ...);
   ```

### High current consumption

**Problem:** Power consumption is still high despite sleep mode.

**Solutions:**
1. Verify sleep mode is actually entered:
   ```c
   uint32_t callback(void *ctx, uint32_t ms) {
       GPIO_SET_DEBUG_PIN();  // Debug: visualize with oscilloscope
       enter_sleep(ms);
       GPIO_CLEAR_DEBUG_PIN();
       return ms;
   }
   ```

2. Disable unused peripherals before sleep:
   ```c
   HAL_SPI_DeInit(&hspi1);
   HAL_I2C_DeInit(&hi2c1);
   ```

3. Check for GPIO current leakage:
   ```c
   HAL_GPIO_WritePin(UNUSED_PORT, UNUSED_PIN, GPIO_PIN_RESET);
   ```

## Performance Metrics

Typical power savings with idle callbacks:

| Mode | Current | Wake-up | Use Case |
|------|---------|---------|----------|
| **Active** | 50-100 mA | - | Processing Modbus |
| **WFI** | 30-50 mA | <10 µs | Idle < 5ms |
| **Sleep** | 5-15 mA | <100 µs | Idle 5-20ms |
| **STOP** | 0.5-3 mA | <1 ms | Idle > 20ms |
| **Standby** | <10 µA | <100 ms | Long idle (backup only) |

**Example savings:**
- RTU client polling every 100ms: **70% power reduction** with STOP mode
- RTU server (reactive): **90% power reduction** with STOP mode
- TCP keep-alive every 5s: **95% power reduction** with STOP mode

## Configuration Options

```c
// Disable power management at compile time (saves ~64 bytes RAM)
#define MB_CONF_ENABLE_POWER_MANAGEMENT 0

// Customize in your project
typedef struct {
    mb_idle_callback_t callback;  // Your idle callback
    void *user_ctx;               // Your context
    uint32_t threshold_ms;        // Your threshold
    bool enabled;                 // Runtime enable/disable
} mb_idle_config_t;
```

## See Also

- [QoS & Backpressure](qos_backpressure.md) - Flow control for reliable operation
- [ISR-Safe Mode](isr_safe_mode.md) - Using Modbus from interrupts
- [Zero-Copy I/O](zero_copy_io.md) - Efficient memory usage
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common issues

## Support

For questions or issues with power management:
- Open an issue on GitHub
- Check existing examples in `examples/power_idle_demo.c`
- Review test cases in `tests/test_mb_power.cpp`
