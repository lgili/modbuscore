---
layout: default
title: API Reference
nav_order: 4
has_children: true
---

# API Reference
{: .no_toc }

Complete function-level documentation for ModbusCore v1.0
{: .fs-6 .fw-300 }

---

## Overview

This section provides detailed API documentation for all public functions in ModbusCore v1.0. Each page includes function signatures, parameters, return values, examples, and best practices.

---

## API Modules

### [Status Codes](status.md)
Error handling and return values. All ModbusCore functions return `mbc_status_t` codes.

**Key Functions:**
- `mbc_status_is_ok()` – Check if status indicates success

**Learn About:**
- All status codes and their meanings
- Error handling patterns
- Best practices for error checking

---

### [PDU API](pdu.md)
Building and parsing Modbus Protocol Data Units.

**Request Builders:**
- `mbc_pdu_build_read_holding_request()` – FC03
- `mbc_pdu_build_write_single_register()` – FC06
- `mbc_pdu_build_write_multiple_registers()` – FC16

**Response Parsers:**
- `mbc_pdu_parse_read_holding_response()` – FC03
- `mbc_pdu_parse_write_single_response()` – FC06
- `mbc_pdu_parse_write_multiple_response()` – FC16
- `mbc_pdu_parse_exception()` – Exception responses

---

### [MBAP API](mbap.md)
Modbus TCP framing (7-byte header + PDU).

**Key Functions:**
- `mbc_mbap_encode()` – Create MBAP frame
- `mbc_mbap_decode()` – Parse MBAP frame
- `mbc_mbap_expected_length()` – Determine frame length from partial data

**Learn About:**
- MBAP frame format
- Incremental TCP reception
- Transaction ID management

---

### Runtime API
*(Coming soon)*

Dependency injection and runtime configuration.

**Key Functions:**
- `mbc_runtime_builder_init()`
- `mbc_runtime_builder_with_transport()`
- `mbc_runtime_builder_build()`

---

### Engine API
*(Coming soon)*

Protocol engine (FSM, request/response handling).

**Key Functions:**
- `mbc_engine_init()`
- `mbc_engine_submit_request()`
- `mbc_engine_step()`
- `mbc_engine_take_pdu()`

---

### Transport API
*(Coming soon)*

Transport layer abstraction (TCP, RTU, custom).

**Implementations:**
- POSIX TCP (Linux/macOS)
- Winsock TCP (Windows)
- Mock transport (testing)

---

## Quick Reference

### Common Patterns

#### Error Handling
```c
mbc_status_t status = mbc_function(...);
if (!mbc_status_is_ok(status)) {
    fprintf(stderr, "Error: %d\n", status);
    return 1;
}
```

#### Build and Send FC03 Request
```c
// Build PDU
mbc_pdu_t request;
mbc_pdu_build_read_holding_request(&request, unit_id, address, quantity);

// Serialize PDU
uint8_t pdu_buffer[256];
pdu_buffer[0] = request.function;
memcpy(&pdu_buffer[1], request.payload, request.payload_length);
size_t pdu_length = 1 + request.payload_length;

// Wrap with MBAP
mbc_mbap_header_t header = {.transaction_id = 1, .unit_id = unit_id};
uint8_t frame[260];
size_t frame_length;
mbc_mbap_encode(&header, pdu_buffer, pdu_length, frame, sizeof(frame), &frame_length);

// Send via engine
mbc_engine_submit_request(&engine, frame, frame_length);
```

#### Receive and Parse Response
```c
// Poll for response
for (int i = 0; i < 100; ++i) {
    mbc_engine_step(&engine, 10);

    mbc_pdu_t response;
    if (mbc_engine_take_pdu(&engine, &response)) {
        // Check for exception
        if (response.function & 0x80U) {
            uint8_t exception_code;
            mbc_pdu_parse_exception(&response, NULL, &exception_code);
            fprintf(stderr, "Exception: 0x%02X\n", exception_code);
            break;
        }

        // Parse FC03 response
        const uint8_t *data;
        size_t count;
        mbc_pdu_parse_read_holding_response(&response, &data, &count);

        // Process registers...
        break;
    }
}
```

---

## Related Documentation

- **[Quick Start Guide](../quick-start.md)** – Get started in 5 minutes
- **[Architecture Overview](../architecture.md)** – Understand the design
- **[Guides](../guides/)** – Testing, porting, troubleshooting

---

## Conventions

### Function Naming

All ModbusCore functions follow this pattern:
```
mbc_<module>_<action>()
```

Examples:
- `mbc_pdu_build_*()` – PDU builders
- `mbc_pdu_parse_*()` – PDU parsers
- `mbc_mbap_encode()` – MBAP encoding
- `mbc_engine_init()` – Engine initialization

### Return Values

- All fallible functions return `mbc_status_t`
- Use `mbc_status_is_ok()` to check success
- Zero (`MBC_STATUS_OK`) = success
- Negative values = error codes

### Parameter Order

1. **Input**: Source data, configuration
2. **Output**: Pointers for results (marked with `out_` prefix)
3. **Optional**: NULL-able parameters (usually output)

Example:
```c
mbc_status_t mbc_mbap_encode(
    const mbc_mbap_header_t *header,    // Input
    const uint8_t *pdu,                 // Input
    size_t pdu_length,                  // Input
    uint8_t *out_buffer,                // Output
    size_t out_capacity,                // Input (buffer size)
    size_t *out_length                  // Output (optional, can be NULL)
);
```

### Const Correctness

- Input parameters are `const`
- Output parameters are mutable
- Structures returned by parsers may point into const buffers (zero-copy)

---

## Need Help?

- **Examples**: See [Quick Start Guide](../quick-start.md)
- **Troubleshooting**: Check [Troubleshooting Guide](../guides/troubleshooting.md)
- **Issues**: [GitHub Issues](https://github.com/lgili/modbuscore/issues)
