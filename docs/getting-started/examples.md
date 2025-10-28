# Examples

Ready-to-run examples demonstrating common use cases.

## Table of Contents

- [Overview](#overview)
- [Basic Examples](#basic-examples)
- [Advanced Examples](#advanced-examples)
- [Embedded Examples](#embedded-examples)
- [Testing Examples](#testing-examples)

## Overview

All examples are located in the [`examples/`](../../examples/) directory and build automatically with the library.

### Running Examples

```bash
# Build all examples
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build

# Run an example
./build/examples/tcp_client_cli
```

## Basic Examples

### TCP Client CLI

**File:** `examples/tcp_client_cli.c`

Interactive TCP client with command-line interface:

```bash
./build/examples/tcp_client_cli 127.0.0.1 502

> read holding 0 10
Register 0: 100
Register 1: 200
...

> write single 5 123
✓ Write successful

> quit
```

**Features:**
- Interactive REPL
- Read holding/input/coils/discrete inputs
- Write single/multiple registers
- Connection management

**Use Case:** Testing and debugging Modbus devices

---

### TCP Server Demo

**File:** `examples/tcp_server_demo.c`

Simple TCP server with simulated device data:

```bash
sudo ./build/examples/tcp_server_demo
🚀 Server started on port 502
📡 Client connected from 127.0.0.1:54321
   RX: Read Holding Registers (addr=0, count=10)
   TX: 27 bytes
```

**Features:**
- Multi-client support (sequential)
- Simulated register/coil maps
- Request logging
- Exception handling

**Use Case:** Development testing without real hardware

---

### RTU Serial Client

**File:** `examples/rtu_serial_client.c`

RTU client over RS-485/RS-232:

```c
// Configuration
const char *port = "/dev/ttyUSB0";
int baudrate = 9600;
uint8_t slave_addr = 1;

// Read holding registers
mbc_pdu_t req, resp;
mbc_pdu_build_read_holding_request(&req, slave_addr, 0, 10);

// Send over serial
uint8_t tx_buf[256];
size_t tx_len;
mbc_rtu_encode(slave_addr, &req, tx_buf, sizeof(tx_buf), &tx_len);
write(serial_fd, tx_buf, tx_len);
```

**Features:**
- Serial port configuration
- RTU frame encoding/decoding
- CRC validation
- Timeout handling

**Use Case:** Field device communication

---

### RTU Serial Server

**File:** `examples/rtu_serial_server.c`

RTU server (slave) on serial port:

```bash
./build/examples/rtu_serial_server /dev/ttyUSB0 9600
🔌 RTU Server on /dev/ttyUSB0 (9600 8N1)
📥 Request from slave 1: Read Holding (0, 10)
📤 Response: 27 bytes
```

**Features:**
- Serial listener
- Request dispatching
- Simulated register map
- Exception responses

**Use Case:** Embedded device simulation

---

## Advanced Examples

### Multi-Client TCP Server

**File:** `examples/tcp_multi_client_demo.c`

Concurrent client handling with threads:

```c
// Server configuration
#define MAX_CLIENTS 10

typedef struct {
    int socket;
    struct sockaddr_in address;
    pthread_t thread;
} client_context_t;

// Handle each client in separate thread
void *client_handler(void *arg) {
    client_context_t *ctx = arg;
    // Process requests...
}
```

**Features:**
- Thread pool
- Connection timeout
- Client statistics
- Load balancing

**Use Case:** Production servers with multiple clients

---

### Auto-Heal Demo

**File:** `examples/auto_heal_demo.c` (when built with `-DENABLE_AUTO_HEAL=ON`)

Automatic error recovery:

```c
// Configure auto-heal
mbc_auto_heal_config_t heal_cfg = {
    .enable_frame_repair = true,
    .enable_retry = true,
    .max_retries = 3,
    .retry_delay_ms = 100
};
mbc_auto_heal_init(&heal_cfg);

// Corrupted frame is automatically repaired
uint8_t corrupt_frame[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x01, 0x03, 0xFF, 0xFF};
mbc_auto_heal_attempt_frame_repair(corrupt_frame, 10);
```

**Features:**
- Frame repair
- Retry logic
- Backoff strategies
- Recovery statistics

**Use Case:** Unreliable networks, noisy serial lines

---

### Power Management

**File:** `examples/power_idle_demo.c`

Low-power operation for battery devices:

```c
// Enter low-power mode when idle
mbc_power_config_t power_cfg = {
    .enable_sleep = true,
    .idle_timeout_ms = 5000,
    .wakeup_on_rx = true
};
mbc_power_init(&power_cfg);

// Library automatically sleeps between transactions
while (1) {
    mbc_runtime_poll(&runtime);  // Sleeps if idle > 5s
}
```

**Features:**
- Automatic sleep/wake
- Configurable timeouts
- Wake-on-RX
- Power consumption tracking

**Use Case:** Battery-powered devices, energy-efficient systems

---

### Diagnostics Demo

**File:** `examples/diagnostics_demo.c`

Comprehensive diagnostics and logging:

```c
// Enable all diagnostics
mbc_diag_config_t diag = {
    .enable_frame_tracing = true,
    .enable_error_logging = true,
    .enable_performance_stats = true,
    .trace_callback = my_trace_handler
};
mbc_diagnostics_init(&diag);

// Custom trace handler
void my_trace_handler(bool is_tx, const uint8_t *data, size_t len) {
    printf("[%s] ", is_tx ? "TX" : "RX");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}
```

**Features:**
- Frame tracing
- Performance counters
- Error logging
- Custom callbacks

**Use Case:** Development, production monitoring

---

## Embedded Examples

### FreeRTOS Client

**File:** `examples/freertos_rtu_client/` (directory)

Complete FreeRTOS project:

```c
void modbus_task(void *params) {
    mbc_runtime_config_t runtime = {0};
    mbc_runtime_init(&runtime);

    while (1) {
        // Periodic read
        mbc_pdu_t req = {0};
        mbc_pdu_build_read_holding_request(&req, 1, 0, 10);
        
        // Send via RTU transport
        // ...

        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second
    }
}

void app_main(void) {
    xTaskCreate(modbus_task, "modbus", 4096, NULL, 5, NULL);
}
```

**Features:**
- RTOS task integration
- Queue-based I/O
- Mutex protection
- ISR-safe mode

**Use Case:** STM32, ESP32, other RTOS platforms

---

### ESP32 RTU Client

**File:** `examples/esp32_rtu_client/` (ESP-IDF project)

ESP32 with UART:

```c
// Initialize UART
uart_config_t uart_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
};
uart_param_config(UART_NUM_1, &uart_config);
uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);

// Use ModbusCore with ESP-IDF
mbc_pdu_t pdu = {0};
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

uint8_t tx_buf[256];
size_t tx_len;
mbc_rtu_encode(1, &pdu, tx_buf, sizeof(tx_buf), &tx_len);
uart_write_bytes(UART_NUM_1, tx_buf, tx_len);
```

**Build:**
```bash
cd examples/esp32_rtu_client
idf.py build flash monitor
```

**Use Case:** ESP32-based Modbus gateways

---

### Zephyr RTU Client

**File:** `examples/zephyr_rtu_client/` (Zephyr project)

Zephyr RTOS integration:

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <modbuscore/protocol/pdu.h>

const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));

void main(void) {
    if (!device_is_ready(uart_dev)) {
        printk("UART not ready\n");
        return;
    }

    // ModbusCore with Zephyr
    mbc_pdu_t pdu = {0};
    mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    // Send via Zephyr UART API
}
```

**Build:**
```bash
cd examples/zephyr_rtu_client
west build -b nrf52840dk_nrf52840
west flash
```

**Use Case:** Nordic, NXP, STM32 with Zephyr

---

### Bare Metal (No OS)

**File:** `examples/bare_metal_rtu_client/`

Polling-based bare metal:

```c
// No RTOS - just polling
int main(void) {
    // Initialize hardware
    uart_init();
    timer_init();

    mbc_runtime_config_t runtime = {0};
    mbc_runtime_init(&runtime);

    while (1) {
        // Poll for incoming data
        if (uart_data_available()) {
            uint8_t byte = uart_read_byte();
            // Process...
        }

        // Periodic transmission
        if (timer_elapsed_ms() > 1000) {
            // Send Modbus request
            timer_reset();
        }
    }
}
```

**Features:**
- No heap allocation
- Polling I/O
- Timer-based scheduling
- Minimal footprint

**Use Case:** Resource-constrained MCUs (AVR, PIC)

---

## Testing Examples

### Loopback Test

**File:** `examples/unit_test_loop_demo.c`

Self-test without external device:

```c
// Loopback: encode -> decode
mbc_pdu_t req = {0};
mbc_pdu_build_read_holding_request(&req, 1, 0, 10);

uint8_t buffer[256];
size_t len;
mbc_rtu_encode(1, &req, buffer, sizeof(buffer), &len);

uint8_t slave;
mbc_pdu_t decoded = {0};
assert(mbc_rtu_decode(buffer, len, &slave, &decoded) == MBC_STATUS_OK);
assert(decoded.function == 0x03);
```

**Use Case:** CI/CD, automated testing

---

### Simulator

**File:** `examples/posix_sim.c`

Full TCP simulator with virtual devices:

```bash
./build/examples/posix_sim
🚀 Simulator started
   Virtual device 1: Temperature sensor
   Virtual device 2: Motor controller
   Virtual device 3: PLC

Listening on port 502...
```

**Features:**
- Multiple virtual slaves
- Realistic timing
- Simulated sensor data
- Network simulation

**Use Case:** Application testing, CI, demos

---

## Building Specific Examples

### Build Only TCP Examples

```bash
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build --target tcp_client_cli tcp_server_demo
```

### Build with Auto-Heal

```bash
cmake -B build -DENABLE_AUTO_HEAL=ON -DBUILD_EXAMPLES=ON
cmake --build build --target auto_heal_demo
```

### Build Embedded Examples

```bash
# FreeRTOS (requires FreeRTOS SDK)
cd examples/freertos_rtu_client
cmake -B build -DCMAKE_TOOLCHAIN_FILE=arm-toolchain.cmake
cmake --build build

# ESP32 (requires ESP-IDF)
cd examples/esp32_rtu_client
idf.py build

# Zephyr (requires Zephyr SDK)
cd examples/zephyr_rtu_client
west build -b <board>
```

## Example Matrix

| Example | TCP | RTU | Client | Server | RTOS | Bare Metal |
|---------|-----|-----|--------|--------|------|------------|
| `tcp_client_cli` | ✅ | ❌ | ✅ | ❌ | ❌ | ❌ |
| `tcp_server_demo` | ✅ | ❌ | ❌ | ✅ | ❌ | ❌ |
| `tcp_multi_client_demo` | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ |
| `rtu_serial_client` | ❌ | ✅ | ✅ | ❌ | ❌ | ✅ |
| `rtu_serial_server` | ❌ | ✅ | ❌ | ✅ | ❌ | ✅ |
| `freertos_rtu_client` | ❌ | ✅ | ✅ | ❌ | ✅ | ❌ |
| `esp32_rtu_client` | ❌ | ✅ | ✅ | ❌ | ✅ | ❌ |
| `zephyr_rtu_client` | ❌ | ✅ | ✅ | ❌ | ✅ | ❌ |
| `bare_metal_rtu_client` | ❌ | ✅ | ✅ | ❌ | ❌ | ✅ |
| `auto_heal_demo` | ✅ | ✅ | ✅ | ❌ | ❌ | ❌ |
| `power_idle_demo` | ✅ | ❌ | ✅ | ❌ | ❌ | ✅ |
| `diagnostics_demo` | ✅ | ❌ | ✅ | ❌ | ❌ | ❌ |
| `unit_test_loop_demo` | ❌ | ✅ | ✅ | ✅ | ❌ | ❌ |
| `posix_sim` | ✅ | ❌ | ❌ | ✅ | ❌ | ❌ |

## Next Steps

- [Architecture](../user-guide/architecture.md) - Understand the design
- [Transports](../user-guide/transports.md) - TCP vs RTU deep dive
- [API Reference](../api/README.md) - Complete API documentation

---

**Prev**: [First Application ←](first-application.md) | **Next**: [Architecture →](../user-guide/architecture.md)
