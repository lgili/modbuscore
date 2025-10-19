---
layout: default
title: Status Codes
parent: API Reference
nav_order: 1
---

# Status Codes API
{: .no_toc }

Error handling and return values in ModbusCore v1.0
{: .fs-6 .fw-300 }

## Table of Contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

All ModbusCore functions that can fail return a `mbc_status_t` code. This provides consistent error handling across the entire library.

**Header:** `<modbuscore/common/status.h>`

---

## Status Type

```c
typedef enum mbc_status {
    MBC_STATUS_OK = 0,                      // Operation completed successfully
    MBC_STATUS_INVALID_ARGUMENT = -1,       // Invalid function argument
    MBC_STATUS_ALREADY_INITIALISED = -2,    // Component already initialized
    MBC_STATUS_NOT_INITIALISED = -3,        // Component not initialized
    MBC_STATUS_UNSUPPORTED = -4,            // Unsupported operation or function code
    MBC_STATUS_IO_ERROR = -5,               // I/O error (network, serial, etc.)
    MBC_STATUS_BUSY = -6,                   // Resource is busy
    MBC_STATUS_NO_RESOURCES = -7,           // Insufficient resources (memory, buffer)
    MBC_STATUS_DECODING_ERROR = -8,         // Frame/PDU decoding error
    MBC_STATUS_TIMEOUT = -9                 // Operation timeout
} mbc_status_t;
```

---

## Status Codes

### MBC_STATUS_OK (0)

**Description:** Operation completed successfully.

**When Returned:**
- All operations that succeed
- Functions that complete without errors

**Example:**
```c
mbc_status_t status = mbc_engine_init(&engine, &config);
if (mbc_status_is_ok(status)) {
    printf("Engine initialized successfully\n");
}
```

---

### MBC_STATUS_INVALID_ARGUMENT (-1)

**Description:** One or more function arguments are invalid (NULL pointer, out of range, etc.).

**Common Causes:**
- NULL pointer passed to required parameter
- Invalid value (e.g., quantity > 125 for FC03)
- Buffer size insufficient

**Example:**
```c
// This will return INVALID_ARGUMENT (pdu is NULL)
mbc_status_t status = mbc_pdu_build_read_holding_request(NULL, 1, 0, 10);

// This will return INVALID_ARGUMENT (quantity > 125)
mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 200);
```

**Fix:**
- Check all pointers are non-NULL
- Verify value ranges (FC03: quantity 1-125)
- Ensure buffer capacity is sufficient

---

### MBC_STATUS_ALREADY_INITIALISED (-2)

**Description:** Attempting to initialize a component that's already initialized.

**Common Causes:**
- Calling `mbc_engine_init()` twice without shutdown
- Calling `mbc_runtime_init()` on already initialized runtime

**Example:**
```c
mbc_engine_init(&engine, &config);
// ...
mbc_engine_init(&engine, &config);  // Returns ALREADY_INITIALISED
```

**Fix:**
- Call shutdown before re-initializing
- Check initialization state before init

---

### MBC_STATUS_NOT_INITIALISED (-3)

**Description:** Component not initialized before use.

**Common Causes:**
- Using engine before calling `mbc_engine_init()`
- Using runtime before calling `mbc_runtime_init()`
- Using component after shutdown

**Example:**
```c
mbc_engine_t engine;  // Not initialized!
mbc_status_t status = mbc_engine_step(&engine, 10);  // Returns NOT_INITIALISED
```

**Fix:**
- Always initialize components before use
- Check `mbc_engine_is_ready()` or `mbc_runtime_is_ready()`

---

### MBC_STATUS_UNSUPPORTED (-4)

**Description:** Unsupported operation or function code.

**Common Causes:**
- Server received unknown function code
- Client parsing unexpected response format
- Feature not implemented yet

**Example:**
```c
// Server receives FC99 (unsupported)
// Engine returns UNSUPPORTED
```

**Fix:**
- Verify function code is supported (FC03, FC06, FC16)
- Check server capabilities
- Update library if feature is missing

---

### MBC_STATUS_IO_ERROR (-5)

**Description:** I/O operation failed (network, serial port, etc.).

**Common Causes:**
- TCP connection lost
- Serial port disconnected
- Network unreachable
- Partial send/receive

