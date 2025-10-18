---
layout: default
title: PDU API
parent: API Reference
nav_order: 2
---

# PDU API
{: .no_toc }

Protocol Data Unit encoding and decoding functions
{: .fs-6 .fw-300 }

## Table of Contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

The PDU (Protocol Data Unit) API provides functions for building and parsing Modbus requests and responses. PDUs are the core data structures that represent Modbus operations, independent of the transport layer (TCP or RTU).

**Header:** `<modbuscore/protocol/pdu.h>`

---

## PDU Structure

```c
typedef struct mbc_pdu {
    uint8_t unit_id;                /**< Unit/Slave ID (0-247) */
    uint8_t function;               /**< Function code (1-127, or 0x80+ for exceptions) */
    uint8_t payload[MBC_PDU_MAX];   /**< PDU payload data */
    size_t payload_length;          /**< Length of payload in bytes */
} mbc_pdu_t;
```

**Constants:**
```c
#define MBC_PDU_MAX 253U  // Maximum PDU payload size (Modbus spec)
```

---

## Core Functions

### mbc_pdu_encode()

Encode PDU to byte buffer (for low-level use, usually not needed by applications).

**Signature:**
```c
mbc_status_t mbc_pdu_encode(const mbc_pdu_t *pdu,
                            uint8_t *buffer,
                            size_t capacity,
                            size_t *out_length);
```

**Parameters:**
- `pdu` – PDU structure to encode
- `buffer` – Output buffer
- `capacity` – Buffer capacity in bytes
- `out_length` – (Output) Encoded length in bytes (can be NULL)

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_INVALID_ARGUMENT` if arguments invalid
- `MBC_STATUS_NO_RESOURCES` if buffer too small

**Example:**
```c
mbc_pdu_t pdu;
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

uint8_t buffer[256];
size_t length;
mbc_status_t status = mbc_pdu_encode(&pdu, buffer, sizeof(buffer), &length);
```

---

### mbc_pdu_decode()

Decode byte buffer to PDU (for low-level use, usually not needed by applications).

**Signature:**
```c
mbc_status_t mbc_pdu_decode(const uint8_t *buffer,
                            size_t length,
                            mbc_pdu_t *out);
```

**Parameters:**
- `buffer` – Input buffer
- `length` – Buffer length in bytes
- `out` – (Output) Decoded PDU structure

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_INVALID_ARGUMENT` if arguments invalid
- `MBC_STATUS_DECODING_ERROR` if invalid PDU format

---

## Request Builders

### mbc_pdu_build_read_holding_request()

Build FC03 (Read Holding Registers) request.

**Signature:**
```c
mbc_status_t mbc_pdu_build_read_holding_request(mbc_pdu_t *pdu,
                                                uint8_t unit_id,
                                                uint16_t address,
                                                uint16_t quantity);
```

**Parameters:**
- `pdu` – PDU structure to fill
- `unit_id` – Unit/Slave ID (1-247, use 0 for broadcast RTU)
- `address` – Starting register address (0-65535)
- `quantity` – Number of registers to read (1-125)

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_INVALID_ARGUMENT` if quantity out of range (0 or >125)

**Example:**
```c
mbc_pdu_t request;

// Read 10 registers starting at address 100 from unit 1
mbc_status_t status = mbc_pdu_build_read_holding_request(&request, 1, 100, 10);

if (mbc_status_is_ok(status)) {
    printf("Function: 0x%02X\n", request.function);  // 0x03
    printf("Payload length: %zu\n", request.payload_length);  // 4 bytes
}
```

**PDU Format:**
```
Function Code: 0x03
Payload: [Address High][Address Low][Quantity High][Quantity Low]
Total: 1 + 4 = 5 bytes
```

---

### mbc_pdu_build_write_single_register()

Build FC06 (Write Single Register) request.

**Signature:**
```c
mbc_status_t mbc_pdu_build_write_single_register(mbc_pdu_t *pdu,
                                                 uint8_t unit_id,
                                                 uint16_t address,
                                                 uint16_t value);
```

**Parameters:**
- `pdu` – PDU structure to fill
- `unit_id` – Unit/Slave ID
- `address` – Register address to write (0-65535)
- `value` – 16-bit value to write

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_INVALID_ARGUMENT` if pdu is NULL

**Example:**
```c
mbc_pdu_t request;

// Write value 1234 to register 500 on unit 1
mbc_pdu_build_write_single_register(&request, 1, 500, 1234);
```

**PDU Format:**
```
Function Code: 0x06
Payload: [Address High][Address Low][Value High][Value Low]
Total: 1 + 4 = 5 bytes
```

---

### mbc_pdu_build_write_multiple_registers()

Build FC16 (Write Multiple Registers) request.

