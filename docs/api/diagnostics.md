# Diagnostics API

Complete reference for ModbusCore diagnostics and logging functions.

## Table of Contents

- [Overview](#overview)
- [Configuration](#configuration)
- [Frame Tracing](#frame-tracing)
- [Counters](#counters)
- [Error Logging](#error-logging)
- [Performance Stats](#performance-stats)

## Overview

ModbusCore diagnostics provide comprehensive visibility into protocol operations:

- **Frame tracing** - Log all TX/RX frames
- **Counters** - Track requests, responses, errors
- **Performance** - Measure timing and throughput
- **Error logging** - Detailed error information

**Header:** `<modbuscore/diagnostics.h>`

## Configuration

### mbc_diagnostics_init

Initialize diagnostics subsystem.

```c
mbc_status_t mbc_diagnostics_init(const mbc_diag_config_t *config);
```

**Parameters:**
```c
typedef struct {
    bool enable_frame_tracing;      // Enable TX/RX frame logging
    bool enable_error_logging;      // Enable error logging
    bool enable_performance_stats;  // Enable timing measurements
    mbc_trace_callback_t trace_callback;  // Custom trace handler (optional)
    void *user_context;             // User context for callbacks
} mbc_diag_config_t;
```

**Example:**
```c
mbc_diag_config_t cfg = {
    .enable_frame_tracing = true,
    .enable_error_logging = true,
    .enable_performance_stats = true,
    .trace_callback = my_trace_handler,
    .user_context = NULL
};

mbc_diagnostics_init(&cfg);
```

---

### mbc_diagnostics_enable

Enable/disable diagnostics at runtime.

```c
void mbc_diagnostics_enable(bool enable);
```

**Example:**
```c
// Disable during normal operation
mbc_diagnostics_enable(false);

// Enable when debugging
mbc_diagnostics_enable(true);
```

---

## Frame Tracing

### mbc_diagnostics_trace_frame

Trace a Modbus frame.

```c
void mbc_diagnostics_trace_frame(
    bool is_tx,
    const uint8_t *data,
    size_t length
);
```

**Parameters:**
- `is_tx` - true for TX (outgoing), false for RX (incoming)
- `data` - Frame data
- `length` - Frame length in bytes

**Example:**
```c
uint8_t tx_buffer[260];
size_t tx_length;

mbc_mbap_encode(&hdr, &pdu, tx_buffer, sizeof(tx_buffer), &tx_length);

// Trace outgoing frame
mbc_diagnostics_trace_frame(true, tx_buffer, tx_length);

send(sock, tx_buffer, tx_length, 0);
```

**Output (default):**
```
[TX] 00 01 00 00 00 06 01 03 00 00 00 0A
```

---

### Custom Trace Callback

Implement custom frame tracing:

```c
typedef void (*mbc_trace_callback_t)(
    bool is_tx,
    const uint8_t *data,
    size_t length,
    void *user_context
);
```

**Example:**
```c
void my_trace_handler(bool is_tx, const uint8_t *data, size_t len, void *ctx) {
    FILE *log = (FILE*)ctx;
    
    fprintf(log, "[%s] ", is_tx ? "TX" : "RX");
    
    for (size_t i = 0; i < len; i++) {
        fprintf(log, "%02X ", data[i]);
    }
    
    fprintf(log, "\n");
    fflush(log);
}

// Usage
FILE *logfile = fopen("modbus.log", "a");

mbc_diag_config_t cfg = {
    .enable_frame_tracing = true,
    .trace_callback = my_trace_handler,
    .user_context = logfile
};

mbc_diagnostics_init(&cfg);
```

---

### mbc_diagnostics_trace_printf

Formatted trace logging.

```c
void mbc_diagnostics_trace_printf(const char *format, ...);
```

**Example:**
```c
mbc_diagnostics_trace_printf("Connecting to %s:%d\n", ip, port);
mbc_diagnostics_trace_printf("Transaction ID: %u\n", txid);
```

---

## Counters

### Counter Types

```c
typedef enum {
    MBC_DIAG_COUNTER_TX_FRAMES,      // Total frames transmitted
    MBC_DIAG_COUNTER_RX_FRAMES,      // Total frames received
    MBC_DIAG_COUNTER_ERRORS,         // Total errors
    MBC_DIAG_COUNTER_TIMEOUTS,       // Timeout errors
    MBC_DIAG_COUNTER_CRC_ERRORS,     // CRC errors (RTU)
    MBC_DIAG_COUNTER_EXCEPTIONS,     // Modbus exceptions
    MBC_DIAG_COUNTER_RETRIES,        // Retry attempts
    MBC_DIAG_COUNTER_MAX
} mbc_diag_counter_t;
```

---

### mbc_diagnostics_get_counter

Get counter value.

```c
uint32_t mbc_diagnostics_get_counter(mbc_diag_counter_t counter);
```

**Example:**
```c
uint32_t tx_count = mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_TX_FRAMES);
uint32_t rx_count = mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_RX_FRAMES);
uint32_t errors = mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_ERRORS);

printf("TX: %u, RX: %u, Errors: %u\n", tx_count, rx_count, errors);
```

---

### mbc_diagnostics_increment_counter

Increment a counter (internal use).

```c
void mbc_diagnostics_increment_counter(mbc_diag_counter_t counter);
```

**Example:**
```c
// Called automatically by library
if (send_failed) {
    mbc_diagnostics_increment_counter(MBC_DIAG_COUNTER_ERRORS);
}
```

---

### mbc_diagnostics_reset_counters

Reset all counters to zero.

```c
void mbc_diagnostics_reset_counters(void);
```

**Example:**
```c
// Reset at start of test
mbc_diagnostics_reset_counters();

// Run test...

// Print final stats
uint32_t tx = mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_TX_FRAMES);
printf("Test completed: %u frames sent\n", tx);
```

---

### mbc_diagnostics_print_counters

Print all counters (debug).

```c
void mbc_diagnostics_print_counters(void);
```

**Output:**
```
Diagnostics Counters:
  TX Frames:    1234
  RX Frames:    1230
  Errors:       4
  Timeouts:     2
  CRC Errors:   1
  Exceptions:   1
  Retries:      3
```

---

## Error Logging

### mbc_diagnostics_log_error

Log an error with context.

```c
void mbc_diagnostics_log_error(
    mbc_status_t status,
    const char *function,
    int line
);
```

**Parameters:**
- `status` - Error status code
- `function` - Function name where error occurred
- `line` - Line number

**Macro:**
```c
#define MBC_LOG_ERROR(status) \
    mbc_diagnostics_log_error(status, __func__, __LINE__)
```

**Example:**
```c
mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 200);
if (status != MBC_STATUS_OK) {
    MBC_LOG_ERROR(status);
    return status;
}
```

**Output:**
```
[ERROR] mbc_pdu_build_read_holding_request:123 - Invalid parameter (1)
```

---

### Error Log Callback

Custom error handler:

```c
typedef void (*mbc_error_callback_t)(
    mbc_status_t status,
    const char *function,
    int line,
    void *user_context
);
```

**Example:**
```c
void my_error_handler(mbc_status_t status, const char *func, int line, void *ctx) {
    syslog(LOG_ERR, "ModbusCore error in %s:%d - code %d", func, line, status);
}

mbc_diag_config_t cfg = {
    .enable_error_logging = true,
    .error_callback = my_error_handler
};
```

---

## Performance Stats

### mbc_diagnostics_start_timer

Start timing measurement.

```c
void mbc_diagnostics_start_timer(void);
```

---

### mbc_diagnostics_stop_timer

Stop timing and record.

```c
uint32_t mbc_diagnostics_stop_timer(void);
```

**Returns:** Elapsed time in microseconds

**Example:**
```c
mbc_diagnostics_start_timer();

// Perform Modbus transaction
mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &len);
send(sock, buffer, len, 0);
recv(sock, buffer, sizeof(buffer), 0);

uint32_t elapsed_us = mbc_diagnostics_stop_timer();
printf("Transaction took %u µs\n", elapsed_us);
```

---

### mbc_diagnostics_get_stats

Get performance statistics.

```c
typedef struct {
    uint32_t min_us;        // Minimum transaction time
    uint32_t max_us;        // Maximum transaction time
    uint32_t avg_us;        // Average transaction time
    uint32_t total_us;      // Total time spent
    uint32_t count;         // Number of transactions
} mbc_perf_stats_t;

void mbc_diagnostics_get_stats(mbc_perf_stats_t *stats);
```

**Example:**
```c
mbc_perf_stats_t stats;
mbc_diagnostics_get_stats(&stats);

printf("Performance Statistics:\n");
printf("  Transactions: %u\n", stats.count);
printf("  Min: %u µs\n", stats.min_us);
printf("  Max: %u µs\n", stats.max_us);
printf("  Avg: %u µs\n", stats.avg_us);
printf("  Total: %.2f ms\n", stats.total_us / 1000.0);
```

---

### mbc_diagnostics_reset_stats

Reset performance statistics.

```c
void mbc_diagnostics_reset_stats(void);
```

---

## Complete Example

```c
#include <modbuscore/diagnostics.h>
#include <stdio.h>

void my_trace(bool is_tx, const uint8_t *data, size_t len, void *ctx) {
    printf("[%s] ", is_tx ? "TX" : "RX");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

int main(void) {
    // Initialize diagnostics
    mbc_diag_config_t cfg = {
        .enable_frame_tracing = true,
        .enable_error_logging = true,
        .enable_performance_stats = true,
        .trace_callback = my_trace,
        .user_context = NULL
    };
    
    mbc_diagnostics_init(&cfg);
    mbc_diagnostics_reset_counters();
    
    // ... Perform Modbus operations ...
    
    // Print statistics
    printf("\n=== Diagnostics Summary ===\n");
    
    uint32_t tx = mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_TX_FRAMES);
    uint32_t rx = mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_RX_FRAMES);
    uint32_t err = mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_ERRORS);
    
    printf("Frames TX: %u\n", tx);
    printf("Frames RX: %u\n", rx);
    printf("Errors: %u\n", err);
    
    mbc_perf_stats_t stats;
    mbc_diagnostics_get_stats(&stats);
    
    printf("\nPerformance:\n");
    printf("  Transactions: %u\n", stats.count);
    printf("  Avg time: %u µs\n", stats.avg_us);
    printf("  Min time: %u µs\n", stats.min_us);
    printf("  Max time: %u µs\n", stats.max_us);
    
    return 0;
}
```

**Output:**
```
[TX] 00 01 00 00 00 06 01 03 00 00 00 0A
[RX] 00 01 00 00 00 17 01 03 14 00 64 00 C8 01 2C ...

=== Diagnostics Summary ===
Frames TX: 10
Frames RX: 10
Errors: 0

Performance:
  Transactions: 10
  Avg time: 1234 µs
  Min time: 1100 µs
  Max time: 1500 µs
```

## Thread Safety

Diagnostics functions use internal mutexes for thread safety. Safe to call from multiple threads.

## Overhead

- **Frame tracing:** ~10 µs per frame (default handler)
- **Counters:** ~0.1 µs per increment
- **Performance stats:** ~1 µs per measurement

**Recommendation:** Disable in production for maximum performance.

## Next Steps

- [Protocol API](protocol.md) - PDU/MBAP/RTU functions
- [Common Types](common.md) - Data structures
- [User Guide](../user-guide/README.md) - Usage patterns

---

**Prev**: [Protocol API ←](protocol.md) | **Home**: [API Reference →](README.md)
