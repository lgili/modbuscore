# Runtime Guide

Complete guide to using ModbusCore's runtime system for building robust Modbus applications.

## Table of Contents

- [Overview](#overview)
- [Runtime Architecture](#runtime-architecture)
- [Builder Pattern](#builder-pattern)
- [Dependency Injection](#dependency-injection)
- [Lifecycle Management](#lifecycle-management)
- [Best Practices](#best-practices)
- [Examples](#examples)

## Overview

The ModbusCore runtime provides a **unified interface** for managing all dependencies required by the protocol engine:

- **Transport layer** - Send/receive abstraction (TCP, RTU, mock)
- **Clock** - Timestamp generation for timeouts
- **Allocator** - Memory management (optional custom allocators)
- **Logger** - Text-based logging (legacy)
- **Diagnostics** - Structured event sink

**Key Benefits:**
- Dependency injection for testability
- Builder pattern for easy configuration
- Automatic safe defaults
- Zero overhead when features aren't used

---

## Runtime Architecture

### Layered Design

```
┌─────────────────────────────────────┐
│     Your Application Code           │
├─────────────────────────────────────┤
│     Runtime (Dependency Manager)    │
├─────────────────────────────────────┤
│  Transport │ Clock │ Allocator      │
│  Interface │ Iface │ Interface      │
└─────────────────────────────────────┘
         │         │         │
         ▼         ▼         ▼
    TCP/RTU    System    malloc/free
```

### Core Components

1. **Runtime Builder** - Fluent API for configuration
2. **Runtime Instance** - Manages dependency lifecycle
3. **Dependency Interfaces** - Pluggable implementations
4. **Default Providers** - Safe fallbacks for optional deps

---

## Builder Pattern

The builder pattern simplifies runtime configuration by allowing incremental setup with automatic defaults.

### Basic Usage

```c
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_tcp.h>

// 1. Initialize builder
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);

// 2. Configure transport (required)
mbc_transport_iface_t iface;
mbc_posix_tcp_ctx_t *ctx;
// ... create transport ...
mbc_runtime_builder_with_transport(&builder, &iface);

// 3. Build runtime
mbc_runtime_t runtime;
mbc_runtime_builder_build(&builder, &runtime);

// 4. Use runtime...

// 5. Cleanup
mbc_runtime_shutdown(&runtime);
mbc_posix_tcp_destroy(ctx);
```

### Fluent API (Method Chaining)

```c
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);

// Chain configuration calls
mbc_runtime_builder_with_transport(&builder, &iface)
    ->with_clock(&custom_clock)
    ->with_allocator(&pool_allocator)
    ->with_logger(&file_logger);

mbc_runtime_t runtime;
mbc_runtime_builder_build(&builder, &runtime);
```

### Configuration Steps

| Step | Required? | Default |
|------|-----------|---------|
| **Transport** | ✅ Yes | None - must provide |
| **Clock** | ❌ Optional | System clock |
| **Allocator** | ❌ Optional | malloc/free |
| **Logger** | ❌ Optional | No-op (silent) |
| **Diagnostics** | ❌ Optional | No-op (silent) |

---

## Dependency Injection

### Transport Injection

The transport layer is the only **required** dependency:

```c
// POSIX TCP
mbc_posix_tcp_config_t tcp_cfg = {
    .host = "192.168.1.100",
    .port = 502,
    .connect_timeout_ms = 5000,
    .recv_timeout_ms = 1000
};

mbc_transport_iface_t iface;
mbc_posix_tcp_ctx_t *ctx;
mbc_posix_tcp_create(&tcp_cfg, &iface, &ctx);

mbc_runtime_builder_with_transport(&builder, &iface);
```

**Supported Transports:**
- POSIX TCP (`posix_tcp.h`)
- Winsock TCP (`winsock_tcp.h`)
- POSIX RTU (`posix_rtu.h`)
- Windows RTU (`win32_rtu.h`)
- Mock (`mock.h` for testing)
- Custom implementations

---

### Clock Injection

Custom clock for precise timing control:

```c
typedef struct {
    uint64_t base_time_ms;
} my_clock_ctx_t;

uint64_t my_clock_now(void *ctx) {
    my_clock_ctx_t *c = (my_clock_ctx_t*)ctx;
    return c->base_time_ms + get_hardware_timer_ms();
}

my_clock_ctx_t clock_ctx = { .base_time_ms = 0 };

mbc_clock_iface_t clock = {
    .ctx = &clock_ctx,
    .now_ms = my_clock_now
};

mbc_runtime_builder_with_clock(&builder, &clock);
```

**Use Cases:**
- Hardware timers (embedded)
- Simulated time (testing)
- High-precision timing

---

### Allocator Injection

Custom memory management:

```c
// Pool allocator example
void *pool_alloc(void *ctx, size_t size) {
    memory_pool_t *pool = (memory_pool_t*)ctx;
    return pool_allocate(pool, size);
}

void pool_free(void *ctx, void *ptr) {
    memory_pool_t *pool = (memory_pool_t*)ctx;
    pool_release(pool, ptr);
}

memory_pool_t my_pool;
pool_init(&my_pool, 4096);

mbc_allocator_iface_t allocator = {
    .ctx = &my_pool,
    .alloc = pool_alloc,
    .free = pool_free
};

mbc_runtime_builder_with_allocator(&builder, &allocator);
```

**Use Cases:**
- Memory pools (deterministic allocation)
- Tracking allocators (leak detection)
- Zero-allocation environments

---

### Logger Injection

Text-based logging:

```c
void file_logger(void *ctx, const char *category, const char *message) {
    FILE *log = (FILE*)ctx;
    fprintf(log, "[%s] %s\n", category, message);
    fflush(log);
}

FILE *logfile = fopen("modbus.log", "a");

mbc_logger_iface_t logger = {
    .ctx = logfile,
    .write = file_logger
};

mbc_runtime_builder_with_logger(&builder, &logger);

// Later...
fclose(logfile);
```

**Use Cases:**
- File logging
- Syslog integration
- Custom log formatters

---

### Diagnostics Injection

Structured event logging:

```c
void diag_handler(void *ctx, const mbc_diag_event_t *event) {
    printf("Event: %s, Status: %d\n", event->name, event->status);
}

mbc_diag_sink_iface_t diag = {
    .ctx = NULL,
    .emit = diag_handler
};

mbc_runtime_builder_with_diag(&builder, &diag);
```

**Use Cases:**
- Performance monitoring
- Real-time analytics
- Telemetry systems

---

## Lifecycle Management

### Initialization

```c
mbc_runtime_t runtime;

mbc_status_t status = mbc_runtime_builder_build(&builder, &runtime);

if (status != MBC_STATUS_OK) {
    fprintf(stderr, "Runtime initialization failed: %d\n", status);
    return -1;
}

printf("Runtime initialized successfully\n");
```

### Runtime State

```c
// Check if runtime is ready
if (mbc_runtime_is_ready(&runtime)) {
    // Safe to use
}

// Get dependencies
const mbc_runtime_config_t *deps = mbc_runtime_dependencies(&runtime);
if (deps) {
    // Access transport, clock, etc.
    uint64_t now = deps->clock.now_ms(deps->clock.ctx);
}
```

### Shutdown

```c
// Shutdown runtime (releases resources)
mbc_runtime_shutdown(&runtime);

// Destroy transport
mbc_posix_tcp_destroy(tcp_ctx);
```

**Important:** Always shutdown runtime before destroying transport contexts.

---

## Best Practices

### 1. Always Check Return Values

```c
// ❌ Bad: Ignoring errors
mbc_runtime_builder_build(&builder, &runtime);

// ✅ Good: Checking errors
if (mbc_runtime_builder_build(&builder, &runtime) != MBC_STATUS_OK) {
    fprintf(stderr, "Failed to build runtime\n");
    return -1;
}
```

### 2. Use Builder Pattern

```c
// ✅ Recommended: Builder with defaults
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);
mbc_runtime_builder_with_transport(&builder, &iface);
mbc_runtime_builder_build(&builder, &runtime);

// ❌ Avoid: Manual construction (easy to forget dependencies)
mbc_runtime_config_t cfg = { /* manual config */ };
mbc_runtime_init(&runtime, &cfg);
```

### 3. Manage Transport Lifecycle

```c
// Transport creation
mbc_posix_tcp_ctx_t *ctx;
mbc_transport_iface_t iface;
mbc_posix_tcp_create(&cfg, &iface, &ctx);

// Build runtime
mbc_runtime_t runtime;
// ... use builder ...

// Cleanup (correct order)
mbc_runtime_shutdown(&runtime);
mbc_posix_tcp_destroy(ctx);  // After runtime shutdown
```

### 4. Separate Instances for Threads

```c
// ✅ Thread-safe: Separate runtime per thread
void *worker_thread(void *arg) {
    mbc_runtime_t runtime;
    // Initialize runtime for this thread...
    
    // Use runtime...
    
    mbc_runtime_shutdown(&runtime);
    return NULL;
}
```

### 5. Test with Mock Transport

```c
#ifdef TESTING
// Use mock transport for unit tests
mbc_mock_transport_ctx_t *mock_ctx;
mbc_transport_iface_t mock_iface;
mbc_mock_transport_create(&mock_iface, &mock_ctx);

// Configure expected responses
mbc_mock_transport_add_response(mock_ctx, response, response_len);

mbc_runtime_builder_with_transport(&builder, &mock_iface);
#endif
```

---

## Examples

### Minimal TCP Client

```c
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_tcp.h>
#include <stdio.h>

int main(void) {
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
        fprintf(stderr, "Connection failed\n");
        return 1;
    }
    
    // Build runtime (uses defaults for clock, allocator, etc.)
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    if (mbc_runtime_builder_build(&builder, &runtime) != MBC_STATUS_OK) {
        fprintf(stderr, "Runtime build failed\n");
        mbc_posix_tcp_destroy(ctx);
        return 1;
    }
    
    printf("Runtime ready!\n");
    
    // Use runtime for Modbus operations...
    
    // Cleanup
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(ctx);
    
    return 0;
}
```

### Full Custom Configuration

```c
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_tcp.h>
#include <stdio.h>
#include <time.h>

// Custom clock (monotonic)
uint64_t monotonic_clock_now(void *ctx) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Custom logger
void syslog_logger(void *ctx, const char *category, const char *message) {
    // syslog(LOG_INFO, "[%s] %s", category, message);
    printf("[%s] %s\n", category, message);
}

int main(void) {
    // Create transport
    mbc_posix_tcp_config_t tcp_cfg = {
        .host = "10.0.0.50",
        .port = 502,
        .connect_timeout_ms = 3000,
        .recv_timeout_ms = 1000
    };
    
    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t *ctx;
    mbc_posix_tcp_create(&tcp_cfg, &iface, &ctx);
    
    // Custom clock
    mbc_clock_iface_t clock = {
        .ctx = NULL,
        .now_ms = monotonic_clock_now
    };
    
    // Custom logger
    mbc_logger_iface_t logger = {
        .ctx = NULL,
        .write = syslog_logger
    };
    
    // Build runtime with custom dependencies
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    
    mbc_runtime_builder_with_transport(&builder, &iface);
    mbc_runtime_builder_with_clock(&builder, &clock);
    mbc_runtime_builder_with_logger(&builder, &logger);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    printf("Runtime configured with custom dependencies\n");
    
    // Use runtime...
    
    // Cleanup
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(ctx);
    
    return 0;
}
```

### RTU Serial Client

```c
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_rtu.h>
#include <stdio.h>

int main(void) {
    // Create RTU transport
    mbc_posix_rtu_config_t rtu_cfg = {
        .device_path = "/dev/ttyUSB0",
        .baud_rate = 19200,
        .data_bits = 8,
        .parity = 'N',
        .stop_bits = 1,
        .guard_time_us = 0,  // Auto-calculate
        .rx_buffer_capacity = 256
    };
    
    mbc_transport_iface_t iface;
    mbc_posix_rtu_ctx_t *ctx;
    
    if (mbc_posix_rtu_create(&rtu_cfg, &iface, &ctx) != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to open serial port\n");
        return 1;
    }
    
    printf("Opened %s at %u baud\n", rtu_cfg.device_path, rtu_cfg.baud_rate);
    
    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    // Use runtime for RTU operations...
    
    // Cleanup
    mbc_runtime_shutdown(&runtime);
    mbc_posix_rtu_destroy(ctx);
    
    return 0;
}
```

---

## Performance Considerations

### Memory Usage

- **Builder:** Stack-allocated (~200 bytes)
- **Runtime:** Stack-allocated (~300 bytes)
- **No heap allocations** (unless custom allocator configured)

### CPU Overhead

- **Builder initialization:** ~100 ns
- **Runtime build:** ~1 µs (one-time cost)
- **Dependency lookup:** Inline (zero overhead)

### Threading

Runtime is **not thread-safe**. Options:

1. **Separate instances per thread** (recommended)
2. **Mutex protection** (shared instance)
3. **Single-threaded** (simplest)

---

## Next Steps

- [Auto-Heal Guide](auto-heal.md) - Automatic error recovery
- [Testing Guide](testing.md) - Testing strategies with mock transports
- [Transport Selection](transports.md) - Choosing TCP vs RTU
- [Runtime API Reference](../api/runtime.md) - Complete API docs

---

**Prev**: [Protocol Engine ←](protocol-engine.md) | **Next**: [Auto-Heal →](auto-heal.md)