**Example:**
```c
mbc_status_t status = mbc_transport_send(&transport, buffer, length, &io);
if (status == MBC_STATUS_IO_ERROR) {
    fprintf(stderr, "Network error: check connection\n");
}
```

**Fix:**
- Check network connectivity
- Verify cable connections (serial)
- Retry connection
- Check firewall settings

---

### MBC_STATUS_BUSY (-6)

**Description:** Resource is currently busy.

**Common Causes:**
- Submitting request while engine is waiting for response
- Engine in wrong state for operation

**Example:**
```c
// Submit first request
mbc_engine_submit_request(&engine, req1, len1);

// Try to submit second request immediately (engine is WAIT_RESPONSE)
status = mbc_engine_submit_request(&engine, req2, len2);  // Returns BUSY
```

**Fix:**
- Wait for current operation to complete
- Check engine state before submitting
- Poll with `mbc_engine_step()` until response received

---

### MBC_STATUS_NO_RESOURCES (-7)

**Description:** Insufficient resources (memory, buffer space, etc.).

**Common Causes:**
- RX buffer full
- PDU payload exceeds maximum size
- Memory allocation failed (if using dynamic allocator)

**Example:**
```c
// RX buffer full (260 bytes max)
status = mbc_engine_step(&engine, 10);  // Returns NO_RESOURCES if buffer full
```

**Fix:**
- Process pending PDUs before receiving more data
- Reduce buffer usage
- Increase buffer sizes (if modifiable)

---

### MBC_STATUS_DECODING_ERROR (-8)

**Description:** Frame or PDU decoding error.

**Common Causes:**
- Corrupted MBAP header
- Invalid PDU format
- Malformed response
- CRC error (RTU mode, Phase 5)

**Example:**
```c
// Invalid MBAP frame (protocol ID != 0)
status = mbc_mbap_decode(buffer, length, &header, &pdu, &pdu_len);
// Returns DECODING_ERROR
```

**Fix:**
- Check data integrity
- Verify server is Modbus-compliant
- Use network capture (Wireshark) to inspect packets

---

### MBC_STATUS_TIMEOUT (-9)

**Description:** Operation timeout.

**Common Causes:**
- Server not responding
- Network latency too high
- Wrong server address/port
- Server busy or offline

**Example:**
```c
// Wait for response with timeout
for (int i = 0; i < 100; ++i) {
    status = mbc_engine_step(&engine, 10);
    if (status == MBC_STATUS_TIMEOUT) {
        fprintf(stderr, "Server timeout\n");
        break;
    }
}
```

**Fix:**
- Increase `response_timeout_ms` in engine config
- Check server is running and reachable
- Verify network connectivity
- Reduce network latency

---

## Helper Functions

### mbc_status_is_ok()

**Signature:**
```c
static inline int mbc_status_is_ok(mbc_status_t status);
```

**Description:** Check if status indicates success.

**Parameters:**
- `status` – Status code to check

**Returns:**
- `1` if status is `MBC_STATUS_OK`
- `0` otherwise

**Example:**
```c
mbc_status_t status = mbc_engine_init(&engine, &config);
if (!mbc_status_is_ok(status)) {
    fprintf(stderr, "Engine init failed: %d\n", status);
    return 1;
}
```

---

## Error Handling Patterns

### Pattern 1: Simple Check

```c
mbc_status_t status = mbc_function(...);
if (!mbc_status_is_ok(status)) {
    fprintf(stderr, "Error: %d\n", status);
    return 1;
}
```

### Pattern 2: Switch on Error Type

```c
mbc_status_t status = mbc_engine_step(&engine, budget);

switch (status) {
    case MBC_STATUS_OK:
        // Success
        break;

    case MBC_STATUS_TIMEOUT:
        fprintf(stderr, "Timeout waiting for response\n");
        retry_request();
        break;

    case MBC_STATUS_IO_ERROR:
        fprintf(stderr, "Network error\n");
        reconnect();
        break;

    default:
        fprintf(stderr, "Unexpected error: %d\n", status);
        abort_operation();
}
```

### Pattern 3: Goto Cleanup on Error

