---
layout: default
title: MBAP API
parent: API Reference
nav_order: 3
---

# MBAP API
{: .no_toc }

Modbus Application Protocol (TCP framing) functions
{: .fs-6 .fw-300 }

## Table of Contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

MBAP (Modbus Application Protocol) is the framing protocol used for Modbus over TCP/IP. It adds a 7-byte header to the PDU, providing transaction IDs, protocol identification, and frame length.

**Header:** `<modbuscore/protocol/mbap.h>`

---

## MBAP Frame Format

```
┌────────────┬────────────┬────────┬─────────┬─────────────┐
│ Trans ID   │ Protocol   │ Length │ Unit ID │ PDU         │
│ (2 bytes)  │ ID (2)     │ (2)    │ (1)     │ (variable)  │
└────────────┴────────────┴────────┴─────────┴─────────────┘
   0-1          2-3          4-5       6        7+

Total: 7 bytes header + PDU length
```

**Fields:**
- **Transaction ID** (2 bytes) – Client-assigned ID to match requests/responses
- **Protocol ID** (2 bytes) – Always `0x0000` for Modbus
- **Length** (2 bytes) – Byte count of Unit ID + PDU (not including MBAP header)
- **Unit ID** (1 byte) – Slave address (usually 1 for TCP, 0xFF for broadcast)
- **PDU** (variable) – Modbus Protocol Data Unit

---

## MBAP Header Structure

```c
typedef struct mbc_mbap_header {
    uint16_t transaction_id;  /**< Transaction ID (client-assigned) */
    uint16_t protocol_id;     /**< Protocol ID (always 0 for Modbus) */
    uint16_t length;          /**< Length field (Unit ID + PDU length) */
    uint8_t unit_id;          /**< Unit/Slave ID */
} mbc_mbap_header_t;
```

---

## Functions

### mbc_mbap_encode()

Encode MBAP frame (header + PDU) into output buffer.

**Signature:**
```c
mbc_status_t mbc_mbap_encode(const mbc_mbap_header_t *header,
                              const uint8_t *pdu,
                              size_t pdu_length,
                              uint8_t *out_buffer,
                              size_t out_capacity,
                              size_t *out_length);
```

**Parameters:**
- `header` – MBAP header (transaction_id, unit_id; protocol_id=0, length auto-calculated)
- `pdu` – PDU data (function code + payload)
- `pdu_length` – PDU length in bytes
- `out_buffer` – Output buffer for complete MBAP frame
- `out_capacity` – Output buffer capacity
- `out_length` – (Output) Actual frame length written

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_INVALID_ARGUMENT` if arguments invalid
- `MBC_STATUS_NO_RESOURCES` if buffer too small

**Example:**
```c
// Build PDU
mbc_pdu_t request_pdu;
mbc_pdu_build_read_holding_request(&request_pdu, 1, 0, 10);

// Serialize PDU to bytes
uint8_t pdu_buffer[256];
pdu_buffer[0] = request_pdu.function;  // FC
memcpy(&pdu_buffer[1], request_pdu.payload, request_pdu.payload_length);
size_t pdu_length = 1 + request_pdu.payload_length;

// Create MBAP header
mbc_mbap_header_t mbap_header = {
    .transaction_id = 1,      // Increment for each request
    .protocol_id = 0,         // Always 0
    .length = 0,              // Will be auto-calculated
    .unit_id = 1
};

// Encode MBAP frame
uint8_t mbap_frame[260];
size_t frame_length;

mbc_status_t status = mbc_mbap_encode(&mbap_header,
                                       pdu_buffer,
                                       pdu_length,
                                       mbap_frame,
                                       sizeof(mbap_frame),
                                       &frame_length);

if (mbc_status_is_ok(status)) {
    printf("MBAP frame: %zu bytes (7 header + %zu PDU)\n",
           frame_length, pdu_length);
    // Now send mbap_frame via TCP socket
}
```

**Frame Structure:**
```
Bytes 0-1: Transaction ID (big-endian)
Bytes 2-3: Protocol ID = 0x0000
Bytes 4-5: Length = pdu_length + 1 (for unit_id)
Byte 6:    Unit ID
Bytes 7+:  PDU data
```

---

### mbc_mbap_decode()

Decode MBAP frame from input buffer.

**Signature:**
```c
mbc_status_t mbc_mbap_decode(const uint8_t *buffer,
                              size_t length,
                              mbc_mbap_header_t *out_header,
                              const uint8_t **out_pdu,
                              size_t *out_pdu_length);
