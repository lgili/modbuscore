# Auto-Heal Guide

Complete guide to ModbusCore's automatic error recovery system.

## Table of Contents

- [Overview](#overview)
- [How It Works](#how-it-works)
- [Configuration](#configuration)
- [Retry Strategies](#retry-strategies)
- [Circuit Breaker](#circuit-breaker)
- [Event Monitoring](#event-monitoring)
- [Examples](#examples)
- [Best Practices](#best-practices)

## Overview

The **Auto-Heal Supervisor** provides automatic error recovery for transient failures in Modbus communications:

- **Automatic retries** for timeouts and I/O errors
- **Exponential backoff** to avoid overwhelming busy servers
- **Circuit breaker** to prevent endless retry loops
- **Telemetry** for monitoring recovery operations

**Key Features:**
- Transparent retry logic (no manual intervention)
- Configurable backoff strategies
- Circuit breaker for fault isolation
- Event callbacks for observability

**Header:** `<modbuscore/runtime/autoheal.h>`

---

## How It Works

### Supervision Cycle

```
┌─────────────────────────────────────┐
│  1. Submit Request                  │
│     (frame stored internally)       │
└───────────┬─────────────────────────┘
            │
            ▼
┌─────────────────────────────────────┐
│  2. Send via Engine                 │
│     (mbc_engine_submit)             │
└───────────┬─────────────────────────┘
            │
            ▼
    ┌───────────────┐
    │  Success?     │
    └───────┬───────┘
            │
    ┌───────┴────────┐
    │ Yes            │ No
    ▼                ▼
┌────────┐    ┌──────────────┐
│ Return │    │ Retry?       │
│  PDU   │    │ (< max)      │
└────────┘    └──────┬───────┘
                     │
              ┌──────┴─────┐
              │ Yes        │ No
              ▼            ▼
        ┌──────────┐  ┌─────────┐
        │ Backoff  │  │ Circuit │
        │ & Retry  │  │ Breaker │
        └──────────┘  └─────────┘
```

### States

| State | Description |
|-------|-------------|
| **IDLE** | No pending request |
| **WAITING** | Request submitted, waiting for response |
| **SCHEDULED** | Retry scheduled (backoff in progress) |
| **CIRCUIT_OPEN** | Circuit breaker open (cooldown active) |

---

## Configuration

### mbc_autoheal_config_t

```c
typedef struct mbc_autoheal_config {
    const mbc_runtime_t *runtime;      // Associated runtime
    uint32_t max_retries;              // Max retries before circuit opens
    uint32_t initial_backoff_ms;       // Initial backoff (0 = immediate)
    uint32_t max_backoff_ms;           // Upper limit for backoff
    uint32_t cooldown_ms;              // Circuit breaker cooldown
    size_t request_capacity;           // Max frame size to store
    mbc_autoheal_observer_fn observer; // Event callback (optional)
    void *observer_ctx;                // Context for observer
} mbc_autoheal_config_t;
```

### Configuration Parameters

| Parameter | Description | Typical Value |
|-----------|-------------|---------------|
| `runtime` | Runtime instance | Required |
| `max_retries` | Maximum retry attempts | 3-5 |
| `initial_backoff_ms` | Starting backoff delay | 100-1000 ms |
| `max_backoff_ms` | Maximum backoff delay | 5000-30000 ms |
| `cooldown_ms` | Circuit breaker cooldown | 10000-60000 ms |
| `request_capacity` | Frame buffer size | 260 bytes |
| `observer` | Event callback | NULL or custom |

---

## Retry Strategies

### Exponential Backoff

The supervisor implements exponential backoff with capping:

```
Retry 1: initial_backoff_ms
Retry 2: initial_backoff_ms * 2
Retry 3: initial_backoff_ms * 4
...
Retry N: min(initial_backoff_ms * 2^N, max_backoff_ms)
```

**Example:**
```c
// Initial: 100ms, Max: 5000ms
// Retry 1: 100ms
// Retry 2: 200ms
// Retry 3: 400ms
// Retry 4: 800ms
// Retry 5: 1600ms
// Retry 6: 3200ms
// Retry 7: 5000ms (capped)
// Retry 8: 5000ms (capped)
```

### Immediate Retry

Set `initial_backoff_ms = 0` for immediate retries:

```c
mbc_autoheal_config_t cfg = {
    .runtime = &runtime,
    .max_retries = 3,
    .initial_backoff_ms = 0,  // No delay
    .max_backoff_ms = 0,
    .cooldown_ms = 10000,
    .request_capacity = 260
};
```

### Linear Backoff

For linear backoff, keep `initial_backoff_ms == max_backoff_ms`:

```c
mbc_autoheal_config_t cfg = {
    .initial_backoff_ms = 1000,  // Always 1 second
    .max_backoff_ms = 1000
};
```

---

## Circuit Breaker

### Purpose

The circuit breaker **prevents endless retry loops** when a device is persistently unavailable.

### Operation

1. **Closed (Normal)** - Requests are submitted normally
2. **Open (Fault)** - After `max_retries` failures, circuit opens
3. **Cooldown** - Wait `cooldown_ms` before allowing new requests
4. **Closed (Recovered)** - After cooldown, circuit closes

### States

```
┌─────────────┐
│   CLOSED    │──────────────┐
│  (Normal)   │              │
└──────┬──────┘              │
       │                     │
       │ max_retries         │ cooldown_ms
       │ exceeded            │ elapsed
       │                     │
       ▼                     │
┌─────────────┐              │
│    OPEN     │──────────────┘
│  (Faulted)  │
└─────────────┘
```

### Checking Circuit State

```c
if (mbc_autoheal_is_circuit_open(&supervisor)) {
    printf("Circuit breaker is open - device unavailable\n");
    
    // Wait for cooldown or take alternative action
}
```

---

## Event Monitoring

### Event Types

```c
typedef enum {
    MBC_AUTOHEAL_EVENT_ATTEMPT,         // Send attempt initiated
    MBC_AUTOHEAL_EVENT_RETRY_SCHEDULED, // Retry scheduled (backoff)
    MBC_AUTOHEAL_EVENT_RESPONSE_OK,     // Successful response
    MBC_AUTOHEAL_EVENT_GIVE_UP,         // All retries exhausted
    MBC_AUTOHEAL_EVENT_CIRCUIT_OPEN,    // Circuit breaker opened
    MBC_AUTOHEAL_EVENT_CIRCUIT_CLOSED   // Circuit breaker closed
} mbc_autoheal_event_t;
```

### Observer Callback

```c
void autoheal_observer(void *ctx, mbc_autoheal_event_t event) {
    const char *event_names[] = {
        "ATTEMPT",
        "RETRY_SCHEDULED",
        "RESPONSE_OK",
        "GIVE_UP",
        "CIRCUIT_OPEN",
        "CIRCUIT_CLOSED"
    };
    
    printf("Auto-heal event: %s\n", event_names[event]);
    
    switch (event) {
        case MBC_AUTOHEAL_EVENT_GIVE_UP:
            // Log critical error
            break;
        case MBC_AUTOHEAL_EVENT_CIRCUIT_OPEN:
            // Alert monitoring system
            break;
        case MBC_AUTOHEAL_EVENT_RESPONSE_OK:
            // Update success metrics
            break;
    }
}

// Configure observer
mbc_autoheal_config_t cfg = {
    // ...
    .observer = autoheal_observer,
    .observer_ctx = NULL
};
```

---

## Examples

### Basic Auto-Heal Setup

```c
#include <modbuscore/runtime/autoheal.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_tcp.h>
#include <stdio.h>

int main(void) {
    // Create transport
    mbc_posix_tcp_config_t tcp_cfg = {
        .host = "192.168.1.100",
        .port = 502,
        .connect_timeout_ms = 5000,
        .recv_timeout_ms = 1000
    };
    
    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t *ctx;
    mbc_posix_tcp_create(&tcp_cfg, &iface, &ctx);
    
    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    // Create protocol engine
    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .transport = &iface,
        .mode = MBC_ENGINE_CLIENT,
        .timeout_ms = 3000
    };
    mbc_engine_init(&engine, &engine_cfg);
    
    // Configure auto-heal
    mbc_autoheal_config_t heal_cfg = {
        .runtime = &runtime,
        .max_retries = 3,
        .initial_backoff_ms = 100,
        .max_backoff_ms = 5000,
        .cooldown_ms = 30000,
        .request_capacity = 260,
        .observer = NULL
    };
    
    mbc_autoheal_supervisor_t supervisor;
    mbc_autoheal_init(&supervisor, &heal_cfg, &engine);
    
    // Build and submit request
    mbc_pdu_t pdu;
    mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    mbc_mbap_header_t hdr = { .transaction_id = 1, .unit_id = 1 };
    uint8_t buffer[260];
    size_t length;
    mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &length);
    
    // Submit with auto-heal
    if (mbc_autoheal_submit(&supervisor, buffer, length) == MBC_STATUS_OK) {
        printf("Request submitted\n");
        
        // Poll until response or failure
        mbc_pdu_t response;
        while (!mbc_autoheal_take_pdu(&supervisor, &response)) {
            mbc_autoheal_step(&supervisor, 512);
            
            if (mbc_autoheal_is_circuit_open(&supervisor)) {
                printf("Circuit breaker open - giving up\n");
                break;
            }
        }
        
        if (response.function == 0x03) {
            printf("Success! Received %u registers\n", response.data_length / 2);
        }
    }
    
    // Cleanup
    mbc_autoheal_shutdown(&supervisor);
    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(ctx);
    
    return 0;
}
```

### With Event Monitoring

```c
typedef struct {
    uint32_t attempt_count;
    uint32_t retry_count;
    uint32_t success_count;
    uint32_t failure_count;
} autoheal_stats_t;

void stats_observer(void *ctx, mbc_autoheal_event_t event) {
    autoheal_stats_t *stats = (autoheal_stats_t*)ctx;
    
    switch (event) {
        case MBC_AUTOHEAL_EVENT_ATTEMPT:
            stats->attempt_count++;
            printf("Attempt #%u\n", stats->attempt_count);
            break;
            
        case MBC_AUTOHEAL_EVENT_RETRY_SCHEDULED:
            stats->retry_count++;
            printf("Retry #%u scheduled\n", stats->retry_count);
            break;
            
        case MBC_AUTOHEAL_EVENT_RESPONSE_OK:
            stats->success_count++;
            printf("Success (after %u retries)\n", stats->retry_count);
            break;
            
        case MBC_AUTOHEAL_EVENT_GIVE_UP:
            stats->failure_count++;
            fprintf(stderr, "Failed after %u attempts\n", stats->attempt_count);
            break;
            
        case MBC_AUTOHEAL_EVENT_CIRCUIT_OPEN:
            fprintf(stderr, "⚠️  Circuit breaker OPEN\n");
            break;
            
        case MBC_AUTOHEAL_EVENT_CIRCUIT_CLOSED:
            printf("✓ Circuit breaker CLOSED\n");
            break;
    }
}

// Usage
autoheal_stats_t stats = {0};

mbc_autoheal_config_t cfg = {
    .runtime = &runtime,
    .max_retries = 5,
    .initial_backoff_ms = 200,
    .max_backoff_ms = 10000,
    .cooldown_ms = 60000,
    .request_capacity = 260,
    .observer = stats_observer,
    .observer_ctx = &stats
};

// ... use auto-heal ...

printf("\n=== Auto-Heal Statistics ===\n");
printf("Total attempts: %u\n", stats.attempt_count);
printf("Retries: %u\n", stats.retry_count);
printf("Successes: %u\n", stats.success_count);
printf("Failures: %u\n", stats.failure_count);
```

### Aggressive Retry (Quick Recovery)

```c
// Fast retry for low-latency networks
mbc_autoheal_config_t aggressive_cfg = {
    .runtime = &runtime,
    .max_retries = 5,
    .initial_backoff_ms = 10,   // 10ms
    .max_backoff_ms = 100,      // Cap at 100ms
    .cooldown_ms = 5000,        // 5 second cooldown
    .request_capacity = 260
};
```

### Conservative Retry (Busy Networks)

```c
// Slow retry for busy/congested networks
mbc_autoheal_config_t conservative_cfg = {
    .runtime = &runtime,
    .max_retries = 3,
    .initial_backoff_ms = 1000,  // 1 second
    .max_backoff_ms = 30000,     // Cap at 30 seconds
    .cooldown_ms = 300000,       // 5 minute cooldown
    .request_capacity = 260
};
```

---

## Best Practices

### 1. Choose Appropriate Retry Limits

```c
// ✅ Good: Limited retries
.max_retries = 3  // Fail fast

// ❌ Bad: Too many retries
.max_retries = 100  // Can delay failures
```

### 2. Set Realistic Backoff

```c
// ✅ Good: Reasonable backoff
.initial_backoff_ms = 100,
.max_backoff_ms = 5000

// ❌ Bad: Too aggressive
.initial_backoff_ms = 1,
.max_backoff_ms = 10  // Hammers server
```

### 3. Monitor Circuit Breaker

```c
// Check periodically
if (mbc_autoheal_is_circuit_open(&supervisor)) {
    // Log alert
    // Notify monitoring system
    // Attempt alternative path
}
```

### 4. Use Observer for Telemetry

```c
// Track metrics
void telemetry_observer(void *ctx, mbc_autoheal_event_t event) {
    metrics_system_t *metrics = (metrics_system_t*)ctx;
    
    switch (event) {
        case MBC_AUTOHEAL_EVENT_RESPONSE_OK:
            metrics_increment(metrics, "modbus.autoheal.success");
            break;
        case MBC_AUTOHEAL_EVENT_GIVE_UP:
            metrics_increment(metrics, "modbus.autoheal.failure");
            break;
    }
}
```

### 5. Reset on Shutdown

```c
// Clear pending requests before shutdown
mbc_autoheal_reset(&supervisor);
mbc_autoheal_shutdown(&supervisor);
```

---

## Tuning Guidelines

### Network Type vs Configuration

| Network Type | max_retries | initial_backoff_ms | max_backoff_ms | cooldown_ms |
|--------------|-------------|---------------------|----------------|-------------|
| **LAN (fast)** | 3-5 | 50-100 | 1000-5000 | 10000-30000 |
| **WAN (slow)** | 3 | 500-1000 | 10000-30000 | 60000-300000 |
| **Cellular** | 5-7 | 1000-2000 | 30000-60000 | 300000-600000 |
| **Serial RTU** | 3-5 | 100-500 | 5000-10000 | 30000-60000 |

### Device Characteristics

| Device Type | Recommended Strategy |
|-------------|----------------------|
| **PLC (reliable)** | Aggressive (few retries, short backoff) |
| **Sensor (unreliable)** | Conservative (more retries, longer backoff) |
| **Gateway (busy)** | Exponential backoff with high max |
| **Embedded (slow)** | Long backoff, patient retries |

---

## Troubleshooting

### Infinite Retry Loop

**Symptom:** Never gives up despite failures

**Cause:** Circuit breaker not configured

**Solution:**
```c
// Ensure max_retries is set
.max_retries = 3,  // Not 0
.cooldown_ms = 30000
```

### Excessive Network Traffic

**Symptom:** Too many retries flooding network

**Cause:** Backoff too small

**Solution:**
```c
// Increase backoff
.initial_backoff_ms = 500,  // Not 10
.max_backoff_ms = 10000
```

### Circuit Breaker Always Open

**Symptom:** Circuit never closes

**Cause:** Cooldown too long or device still offline

**Solution:**
```c
// Reduce cooldown for testing
.cooldown_ms = 5000,  // Not 300000

// Check device availability
if (mbc_posix_tcp_is_connected(tcp_ctx)) {
    // Device online
}
```

---

## Next Steps

- [Testing Guide](testing.md) - Testing with auto-heal
- [Runtime Guide](runtime.md) - Runtime configuration
- [Diagnostics](../api/diagnostics.md) - Monitoring and metrics

---

**Prev**: [Runtime ←](runtime.md) | **Next**: [Testing →](testing.md)