**Signature:**
```c
mbc_status_t mbc_pdu_build_write_multiple_registers(mbc_pdu_t *pdu,
                                                    uint8_t unit_id,
                                                    uint16_t address,
                                                    const uint16_t *values,
                                                    size_t quantity);
```

**Parameters:**
- `pdu` – PDU structure to fill
- `unit_id` – Unit/Slave ID
- `address` – Starting register address
- `values` – Array of register values to write
- `quantity` – Number of registers to write (1-123)

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_INVALID_ARGUMENT` if quantity out of range or values is NULL

**Example:**
```c
mbc_pdu_t request;
uint16_t values[] = {100, 200, 300, 400, 500};

// Write 5 registers starting at address 0 on unit 1
mbc_pdu_build_write_multiple_registers(&request, 1, 0, values, 5);
```

**PDU Format:**
```
Function Code: 0x10
Payload: [Addr High][Addr Low][Qty High][Qty Low][Byte Count][Data...]
Total: 1 + 5 + (quantity * 2) bytes
```

**Note:** Maximum quantity is 123 registers (246 bytes of data + 5 bytes header = 251 < 253 max PDU).

---

## Response Parsers

### mbc_pdu_parse_read_holding_response()

Parse FC03 (Read Holding Registers) response.

**Signature:**
```c
mbc_status_t mbc_pdu_parse_read_holding_response(const mbc_pdu_t *pdu,
                                                 const uint8_t **out_data,
                                                 size_t *out_registers);
```

**Parameters:**
- `pdu` – Response PDU to parse
- `out_data` – (Output) Pointer to register data (big-endian uint16 pairs)
- `out_registers` – (Output) Number of registers in response

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_INVALID_ARGUMENT` if arguments are NULL
- `MBC_STATUS_DECODING_ERROR` if invalid format

**Example:**
```c
mbc_pdu_t response;
// ... (receive response via engine) ...

const uint8_t *data;
size_t count;

mbc_status_t status = mbc_pdu_parse_read_holding_response(&response, &data, &count);

if (mbc_status_is_ok(status)) {
    printf("Received %zu registers:\n", count);
    for (size_t i = 0; i < count; ++i) {
        // Data is big-endian: [high byte][low byte]
        uint16_t value = (data[i * 2] << 8) | data[i * 2 + 1];
        printf("  Register[%zu] = %u (0x%04X)\n", i, value, value);
    }
}
```

**Response Format:**
```
Function Code: 0x03
Payload: [Byte Count][Data High][Data Low]...
```

**Note:** Data pointer points into the PDU payload (zero-copy).

---

### mbc_pdu_parse_write_single_response()

Parse FC06 (Write Single Register) response.

**Signature:**
```c
mbc_status_t mbc_pdu_parse_write_single_response(const mbc_pdu_t *pdu,
                                                 uint16_t *out_address,
                                                 uint16_t *out_value);
```

**Parameters:**
- `pdu` – Response PDU to parse
- `out_address` – (Output) Register address that was written
- `out_value` – (Output) Value that was written

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_DECODING_ERROR` if invalid format

**Example:**
```c
mbc_pdu_t response;
uint16_t address, value;

mbc_pdu_parse_write_single_response(&response, &address, &value);
printf("Wrote %u to register %u\n", value, address);
```

---

### mbc_pdu_parse_write_multiple_response()

Parse FC16 (Write Multiple Registers) response.

**Signature:**
```c
mbc_status_t mbc_pdu_parse_write_multiple_response(const mbc_pdu_t *pdu,
                                                   uint16_t *out_address,
                                                   uint16_t *out_quantity);
```

**Parameters:**
- `pdu` – Response PDU to parse
- `out_address` – (Output) Starting register address
- `out_quantity` – (Output) Number of registers written

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_DECODING_ERROR` if invalid format

**Example:**
```c
mbc_pdu_t response;
uint16_t address, quantity;

mbc_pdu_parse_write_multiple_response(&response, &address, &quantity);
printf("Wrote %u registers starting at %u\n", quantity, address);
```

---

## Exception Handling

### mbc_pdu_parse_exception()

Parse exception response.

**Signature:**
```c
mbc_status_t mbc_pdu_parse_exception(const mbc_pdu_t *pdu,
                                     uint8_t *out_function,
                                     uint8_t *out_code);
```

**Parameters:**
- `pdu` – Exception PDU to parse (function code has bit 0x80 set)
- `out_function` – (Output) Original function code that caused exception
- `out_code` – (Output) Exception code

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_DECODING_ERROR` if not a valid exception

**Exception Codes:**
- `0x01` – ILLEGAL FUNCTION
- `0x02` – ILLEGAL DATA ADDRESS
- `0x03` – ILLEGAL DATA VALUE
- `0x04` – SLAVE DEVICE FAILURE
- `0x05` – ACKNOWLEDGE
- `0x06` – SLAVE DEVICE BUSY
- `0x08` – MEMORY PARITY ERROR
- `0x0A` – GATEWAY PATH UNAVAILABLE
- `0x0B` – GATEWAY TARGET DEVICE FAILED TO RESPOND

**Example:**
```c
mbc_pdu_t response;