```

**Parameters:**
- `buffer` – Input buffer containing complete MBAP frame
- `length` – Buffer length in bytes
- `out_header` – (Output) Decoded MBAP header
- `out_pdu` – (Output) Pointer to PDU data within buffer (zero-copy)
- `out_pdu_length` – (Output) PDU length in bytes

**Returns:**
- `MBC_STATUS_OK` on success
- `MBC_STATUS_INVALID_ARGUMENT` if arguments invalid or buffer too short
- `MBC_STATUS_DECODING_ERROR` if protocol ID != 0 or length mismatch

**Example:**
```c
// Receive complete MBAP frame from TCP socket
uint8_t rx_buffer[260];
size_t rx_length = recv(sockfd, rx_buffer, sizeof(rx_buffer), 0);

// Decode MBAP frame
mbc_mbap_header_t header;
const uint8_t *pdu_data;
size_t pdu_length;

mbc_status_t status = mbc_mbap_decode(rx_buffer,
                                       rx_length,
                                       &header,
                                       &pdu_data,
                                       &pdu_length);

if (mbc_status_is_ok(status)) {
    printf("Transaction ID: %u\n", header.transaction_id);
    printf("Unit ID: %u\n", header.unit_id);
    printf("PDU length: %zu\n", pdu_length);

    // Reconstruct PDU structure
    mbc_pdu_t response_pdu;
    response_pdu.unit_id = header.unit_id;
    response_pdu.function = pdu_data[0];
    response_pdu.payload_length = pdu_length - 1;
    memcpy(response_pdu.payload, &pdu_data[1], response_pdu.payload_length);

    // Parse PDU...
}
```

**Note:** `out_pdu` points into the input buffer (zero-copy). Don't free the buffer while using the PDU pointer!

---

### mbc_mbap_expected_length()

Determine expected MBAP frame length from partial buffer.

**Signature:**
```c
size_t mbc_mbap_expected_length(const uint8_t *buffer, size_t length);
```

**Parameters:**
- `buffer` – Partial MBAP frame buffer
- `length` – Current buffer length in bytes

**Returns:**
- Expected total frame length in bytes
- `0` if not enough data yet to determine length (need at least 6 bytes)

**Example:**
```c
// Incremental TCP reception
uint8_t rx_buffer[260];
size_t rx_length = 0;
size_t expected = 0;

while (true) {
    // Receive some bytes
    size_t space = sizeof(rx_buffer) - rx_length;
    ssize_t received = recv(sockfd, &rx_buffer[rx_length], space, 0);

    if (received > 0) {
        rx_length += received;

        // Determine expected frame length
        if (expected == 0) {
            expected = mbc_mbap_expected_length(rx_buffer, rx_length);
            if (expected > 0) {
                printf("Expecting %zu-byte frame\n", expected);
            }
        }

        // Check if we have complete frame
        if (expected > 0 && rx_length >= expected) {
            printf("Complete frame received!\n");
            // Decode MBAP frame...
            break;
        }
    }
}
```

**How It Works:**
```c
// Need at least 6 bytes to read length field
if (length < 6) return 0;

// Extract length field (bytes 4-5, big-endian)
uint16_t mbap_length = (buffer[4] << 8) | buffer[5];

// Total frame = 6 (MBAP header without length field) + mbap_length
return 6 + mbap_length;
```

**Example Frame:**
```
Buffer: [00 01 00 00 00 06 01 ...]
         ^^^^^ ^^^^^ ^^^^^ ^^
         TxID  Proto Len   Unit

Length field = 0x0006 = 6 bytes
Expected total = 6 (header without length) + 6 = 12 bytes
```

---

## Complete Example

### Send FC03 Request

```c
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

