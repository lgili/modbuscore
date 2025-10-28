# API Reference

Complete API documentation for ModbusCore v1.0.0.

## Overview

ModbusCore is a **cross-platform, zero-copy Modbus library** designed for:

- Embedded systems (bare-metal, FreeRTOS, Zephyr)
- Desktop/server applications (Linux, Windows, macOS)
- Portable C99 code with minimal dependencies

This API reference covers all public interfaces organized by functional area.

## API Modules

### Core Protocol
- **[Common Types & Status Codes](common.md)** - Status codes, structures, constants
- **[Protocol Functions](protocol.md)** - PDU, MBAP, RTU encoding/decoding

### Runtime & Transport
- **[Runtime API](runtime.md)** - Runtime builder and dependency management
- **[Transport API](transport.md)** - TCP/RTU transport drivers (POSIX, Windows)

### Diagnostics & Testing
- **[Diagnostics API](diagnostics.md)** - Logging, tracing, performance monitoring

## Quick Navigation

| Category | What's Here | Key Functions |
|----------|-------------|---------------|
| **[Protocol](protocol.md)** | Low-level PDU, MBAP, RTU | `mbc_pdu_build_*`, `mbc_mbap_encode`, `mbc_rtu_encode` |
| **[Common](common.md)** | Status codes, types, constants | `mbc_status_t`, `mbc_pdu_t`, `MBC_PDU_MAX` |
| **[Runtime](runtime.md)** | Runtime builder, dependencies | `mbc_runtime_builder_*`, `mbc_runtime_init` |
| **[Transport](transport.md)** | TCP and RTU drivers | `mbc_posix_tcp_create`, `mbc_posix_rtu_create` |
| **[Diagnostics](diagnostics.md)** | Logging, tracing, stats | `mbc_diagnostics_init`, `mbc_diagnostics_trace_frame` |

## Getting Started

1. **Understand basics**: Start with [Common Types](common.md) for data structures and status codes
2. **Protocol layer**: Read [Protocol Functions](protocol.md) for PDU/MBAP/RTU encoding/decoding
3. **Configure runtime**: Use [Runtime API](runtime.md) with builder pattern
4. **Choose transport**: Select TCP or RTU from [Transport API](transport.md)
5. **Enable debugging**: Add diagnostics with [Diagnostics API](diagnostics.md)

## Complete API Coverage

✅ **Protocol Layer** - All PDU building, MBAP encoding, RTU framing, CRC functions  
✅ **Common Types** - Status codes, structures, constants, helper macros  
✅ **Runtime** - Builder pattern, dependency injection, lifecycle management  
✅ **Transport** - POSIX/Windows TCP, POSIX/Windows RTU, mock transports  
✅ **Diagnostics** - Frame tracing, counters, error logging, performance stats  

## Platform Support

| Platform | TCP Transport | RTU Transport |
|----------|---------------|---------------|
| **Linux** | `posix_tcp.h` | `posix_rtu.h` |
| **macOS** | `posix_tcp.h` | `posix_rtu.h` |
| **Windows** | `winsock_tcp.h` | `win32_rtu.h` |
| **Embedded** | Custom | `rtu_uart.h` |
| **Testing** | `mock.h` | `mock.h` |

## Header Organization

```
include/modbuscore/
├── common/
│   └── status.h           # Status codes, error handling
├── protocol/
│   ├── pdu.h              # PDU building/parsing
│   ├── mbap.h             # MBAP encoding/decoding
│   ├── crc.h              # CRC-16 calculation
│   ├── engine.h           # Protocol engine (advanced)
│   └── events.h           # Event definitions
├── runtime/
│   ├── runtime.h          # Runtime management
│   ├── builder.h          # Runtime builder
│   ├── dependencies.h     # Dependency interfaces
│   ├── diagnostics.h      # Diagnostics system
│   └── autoheal.h         # Auto-heal feature
└── transport/
    ├── iface.h            # Transport interface
    ├── posix_tcp.h        # POSIX TCP driver
    ├── posix_rtu.h        # POSIX RTU driver
    ├── winsock_tcp.h      # Windows TCP driver
    ├── win32_rtu.h        # Windows RTU driver
    ├── rtu_uart.h         # Generic UART RTU
    └── mock.h             # Mock transport (testing)
```

## Example: Complete Workflow

