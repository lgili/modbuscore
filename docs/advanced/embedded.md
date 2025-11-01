# Embedded Systems Guide

Complete guide to deploying ModbusCore on embedded platforms.

## Table of Contents

- [Overview](#overview)
- [Platform Support](#platform-support)
- [Bare-Metal Integration](#bare-metal-integration)
- [FreeRTOS Integration](#freertos-integration)
- [Zephyr Integration](#zephyr-integration)
- [Memory Footprint](#memory-footprint)
- [Power Management](#power-management)
- [Optimization Techniques](#optimization-techniques)
- [Examples](#examples)

## Overview

ModbusCore is designed for **resource-constrained embedded systems** with:

- **Zero heap allocations** (no malloc/free)
- **Minimal memory footprint** (~12 KB flash, ~2 KB RAM)
- **Portable C99** (no platform-specific dependencies)
- **Configurable features** (disable unused functionality)
- **RTOS-friendly** (thread-safe with proper guards)

**Target Platforms:**
- ARM Cortex-M (M0, M3, M4, M7)
- RISC-V
- Xtensa (ESP32)
- AVR (limited support)
- Bare-metal, FreeRTOS, Zephyr

---

## Platform Support

### Tier 1: Fully Supported

| Platform | Architecture | RAM | Flash | Status |
|----------|--------------|-----|-------|--------|
| ARM Cortex-M4 | ARMv7E-M | 64+ KB | 256+ KB | ✅ Tested |
| ARM Cortex-M7 | ARMv7E-M | 128+ KB | 512+ KB | ✅ Tested |
| ESP32 | Xtensa LX6 | 320 KB | 4 MB | ✅ Tested |

### Tier 2: Community Supported

| Platform | Architecture | RAM | Flash | Status |
|----------|--------------|-----|-------|--------|
| ARM Cortex-M3 | ARMv7-M | 32+ KB | 128+ KB | ⚙️ Community |
| ARM Cortex-M0+ | ARMv6-M | 16+ KB | 64+ KB | ⚙️ Community |
| RISC-V | RV32I | 32+ KB | 128+ KB | ⚙️ Community |

### Minimum Requirements

- **Flash:** 16 KB (protocol only) - 64 KB (full features)
- **RAM:** 2 KB (minimal) - 8 KB (typical)
- **CPU:** 16 MHz+ recommended
- **Compiler:** GCC 7+, Clang 8+, ARM Compiler 6+

---

## Bare-Metal Integration

### Project Structure

```
my-firmware/
├── CMakeLists.txt
├── main.c
├── modbus_port.c          # Platform-specific code
├── modbus_port.h
└── modbuscore/            # ModbusCore library
```

### CMake Configuration

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(my-firmware C)

# Toolchain settings
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)

# Compiler flags for Cortex-M4
add_compile_options(
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    -Os                     # Optimize for size
    -ffunction-sections
    -fdata-sections
    -Wall
)

# Linker flags
add_link_options(
    -mcpu=cortex-m4
    -mthumb
    -specs=nosys.specs      # No system calls
    -Wl,--gc-sections       # Remove unused sections
)

# Add ModbusCore
add_subdirectory(modbuscore)
target_link_libraries(my-firmware PRIVATE modbuscore)
```

### Custom Transport (UART)

```c
// modbus_port.c
#include <modbuscore/transport/iface.h>
#include "stm32f4xx_hal.h"  // Or your MCU HAL

extern UART_HandleTypeDef huart1;

typedef struct {
    UART_HandleTypeDef *huart;
    uint8_t rx_buffer[256];
    size_t rx_index;
} uart_ctx_t;

mbc_status_t uart_send(void *ctx, const uint8_t *buffer, size_t length,
                       mbc_transport_io_t *out) {
    uart_ctx_t *uart = (uart_ctx_t*)ctx;
    
    HAL_StatusTypeDef status = HAL_UART_Transmit(uart->huart, 
                                                  (uint8_t*)buffer,
                                                  length, 
                                                  1000);
    
    if (status != HAL_OK) {
        return MBC_STATUS_TRANSPORT_ERROR;
    }
    
    if (out) {
        out->processed = length;
    }
    
    return MBC_STATUS_OK;
}

mbc_status_t uart_receive(void *ctx, uint8_t *buffer, size_t capacity,
                          mbc_transport_io_t *out) {
    uart_ctx_t *uart = (uart_ctx_t*)ctx;
    
    // Non-blocking receive
    if (uart->rx_index == 0) {
        return MBC_STATUS_WOULD_BLOCK;
    }
    
    size_t to_copy = (uart->rx_index < capacity) ? uart->rx_index : capacity;
    memcpy(buffer, uart->rx_buffer, to_copy);
    
    if (out) {
        out->processed = to_copy;
    }
    
    uart->rx_index = 0;  // Reset buffer
    
    return MBC_STATUS_OK;
}

uint64_t uart_now(void *ctx) {
    return HAL_GetTick();  // Milliseconds
}

// Create transport interface
mbc_transport_iface_t create_uart_transport(void) {
    static uart_ctx_t ctx = {
        .huart = &huart1,
        .rx_index = 0
    };
    
    mbc_transport_iface_t iface = {
        .ctx = &ctx,
        .send = uart_send,
        .receive = uart_receive,
        .now = uart_now,
        .yield = NULL
    };
    
    return iface;
}

// UART RX interrupt handler
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart1) {
        uart_ctx_t *ctx = /* get context */;
        
        // Store received byte
        if (ctx->rx_index < sizeof(ctx->rx_buffer)) {
            // Byte already in rx_buffer via DMA
            ctx->rx_index++;
        }
        
        // Re-enable interrupt
        HAL_UART_Receive_IT(huart, &ctx->rx_buffer[ctx->rx_index], 1);
    }
}
```

### Main Application

```c
// main.c
#include <modbuscore/runtime/builder.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/rtu.h>
#include "modbus_port.h"

int main(void) {
    // Initialize MCU (clock, peripherals, etc.)
    HAL_Init();
    SystemClock_Config();
    MX_UART1_Init();
    
    // Create Modbus transport
    mbc_transport_iface_t iface = create_uart_transport();
    
    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    // Main loop
    while (1) {
        // Build request
        mbc_pdu_t pdu;
        mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
        
        // Encode RTU frame
        uint8_t buffer[260];
        size_t length;
        mbc_rtu_encode(1, &pdu, buffer, sizeof(buffer), &length);
        
        // Send
        mbc_transport_send(&iface, buffer, length, NULL);
        
        // Wait and process response...
        HAL_Delay(1000);
    }
}
```

---

## FreeRTOS Integration

### Task Structure

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <modbuscore/runtime/builder.h>

// Modbus task handle
TaskHandle_t modbus_task_handle;
SemaphoreHandle_t modbus_mutex;

void modbus_task(void *pvParameters) {
    // Create transport
    mbc_transport_iface_t iface = create_uart_transport();
    
    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    TickType_t last_wake = xTaskGetTickCount();
    
    while (1) {
        // Acquire mutex for thread-safe operation
        if (xSemaphoreTake(modbus_mutex, portMAX_DELAY) == pdTRUE) {
            // Perform Modbus operations
            mbc_pdu_t pdu;
            mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
            
            // Send request...
            
            xSemaphoreGive(modbus_mutex);
        }
        
        // Delay 100ms (10 Hz polling)
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

int main(void) {
    // Initialize hardware
    HAL_Init();
    SystemClock_Config();
    
    // Create mutex
    modbus_mutex = xSemaphoreCreateMutex();
    
    // Create Modbus task
    xTaskCreate(modbus_task,
                "ModbusTask",
                512,           // Stack size (words)
                NULL,
                tskIDLE_PRIORITY + 2,
                &modbus_task_handle);
    
    // Start scheduler
    vTaskStartScheduler();
    
    while (1) {
        // Should never reach here
    }
}
```

### Memory Management

```c
// Custom FreeRTOS allocator
void *freertos_alloc(void *ctx, size_t size) {
    return pvPortMalloc(size);
}

void freertos_free(void *ctx, void *ptr) {
    vPortFree(ptr);
}

mbc_allocator_iface_t allocator = {
    .ctx = NULL,
    .alloc = freertos_alloc,
    .free = freertos_free
};

mbc_runtime_builder_with_allocator(&builder, &allocator);
```

### Cooperative Yield

```c
void freertos_yield(void *ctx) {
    taskYIELD();
}

mbc_transport_iface_t iface = {
    // ...
    .yield = freertos_yield
};
```

---

## Zephyr Integration

### Project Configuration

```c
// prj.conf
CONFIG_MODBUS=y
CONFIG_MODBUS_CLIENT=y
CONFIG_MODBUS_RTU=y
CONFIG_SERIAL=y
CONFIG_UART_INTERRUPT_DRIVEN=y
CONFIG_LOG=y
```

### Application

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <modbuscore/runtime/builder.h>

const struct device *uart_dev;

mbc_status_t zephyr_uart_send(void *ctx, const uint8_t *buffer, size_t length,
                               mbc_transport_io_t *out) {
    const struct device *dev = (const struct device*)ctx;
    
    for (size_t i = 0; i < length; i++) {
        uart_poll_out(dev, buffer[i]);
    }
    
    if (out) {
        out->processed = length;
    }
    
    return MBC_STATUS_OK;
}

mbc_status_t zephyr_uart_receive(void *ctx, uint8_t *buffer, size_t capacity,
                                  mbc_transport_io_t *out) {
    const struct device *dev = (const struct device*)ctx;
    
    int ret = uart_poll_in(dev, buffer);
    
    if (ret < 0) {
        return MBC_STATUS_WOULD_BLOCK;
    }
    
    if (out) {
        out->processed = 1;
    }
    
    return MBC_STATUS_OK;
}

uint64_t zephyr_now(void *ctx) {
    return k_uptime_get();
}

void main(void) {
    // Get UART device
    uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));
    
    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready\n");
        return;
    }
    
    // Create transport
    mbc_transport_iface_t iface = {
        .ctx = (void*)uart_dev,
        .send = zephyr_uart_send,
        .receive = zephyr_uart_receive,
        .now = zephyr_now,
        .yield = k_yield
    };
    
    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    // Main loop
    while (1) {
        // Modbus operations...
        
        k_sleep(K_MSEC(100));
    }
}
```

---

## Memory Footprint

### Flash Usage (ARM Cortex-M4, -Os)

| Component | Size | Notes |
|-----------|------|-------|
| PDU encoding/parsing | ~2 KB | Core protocol |
| MBAP encoding/decoding | ~1 KB | TCP framing |
| RTU encoding/CRC | ~2 KB | Serial framing |
| Transport abstraction | ~500 bytes | Interface layer |
| Runtime builder | ~1 KB | Dependency management |
| Diagnostics (optional) | ~2 KB | Logging, tracing |
| **Total (minimal)** | **~7 KB** | Protocol only |
| **Total (typical)** | **~12 KB** | Full features |

### RAM Usage

| Component | Size | Notes |
|-----------|------|-------|
| PDU structure | ~260 bytes | Stack allocation |
| TX/RX buffers | ~520 bytes | 2x 260 bytes |
| Runtime context | ~200 bytes | State management |
| Transport context | ~512 bytes | Driver state |
| **Total (minimal)** | **~1.5 KB** | Basic operation |
| **Total (typical)** | **~2 KB** | With diagnostics |

### Optimization

```cmake
# Reduce footprint
target_compile_definitions(modbuscore PRIVATE
    MBC_DISABLE_DIAGNOSTICS=1    # Remove diagnostics
    MBC_DISABLE_AUTO_HEAL=1      # Remove auto-heal
    MBC_PDU_MAX_SIZE=128         # Smaller buffers
)

target_compile_options(modbuscore PRIVATE
    -Os                          # Optimize for size
    -flto                        # Link-time optimization
    -ffunction-sections
    -fdata-sections
)

target_link_options(modbuscore PRIVATE
    -Wl,--gc-sections            # Remove unused code
)
```

---

## Power Management

### Sleep Modes

```c
// Poll with sleep
void modbus_poll_with_sleep(void) {
    static uint64_t last_poll_ms = 0;
    uint64_t now_ms = HAL_GetTick();
    
    if (now_ms - last_poll_ms >= 100) {  // 100ms interval
        // Perform Modbus operation
        modbus_transaction();
        
        last_poll_ms = now_ms;
    } else {
        // Enter low-power mode
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    }
}
```

### Interrupt-Driven I/O

```c
// Use interrupts instead of polling
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    // Wake from sleep on data reception
    // Process received byte
    // Re-enable interrupt
}

// Main loop can sleep
while (1) {
    if (data_available) {
        process_modbus_frame();
        data_available = false;
    } else {
        // Deep sleep (wake on UART interrupt)
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    }
}
```

---

## Optimization Techniques

### Stack Reduction

```c
// ❌ Bad: Large stack usage
void process_request(void) {
    uint8_t buffer[260];  // 260 bytes on stack
    mbc_pdu_t pdu;        // 260 bytes on stack
    // Total: 520 bytes
}

// ✅ Good: Reuse static buffers
static uint8_t shared_buffer[260];
static mbc_pdu_t shared_pdu;

void process_request(void) {
    // Zero stack usage (global/static)
}
```

### Function Inlining

```c
// Force inline for critical paths
#define ALWAYS_INLINE __attribute__((always_inline)) inline

ALWAYS_INLINE
static uint16_t read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}
```

### Const Data in Flash

```c
// Store lookup tables in flash (not RAM)
static const uint16_t crc_table[256] __attribute__((section(".rodata"))) = {
    0x0000, 0xC0C1, 0xC181, 0x0140, /* ... */
};
```

---

## Examples

### STM32 Bare-Metal (RTU Master)

```c
#include "stm32f4xx_hal.h"
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/rtu.h>

UART_HandleTypeDef huart1;

int main(void) {
    HAL_Init();
    SystemClock_Config();
    
    // Configure UART: 19200 baud, 8N1
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 19200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    HAL_UART_Init(&huart1);
    
    while (1) {
        // Build request
        mbc_pdu_t pdu;
        mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
        
        // Encode RTU frame
        uint8_t tx_buffer[260];
        size_t tx_length;
        mbc_rtu_encode(1, &pdu, tx_buffer, sizeof(tx_buffer), &tx_length);
        
        // Send
        HAL_UART_Transmit(&huart1, tx_buffer, tx_length, 1000);
        
        // Receive
        uint8_t rx_buffer[260];
        HAL_UART_Receive(&huart1, rx_buffer, sizeof(rx_buffer), 1000);
        
        // Decode
        uint8_t unit_id;
        mbc_pdu_t resp_pdu;
        if (mbc_rtu_decode(rx_buffer, 20, &unit_id, &resp_pdu) == MBC_STATUS_OK) {
            // Parse response
            uint16_t values[10];
            size_t count;
            mbc_pdu_parse_read_response(&resp_pdu, values, 10, &count);
            
            // Use values...
        }
        
        HAL_Delay(1000);
    }
}
```

### ESP32 (FreeRTOS + WiFi)

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_tcp.h>

static const char *TAG = "modbus";

void modbus_task(void *pvParameters) {
    // Wait for WiFi connection
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Create TCP transport
    mbc_posix_tcp_config_t cfg = {
        .host = "192.168.1.100",
        .port = 502,
        .connect_timeout_ms = 5000,
        .recv_timeout_ms = 1000
    };
    
    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t *ctx;
    
    if (mbc_posix_tcp_create(&cfg, &iface, &ctx) != MBC_STATUS_OK) {
        ESP_LOGE(TAG, "Failed to connect");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Connected to Modbus server");
    
    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    // Main loop
    while (1) {
        // Modbus operations...
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(ctx);
    vTaskDelete(NULL);
}

void app_main(void) {
    // Initialize WiFi, NVS, etc.
    // ...
    
    // Create Modbus task
    xTaskCreate(modbus_task, "modbus_task", 4096, NULL, 5, NULL);
}
```

---

## Best Practices

### ✅ Embedded Checklist

- [ ] Compile with size optimization (`-Os`)
- [ ] Enable link-time optimization (`-flto`)
- [ ] Use static/global buffers (not stack)
- [ ] Disable unused features (diagnostics, etc.)
- [ ] Profile memory usage
- [ ] Test on target hardware
- [ ] Use interrupt-driven I/O
- [ ] Implement power management
- [ ] Handle stack overflow (MPU)
- [ ] Use watchdog timer

### ⚠️ Common Pitfalls

- **Stack overflow** - Use static buffers
- **Excessive polling** - Use interrupts
- **Blocking calls** - Use non-blocking I/O
- **Large arrays** - Place in flash (.rodata)
- **Heap fragmentation** - Avoid malloc/free

---

## Next Steps

- [Performance Tuning](performance.md) - Optimize for your platform
- [Custom Transports](custom-transports.md) - Implement platform-specific drivers
- [Testing Guide](../user-guide/testing.md) - Test on embedded hardware

---

**Prev**: [Security ←](security.md) | **Next**: [Fuzzing →](fuzzing.md)