void send_fc03_request(int sockfd)
{
    // 1. Build PDU
    mbc_pdu_t request_pdu;
    mbc_pdu_build_read_holding_request(&request_pdu, 1, 0, 10);

    // 2. Serialize PDU
    uint8_t pdu_buffer[256];
    pdu_buffer[0] = request_pdu.function;
    memcpy(&pdu_buffer[1], request_pdu.payload, request_pdu.payload_length);
    size_t pdu_length = 1 + request_pdu.payload_length;

    // 3. Create MBAP header
    static uint16_t transaction_id = 0;
    mbc_mbap_header_t header = {
        .transaction_id = ++transaction_id,  // Increment for each request
        .protocol_id = 0,
        .unit_id = 1
    };

    // 4. Encode MBAP frame
    uint8_t mbap_frame[260];
    size_t frame_length;

    mbc_mbap_encode(&header, pdu_buffer, pdu_length,
                    mbap_frame, sizeof(mbap_frame), &frame_length);

    // 5. Send via TCP
    send(sockfd, mbap_frame, frame_length, 0);

    printf("Sent FC03 request: %zu bytes, TxID=%u\n",
           frame_length, transaction_id);
}
```

### Receive FC03 Response

```c
void receive_fc03_response(int sockfd)
{
    uint8_t rx_buffer[260];
    size_t rx_length = 0;
    size_t expected = 0;

    // Incremental reception
    while (rx_length < sizeof(rx_buffer)) {
        ssize_t received = recv(sockfd, &rx_buffer[rx_length],
                               sizeof(rx_buffer) - rx_length, 0);
        if (received <= 0) break;

        rx_length += received;

        // Determine expected length
        if (expected == 0) {
            expected = mbc_mbap_expected_length(rx_buffer, rx_length);
        }

        // Check if complete
        if (expected > 0 && rx_length >= expected) {
            // Decode MBAP
            mbc_mbap_header_t header;
            const uint8_t *pdu_data;
            size_t pdu_length;

            mbc_mbap_decode(rx_buffer, expected, &header, &pdu_data, &pdu_length);

            printf("Received response: TxID=%u, Unit=%u\n",
                   header.transaction_id, header.unit_id);

            // Reconstruct PDU
            mbc_pdu_t response_pdu;
            response_pdu.unit_id = header.unit_id;
            response_pdu.function = pdu_data[0];
            response_pdu.payload_length = pdu_length - 1;
            memcpy(response_pdu.payload, &pdu_data[1], response_pdu.payload_length);

            // Parse FC03 response
            const uint8_t *data;
            size_t count;
            mbc_pdu_parse_read_holding_response(&response_pdu, &data, &count);

            printf("Read %zu registers\n", count);
            for (size_t i = 0; i < count; ++i) {
                uint16_t value = (data[i * 2] << 8) | data[i * 2 + 1];
                printf("  [%zu] = %u\n", i, value);
            }

            break;
        }
    }
}
```

---

## Best Practices

### ✅ DO

- **Increment transaction ID** for each new request
- **Validate protocol ID** is 0 when decoding
- **Use `mbc_mbap_expected_length()`** for incremental TCP reception
- **Check length field** matches received data

### ❌ DON'T

- **Hardcode length field** – Let `mbc_mbap_encode()` calculate it
- **Free buffer while using PDU pointer** – It points into buffer (zero-copy)
- **Assume complete frames** – TCP is stream-based, use incremental reception
- **Ignore transaction ID** – Use it to match responses with requests

---

## MBAP vs RTU

| Feature | MBAP (TCP) | RTU (Serial) |
|---------|------------|--------------|
| **Framing** | 7-byte header | Unit ID + CRC16 trailer |
| **Transaction ID** | Yes | No |
| **Length Field** | Yes (in header) | No (implicit from timing/CRC) |
| **Error Check** | TCP checksum | CRC16 |
| **Broadcast** | Rare (use unit 0xFF) | Common (unit 0) |
| **Multi-drop** | Multiple TCP connections | Single bus |

---

## Hex Dump Example

### FC03 Request (Read 10 registers at address 0)

```
MBAP Frame (13 bytes):
00 01       Transaction ID = 1
00 00       Protocol ID = 0 (Modbus)
00 06       Length = 6 (1 Unit ID + 5 PDU)
01          Unit ID = 1

03          Function Code = 0x03 (Read Holding)
00 00       Start Address = 0
00 0A       Quantity = 10
```

### FC03 Response (10 registers, all zeros)

```
MBAP Frame (27 bytes):
00 01       Transaction ID = 1
00 00       Protocol ID = 0
00 15       Length = 21 (1 Unit ID + 20 PDU)
01          Unit ID = 1

03          Function Code = 0x03
14          Byte Count = 20 (10 registers * 2 bytes)
00 00       Register 0 = 0
00 01       Register 1 = 1
00 02       Register 2 = 2
... (continues for 10 registers)
```

---

## Related APIs

- [**PDU API**](pdu.md) – Building and parsing PDUs
- [**Engine API**](engine.md) – Automatic MBAP handling
- [**Quick Start**](../quick-start.md) – Complete examples
- [**Architecture**](../architecture.md) – Framing layer details