// Check if exception (bit 7 set in function code)
if (response.function & 0x80U) {
    uint8_t original_fc, exception_code;
    mbc_pdu_parse_exception(&response, &original_fc, &exception_code);

    printf("Exception on FC%02X: ", original_fc);
    switch (exception_code) {
        case 0x01:
            printf("ILLEGAL FUNCTION\n");
            break;
        case 0x02:
            printf("ILLEGAL DATA ADDRESS\n");
            break;
        case 0x03:
            printf("ILLEGAL DATA VALUE\n");
            break;
        case 0x04:
            printf("SLAVE DEVICE FAILURE\n");
            break;
        default:
            printf("Unknown exception (0x%02X)\n", exception_code);
    }
}
```

---

## Complete Example

### Read Holding Registers (FC03)

```c
#include <modbuscore/protocol/pdu.h>
#include <stdio.h>

void example_fc03(void)
{
    mbc_pdu_t request, response;

    // Build FC03 request
    mbc_pdu_build_read_holding_request(&request, 1, 0, 10);
    printf("Request FC: 0x%02X\n", request.function);
    printf("Request payload: %zu bytes\n", request.payload_length);

    // ... (send request and receive response via engine) ...

    // Check for exception
    if (response.function & 0x80U) {
        uint8_t exception_code;
        mbc_pdu_parse_exception(&response, NULL, &exception_code);
        fprintf(stderr, "Exception: 0x%02X\n", exception_code);
        return;
    }

    // Parse normal response
    const uint8_t *data;
    size_t count;
    if (mbc_pdu_parse_read_holding_response(&response, &data, &count) == MBC_STATUS_OK) {
        printf("Read %zu registers:\n", count);
        for (size_t i = 0; i < count; ++i) {
            uint16_t value = (data[i * 2] << 8) | data[i * 2 + 1];
            printf("  [%zu] = %u\n", i, value);
        }
    }
}
```

### Write Multiple Registers (FC16)

```c
void example_fc16(void)
{
    mbc_pdu_t request, response;
    uint16_t values[] = {100, 200, 300, 400, 500};

    // Build FC16 request
    mbc_pdu_build_write_multiple_registers(&request, 1, 0, values, 5);

    // ... (send and receive) ...

    // Parse response
    uint16_t address, quantity;
    if (mbc_pdu_parse_write_multiple_response(&response, &address, &quantity) == MBC_STATUS_OK) {
        printf("Wrote %u registers starting at %u\n", quantity, address);
    }
}
```

---

## Best Practices

### ✅ DO

- **Check for exceptions** before parsing normal responses
- **Validate quantities** (FC03: 1-125, FC16: 1-123)
- **Use zero-copy parsing** – Don't copy register data unless needed
- **Handle all exception codes** – Some indicate temporary conditions

### ❌ DON'T

- **Exceed max quantities** – Will return INVALID_ARGUMENT
- **Ignore exception responses** – They indicate errors!
- **Assume response matches request** – Always check function code
- **Modify data pointer** – It points into PDU structure

---

## PDU Format Reference

### FC03 Request (Read Holding Registers)
```
[FC=0x03][Addr Hi][Addr Lo][Qty Hi][Qty Lo]
5 bytes total
```

### FC03 Response
```
[FC=0x03][Byte Count][Data Hi][Data Lo]...
3 + (quantity * 2) bytes
```

### FC06 Request (Write Single Register)
```
[FC=0x06][Addr Hi][Addr Lo][Value Hi][Value Lo]
5 bytes total
```

### FC06 Response (Echo of Request)
```
[FC=0x06][Addr Hi][Addr Lo][Value Hi][Value Lo]
5 bytes total
```

### FC16 Request (Write Multiple Registers)
```
[FC=0x10][Addr Hi][Addr Lo][Qty Hi][Qty Lo][Byte Count][Data...]
7 + (quantity * 2) bytes
```

### FC16 Response
```
[FC=0x10][Addr Hi][Addr Lo][Qty Hi][Qty Lo]
5 bytes total
```

### Exception Response
```
[FC=0x80+original][Exception Code]
2 bytes total
```

---

## Related APIs

- [**Status Codes**](status.md) – Error handling
- [**Engine API**](engine.md) – Sending/receiving PDUs
- [**MBAP API**](mbap.md) – TCP framing
- [**Quick Start**](../quick-start.md) – Complete examples
