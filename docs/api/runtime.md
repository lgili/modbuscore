# Runtime API

Complete reference for ModbusCore runtime builder and dependency management.

## Table of Contents

- [Overview](#overview)
- [Runtime Builder](#runtime-builder)
- [Runtime Management](#runtime-management)
- [Dependency Interfaces](#dependency-interfaces)
- [Default Dependencies](#default-dependencies)
- [Complete Examples](#complete-examples)

## Overview

The ModbusCore runtime manages all core dependencies required by the protocol engine:

- **Transport** - Send/receive interface (TCP, RTU, mock)
- **Clock** - Timestamp generation for timeouts
- **Allocator** - Memory management
- **Logger** - Text-based logging (legacy)
- **Diagnostics** - Structured event sink

**Key Features:**
- Builder pattern for easy configuration
- Automatic safe defaults for optional dependencies
- Dependency injection for testability
- Zero overhead when dependencies aren't used

**Headers:**
- `<modbuscore/runtime/builder.h>` - Runtime builder
- `<modbuscore/runtime/runtime.h>` - Runtime management
- `<modbuscore/runtime/dependencies.h>` - Dependency interfaces

---

## Runtime Builder

The builder pattern simplifies runtime configuration by providing:

1. Fluent API for setting dependencies
2. Automatic defaults for optional dependencies
3. Validation before runtime initialization

### mbc_runtime_builder_t

Builder state structure:

```c
typedef struct mbc_runtime_builder {
    mbc_runtime_config_t config;  // Configuration being built
    bool transport_set;           // Transport configured?
    bool clock_set;               // Clock configured?
    bool allocator_set;           // Allocator configured?
    bool logger_set;              // Logger configured?
    bool diag_set;                // Diagnostics configured?
} mbc_runtime_builder_t;
```

---

### mbc_runtime_builder_init

Initialize a runtime builder.

```c
void mbc_runtime_builder_init(mbc_runtime_builder_t *builder);
```

**Example:**
```c
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);
```

**Note:** Always initialize builder before use to ensure clean state.

---

### mbc_runtime_builder_with_transport

Set transport layer (required).

```c
mbc_runtime_builder_t *mbc_runtime_builder_with_transport(
    mbc_runtime_builder_t *builder,
    const mbc_transport_iface_t *transport
);
```

**Parameters:**
- `builder` - Builder instance
- `transport` - Transport interface (TCP, RTU, mock)

**Returns:** Builder pointer (for chaining)

**Example:**
```c
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

---

### mbc_runtime_builder_with_clock

Set clock interface (optional).

```c
mbc_runtime_builder_t *mbc_runtime_builder_with_clock(
    mbc_runtime_builder_t *builder,
    const mbc_clock_iface_t *clock
);
```

**Default:** System clock (POSIX: `clock_gettime`, Windows: `GetTickCount64`)

**Example:**
```c
uint64_t my_clock_now(void *ctx) {
    return my_custom_timer_ms();
}

mbc_clock_iface_t clock = {
    .ctx = NULL,
    .now_ms = my_clock_now
};

mbc_runtime_builder_with_clock(&builder, &clock);
```

---

### mbc_runtime_builder_with_allocator

Set allocator interface (optional).

```c
mbc_runtime_builder_t *mbc_runtime_builder_with_allocator(
    mbc_runtime_builder_t *builder,
    const mbc_allocator_iface_t *allocator
);
```

**Default:** Standard library (`malloc`/`free`)

**Example:**
```c
void *my_alloc(void *ctx, size_t size) {
    return custom_malloc(size);
}

void my_free(void *ctx, void *ptr) {
    custom_free(ptr);
}

mbc_allocator_iface_t allocator = {
    .ctx = NULL,
    .alloc = my_alloc,
    .free = my_free
};

mbc_runtime_builder_with_allocator(&builder, &allocator);
```

---

### mbc_runtime_builder_with_logger

Set logger interface (optional).

```c
mbc_runtime_builder_t *mbc_runtime_builder_with_logger(
    mbc_runtime_builder_t *builder,
    const mbc_logger_iface_t *logger
);
```

**Default:** No-op (silent)

**Example:**
```c
void my_log(void *ctx, const char *category, const char *message) {
    printf("[%s] %s\n", category, message);
}

mbc_logger_iface_t logger = {
    .ctx = NULL,
    .write = my_log
};

mbc_runtime_builder_with_logger(&builder, &logger);
```

---

### mbc_runtime_builder_with_diag

Set structured diagnostics sink (optional).

```c
mbc_runtime_builder_t *mbc_runtime_builder_with_diag(
    mbc_runtime_builder_t *builder,
    const mbc_diag_sink_iface_t *diag
);
```

**Default:** No-op

**Example:**
```c
void my_diag_event(void *ctx, const mbc_diag_event_t *event) {
    printf("Event: %s\n", event->name);
}

mbc_diag_sink_iface_t diag = {
    .ctx = NULL,
    .emit = my_diag_event
};

mbc_runtime_builder_with_diag(&builder, &diag);
```

---

### mbc_runtime_builder_build

Build runtime from builder configuration.

```c
mbc_status_t mbc_runtime_builder_build(
    mbc_runtime_builder_t *builder,
    mbc_runtime_t *runtime
);
```

**Parameters:**
- `builder` - Configured builder
- `runtime` - Runtime structure to initialize

**Returns:**
- `MBC_STATUS_OK` - Runtime initialized successfully
- `MBC_STATUS_INVALID_PARAMETER` - Transport not set
- `MBC_STATUS_ERROR` - Initialization failed

**Example:**
```c
mbc_runtime_t runtime;

mbc_status_t status = mbc_runtime_builder_build(&builder, &runtime);

if (status == MBC_STATUS_OK) {
    printf("Runtime initialized!\n");
    
    // Use runtime...
    
    mbc_runtime_shutdown(&runtime);
}
```

**Note:** Any dependencies not explicitly set will be filled with safe defaults.

---

## Runtime Management

### mbc_runtime_t

Runtime state structure:

```c
typedef struct mbc_runtime {
    bool initialised;           // Initialization flag
    mbc_runtime_config_t deps;  // Dependency configuration
} mbc_runtime_t;
```

**Note:** Treat as opaque. Use API functions to access.

---

### mbc_runtime_init

Initialize runtime directly (advanced).

```c
mbc_status_t mbc_runtime_init(
    mbc_runtime_t *runtime,
    const mbc_runtime_config_t *config
);
```

**Recommendation:** Use builder instead for automatic defaults.

**Example:**
```c
mbc_runtime_config_t config = {
    .transport = iface,
    .clock = clock,
    .allocator = allocator,
    .logger = logger,
    .diag = diag
};

mbc_runtime_t runtime;
mbc_runtime_init(&runtime, &config);
```

---

### mbc_runtime_shutdown

Shutdown runtime and release resources.

```c
void mbc_runtime_shutdown(mbc_runtime_t *runtime);
```

**Example:**
```c
mbc_runtime_shutdown(&runtime);
```

**Note:** Safe to call multiple times.

---

### mbc_runtime_is_ready

Check if runtime is initialized.

```c
bool mbc_runtime_is_ready(const mbc_runtime_t *runtime);
```

**Example:**
```c
if (mbc_runtime_is_ready(&runtime)) {
    // Perform operations...
}
```

---

### mbc_runtime_dependencies

Get runtime dependency configuration.

```c
const mbc_runtime_config_t *mbc_runtime_dependencies(const mbc_runtime_t *runtime);
```

**Returns:** Pointer to dependency config, or NULL if not initialized

**Example:**
```c
const mbc_runtime_config_t *deps = mbc_runtime_dependencies(&runtime);

if (deps) {
    // Access dependencies...
    uint64_t now = deps->clock.now_ms(deps->clock.ctx);
}
```

---

## Dependency Interfaces

### Transport Interface

```c
typedef struct mbc_transport_iface {
    void *ctx;
    mbc_status_t (*send)(void *ctx, const uint8_t *buffer, size_t length,
                         mbc_transport_io_t *out);
    mbc_status_t (*receive)(void *ctx, uint8_t *buffer, size_t capacity,
                            mbc_transport_io_t *out);
    uint64_t (*now)(void *ctx);
    void (*yield)(void *ctx);
} mbc_transport_iface_t;
```

See [Transport API](transport.md) for details.

---

### Clock Interface

```c
typedef struct mbc_clock_iface {
    void *ctx;
    uint64_t (*now_ms)(void *ctx);  // Current time in milliseconds
} mbc_clock_iface_t;
```

**Purpose:** Provide timestamps for timeout detection and performance measurement.

---

### Allocator Interface

```c
typedef struct mbc_allocator_iface {
    void *ctx;
    void *(*alloc)(void *ctx, size_t size);
    void (*free)(void *ctx, void *ptr);
} mbc_allocator_iface_t;
```

**Purpose:** Custom memory management (e.g., pool allocators, tracking).

---

### Logger Interface

```c
typedef struct mbc_logger_iface {
    void *ctx;
    void (*write)(void *ctx, const char *category, const char *message);
} mbc_logger_iface_t;
```

**Purpose:** Legacy text logging.

---

### Diagnostics Sink Interface

```c
typedef struct mbc_diag_sink_iface {
    void *ctx;
    void (*emit)(void *ctx, const mbc_diag_event_t *event);
} mbc_diag_sink_iface_t;
```

**Purpose:** Structured event logging for advanced diagnostics.

---

## Default Dependencies

When using the builder, missing dependencies are filled with defaults:

| Dependency | Default Behavior |
|------------|------------------|
| **Transport** | **None** - MUST be set explicitly |
| **Clock** | POSIX: `clock_gettime(CLOCK_MONOTONIC)` <br> Windows: `GetTickCount64()` |
| **Allocator** | Standard `malloc()`/`free()` |
| **Logger** | No-op (silent) |
| **Diagnostics** | No-op (silent) |

**Example with defaults:**
```c
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);

// Only transport is required
mbc_runtime_builder_with_transport(&builder, &iface);

mbc_runtime_t runtime;
mbc_runtime_builder_build(&builder, &runtime);  // Uses default clock, allocator, etc.
```

---

## Complete Examples

### Minimal Setup (TCP Client)

```c
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_tcp.h>
#include <stdio.h>

int main(void) {
    // Configure TCP transport
    mbc_posix_tcp_config_t tcp_cfg = {
        .host = "192.168.1.100",
        .port = 502,
        .connect_timeout_ms = 5000,
        .recv_timeout_ms = 1000
    };
    
    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t *ctx;
    
    if (mbc_posix_tcp_create(&tcp_cfg, &iface, &ctx) != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }
    
    // Build runtime with defaults
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    if (mbc_runtime_builder_build(&builder, &runtime) != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to build runtime\n");
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

---

### Custom Dependencies (Full Configuration)

```c
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_tcp.h>
#include <stdio.h>
#include <stdlib.h>

// Custom clock
uint64_t custom_clock_now(void *ctx) {
    return my_timer_get_ms();
}

// Custom allocator with tracking
void *tracked_alloc(void *ctx, size_t size) {
    printf("Allocating %zu bytes\n", size);
    return malloc(size);
}

void tracked_free(void *ctx, void *ptr) {
    printf("Freeing memory\n");
    free(ptr);
}

// Custom logger
void custom_log(void *ctx, const char *category, const char *message) {
    FILE *log = (FILE*)ctx;
    fprintf(log, "[%s] %s\n", category, message);
    fflush(log);
}

int main(void) {
    // Open log file
    FILE *logfile = fopen("modbus.log", "a");
    
    // Create TCP transport
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
        .now_ms = custom_clock_now
    };
    
    // Custom allocator
    mbc_allocator_iface_t allocator = {
        .ctx = NULL,
        .alloc = tracked_alloc,
        .free = tracked_free
    };
    
    // Custom logger
    mbc_logger_iface_t logger = {
        .ctx = logfile,
        .write = custom_log
    };
    
    // Build runtime with all custom dependencies
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    
    mbc_runtime_builder_with_transport(&builder, &iface);
    mbc_runtime_builder_with_clock(&builder, &clock);
    mbc_runtime_builder_with_allocator(&builder, &allocator);
    mbc_runtime_builder_with_logger(&builder, &logger);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    printf("Runtime configured with custom dependencies\n");
    
    // Use runtime...
    
    // Cleanup
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(ctx);
    fclose(logfile);
    
    return 0;
}
```

---

### Fluent Builder Pattern

```c
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);

// Chain builder calls
mbc_runtime_builder_with_transport(&builder, &iface)
    ->with_clock(&clock)
    ->with_allocator(&allocator)
    ->with_logger(&logger);

mbc_runtime_t runtime;
mbc_runtime_builder_build(&builder, &runtime);
```

---

## Thread Safety

The runtime itself is **not thread-safe**. If sharing a runtime across threads:

1. Protect with mutexes
2. Use separate runtime instances per thread
3. Ensure transport is thread-safe

**Example:**
```c
pthread_mutex_t runtime_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_lock(&runtime_lock);
// Use runtime...
pthread_mutex_unlock(&runtime_lock);
```

---

## Memory Management

- **Builder:** Stack-allocated, no cleanup needed
- **Runtime:** Stack-allocated, call `mbc_runtime_shutdown()` to release
- **Transport context:** Heap-allocated, call destroy function

**Typical cleanup:**
```c
mbc_runtime_shutdown(&runtime);       // Shutdown runtime
mbc_posix_tcp_destroy(tcp_ctx);       // Destroy transport
```

---

## Performance

- **Builder overhead:** ~100 ns (one-time setup cost)
- **Runtime initialization:** ~1 µs
- **Dependency lookup:** Inline (zero overhead)

---

## Next Steps

- [Transport API](transport.md) - Transport layer details
- [Diagnostics API](diagnostics.md) - Logging and tracing
- [User Guide: Runtime](../user-guide/runtime.md) - High-level usage

---

**Prev**: [Transport API ←](transport.md) | **Home**: [API Reference →](README.md)