```c
mbc_status_t status;

status = mbc_posix_tcp_create(&tcp_config, &transport, &tcp_ctx);
if (!mbc_status_is_ok(status)) {
    fprintf(stderr, "TCP create failed\n");
    return 1;
}

status = mbc_runtime_builder_build(&builder, &runtime);
if (!mbc_status_is_ok(status)) {
    fprintf(stderr, "Runtime build failed\n");
    goto cleanup_tcp;
}

status = mbc_engine_init(&engine, &engine_config);
if (!mbc_status_is_ok(status)) {
    fprintf(stderr, "Engine init failed\n");
    goto cleanup_runtime;
}

// ... normal operation ...

// Cleanup in reverse order
mbc_engine_shutdown(&engine);
cleanup_runtime:
    mbc_runtime_shutdown(&runtime);
cleanup_tcp:
    mbc_posix_tcp_destroy(tcp_ctx);

return 0;
```

### Pattern 4: Error to String (Custom Helper)

```c
const char *mbc_status_to_string(mbc_status_t status) {
    switch (status) {
        case MBC_STATUS_OK: return "OK";
        case MBC_STATUS_INVALID_ARGUMENT: return "Invalid argument";
        case MBC_STATUS_ALREADY_INITIALISED: return "Already initialized";
        case MBC_STATUS_NOT_INITIALISED: return "Not initialized";
        case MBC_STATUS_UNSUPPORTED: return "Unsupported";
        case MBC_STATUS_IO_ERROR: return "I/O error";
        case MBC_STATUS_BUSY: return "Busy";
        case MBC_STATUS_NO_RESOURCES: return "No resources";
        case MBC_STATUS_DECODING_ERROR: return "Decoding error";
        case MBC_STATUS_TIMEOUT: return "Timeout";
        default: return "Unknown error";
    }
}

// Usage:
mbc_status_t status = mbc_function(...);
printf("Status: %s\n", mbc_status_to_string(status));
```

---

## Best Practices

### ✅ DO

- **Always check return values** from all ModbusCore functions
- **Use `mbc_status_is_ok()`** for simple success checks
- **Log error codes** for debugging
- **Handle timeout separately** from other errors (may need retry)
- **Cleanup resources** on error (goto pattern recommended)

### ❌ DON'T

- **Ignore return values** – All functions can fail!
- **Assume success** – Always check status
- **Use magic numbers** – Use enum values (e.g., `MBC_STATUS_TIMEOUT`)
- **Leak resources** on error – Always cleanup

---

## Related APIs

- [**PDU API**](pdu.md) – PDU encoding/decoding functions
- [**Engine API**](engine.md) – Protocol engine operations
- [**Runtime API**](runtime.md) – Dependency management
- [**MBAP API**](mbap.md) – TCP framing functions

---

## Example: Complete Error Handling

```c
#include <modbuscore/common/status.h>
#include <stdio.h>

int main(void) {
    mbc_transport_iface_t transport;
    mbc_posix_tcp_ctx_t *tcp_ctx = NULL;
    mbc_runtime_t runtime = {0};
    mbc_engine_t engine = {0};
    int exit_code = 1;

    // Create TCP transport
    mbc_posix_tcp_config_t tcp_config = {
        .host = "192.168.1.100",
        .port = 502,
        .connect_timeout_ms = 5000,
        .recv_timeout_ms = 1000,
    };

    mbc_status_t status = mbc_posix_tcp_create(&tcp_config, &transport, &tcp_ctx);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "✗ TCP connect failed: %d\n", status);
        return 1;
    }
    printf("✓ TCP connected\n");

    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    status = mbc_runtime_builder_build(&builder, &runtime);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "✗ Runtime build failed: %d\n", status);
        goto cleanup_tcp;
    }
    printf("✓ Runtime initialized\n");

    // Initialize engine
    mbc_engine_config_t engine_config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .response_timeout_ms = 3000,
    };

    status = mbc_engine_init(&engine, &engine_config);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "✗ Engine init failed: %d\n", status);
        goto cleanup_runtime;
    }
    printf("✓ Engine ready\n");

    // ... perform operations ...

    exit_code = 0;  // Success!

    // Cleanup (reverse order)
    mbc_engine_shutdown(&engine);
cleanup_runtime:
    mbc_runtime_shutdown(&runtime);
cleanup_tcp:
    mbc_posix_tcp_destroy(tcp_ctx);

    return exit_code;
}
```