```c
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_tcp.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

int main(void) {
    // 1. Create transport
    mbc_posix_tcp_config_t cfg = {
        .host = "192.168.1.100",
        .port = 502,
        .connect_timeout_ms = 5000,
        .recv_timeout_ms = 1000
    };
    
    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t *ctx;
    mbc_posix_tcp_create(&cfg, &iface, &ctx);
    
    // 2. Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    // 3. Build request
    mbc_pdu_t pdu;
    mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    // 4. Encode and send
    mbc_mbap_header_t hdr = { .transaction_id = 1, .unit_id = 1 };
    uint8_t buffer[260];
    size_t length;
    mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &length);
    
    mbc_transport_send(&iface, buffer, length, NULL);
    
    // 5. Cleanup
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(ctx);
    
    return 0;
}
```

## Most Used Functions

| Function | Purpose | Header |
|----------|---------|--------|
| `mbc_pdu_build_read_holding_request()` | Build read request | `protocol/pdu.h` |
| `mbc_mbap_encode()` | Encode TCP frame | `protocol/mbap.h` |
| `mbc_rtu_encode()` | Encode RTU frame with CRC | `protocol/rtu.h` |
| `mbc_mbap_decode()` | Decode TCP frame | `protocol/mbap.h` |
| `mbc_rtu_decode()` | Decode RTU frame, verify CRC | `protocol/rtu.h` |
| `mbc_runtime_builder_build()` | Build runtime instance | `runtime/builder.h` |
| `mbc_posix_tcp_create()` | Create TCP transport | `transport/posix_tcp.h` |
| `mbc_diagnostics_init()` | Initialize diagnostics | `runtime/diagnostics.h` |

## Status Codes

All functions return `mbc_status_t`:

```c
typedef enum {
    MBC_STATUS_OK = 0,                // Success
    MBC_STATUS_INVALID_PARAMETER,     // Invalid parameter
    MBC_STATUS_BUFFER_TOO_SMALL,      // Output buffer too small
    MBC_STATUS_INVALID_FUNCTION,      // Unsupported function code
    MBC_STATUS_CRC_ERROR,             // CRC mismatch (RTU)
    MBC_STATUS_TIMEOUT,               // Operation timeout
    MBC_STATUS_TRANSPORT_ERROR,       // Transport error
    MBC_STATUS_WOULD_BLOCK,           // Non-blocking would block
    // ... see common.md for complete list
} mbc_status_t;
```

**Error Handling:**
```c
mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
if (status != MBC_STATUS_OK) {
    fprintf(stderr, "Error: %d\n", status);
    return status;
}
```

## Thread Safety

ModbusCore APIs are **not thread-safe by default**. For multi-threaded use:

### Option 1: Separate Instances Per Thread

```c
// Thread 1
mbc_runtime_t runtime1;

// Thread 2
mbc_runtime_t runtime2;
```

### Option 2: Mutex Protection

```c
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_lock(&lock);
// Use ModbusCore APIs
mbc_transport_send(&iface, buffer, length, NULL);
pthread_mutex_unlock(&lock);
```

## Memory Requirements

### Stack Usage

| Operation | Stack Size |
|-----------|-----------|
| Build PDU | ~260 bytes |
| Encode MBAP | ~280 bytes |
| Encode RTU | ~260 bytes |
| Full transaction | ~600 bytes |

### Heap Usage

ModbusCore **never allocates heap memory** by default (zero `malloc`/`free` calls).

Custom allocators can be provided via runtime builder.

## Performance

### Typical Latencies (ARM Cortex-M4 @ 80MHz)

| Operation | CPU Cycles |
|-----------|-----------|
| `mbc_pdu_build_*()` | ~100 |
| `mbc_mbap_encode()` | ~200 |
| `mbc_rtu_encode()` | ~500 (CRC) |
| `mbc_rtu_calculate_crc()` | ~400 |

### Network Latency

| Transport | Typical Latency |
|-----------|-----------------|
| TCP (LAN) | ~500 µs |
| RTU @ 9600 baud | ~10-50 ms |
| RTU @ 115200 baud | ~1-5 ms |

## Next Steps

- **[Common Types](common.md)** → Understand status codes and data structures
- **[Protocol API](protocol.md)** → Learn PDU/MBAP/RTU encoding
- **[Runtime API](runtime.md)** → Configure runtime with builder pattern
- **[Transport API](transport.md)** → Choose TCP or RTU transport
- **[Diagnostics API](diagnostics.md)** → Enable logging and tracing

## See Also

- [User Guide](../user-guide/README.md) - Architectural overview and usage patterns
- [Getting Started](../getting-started/README.md) - Installation and first application
- [Examples](../getting-started/examples.md) - Complete code samples
- [Troubleshooting](../reference/troubleshooting.md) - Common issues and solutions

---

**Next**: [Common Types →](common.md) | **Home**: [Documentation →](../README.md)
