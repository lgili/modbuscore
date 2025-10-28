# Protocol API

Complete reference for PDU, MBAP, and RTU protocol functions.

## Table of Contents

- [PDU Functions](#pdu-functions)
- [MBAP Functions](#mbap-functions)
- [RTU Functions](#rtu-functions)
- [CRC Functions](#crc-functions)

## PDU Functions

### mbc_pdu_build_read_holding_request

Build a Read Holding Registers (0x03) request PDU.

```c
mbc_status_t mbc_pdu_build_read_holding_request(
    mbc_pdu_t *pdu,
    uint8_t slave_addr,
    uint16_t start_addr,
    uint16_t count
);
```

**Parameters:**
- `pdu` - Output PDU structure
- `slave_addr` - Modbus slave address (1-247, for context only)
- `start_addr` - Starting register address (0-65535)
- `count` - Number of registers to read (1-125)

**Returns:**
- `MBC_STATUS_OK` - Success
- `MBC_STATUS_INVALID_PARAMETER` - Invalid parameters

**Example:**
```c
mbc_pdu_t pdu = {0};
if (mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10) == MBC_STATUS_OK) {
    // PDU ready: function=0x03, data=[00 00 00 0A]
}
```

---

### mbc_pdu_build_read_input_request

Build a Read Input Registers (0x04) request PDU.

```c
mbc_status_t mbc_pdu_build_read_input_request(
    mbc_pdu_t *pdu,
    uint8_t slave_addr,
    uint16_t start_addr,
    uint16_t count
);
```

**Parameters:** Same as `mbc_pdu_build_read_holding_request`

**Limits:** Maximum 125 registers per request

---

### mbc_pdu_build_read_coils_request

Build a Read Coils (0x01) request PDU.

```c
mbc_status_t mbc_pdu_build_read_coils_request(
    mbc_pdu_t *pdu,
    uint8_t slave_addr,
    uint16_t start_addr,
    uint16_t count
);
```

**Parameters:**
- `count` - Number of coils to read (1-2000)

**Limits:** Maximum 2000 coils per request

---

### mbc_pdu_build_read_discrete_request

Build a Read Discrete Inputs (0x02) request PDU.

```c
mbc_status_t mbc_pdu_build_read_discrete_request(
    mbc_pdu_t *pdu,
    uint8_t slave_addr,
    uint16_t start_addr,
    uint16_t count
);
```

**Limits:** Maximum 2000 discrete inputs per request

---

### mbc_pdu_build_write_single_request

Build a Write Single Register (0x06) request PDU.

```c
mbc_status_t mbc_pdu_build_write_single_request(
    mbc_pdu_t *pdu,
    uint8_t slave_addr,
    uint16_t reg_addr,
    uint16_t value
);
```

**Parameters:**
- `reg_addr` - Register address to write
- `value` - 16-bit value to write

**Example:**
```c
mbc_pdu_t pdu = {0};
mbc_pdu_build_write_single_request(&pdu, 1, 5, 1234);
// PDU: function=0x06, data=[00 05 04 D2]
```

---

### mbc_pdu_build_write_single_coil_request

Build a Write Single Coil (0x05) request PDU.

```c
mbc_status_t mbc_pdu_build_write_single_coil_request(
    mbc_pdu_t *pdu,
    uint8_t slave_addr,
    uint16_t coil_addr,
    bool value
);
```

**Parameters:**
- `value` - true = ON (0xFF00), false = OFF (0x0000)

---

### mbc_pdu_build_write_multiple_request

Build a Write Multiple Registers (0x10) request PDU.

```c
mbc_status_t mbc_pdu_build_write_multiple_request(
    mbc_pdu_t *pdu,
    uint8_t slave_addr,
    uint16_t start_addr,
    const uint16_t *values,
    uint16_t count
);
```

**Parameters:**
- `values` - Array of register values
- `count` - Number of registers (1-123)

**Limits:** Maximum 123 registers per request

**Example:**
```c
uint16_t values[] = {100, 200, 300};
mbc_pdu_t pdu = {0};
mbc_pdu_build_write_multiple_request(&pdu, 1, 0, values, 3);
```

---

### mbc_pdu_build_write_multiple_coils_request

Build a Write Multiple Coils (0x0F) request PDU.

```c
mbc_status_t mbc_pdu_build_write_multiple_coils_request(
    mbc_pdu_t *pdu,
    uint8_t slave_addr,
    uint16_t start_addr,
    const uint8_t *coil_values,
    uint16_t count
);
```

**Parameters:**
- `coil_values` - Packed bits (LSB first)
- `count` - Number of coils (1-1968)

**Limits:** Maximum 1968 coils per request

---

### mbc_pdu_parse_read_response

Parse a read response PDU.

```c
mbc_status_t mbc_pdu_parse_read_response(
    const mbc_pdu_t *pdu,
    void *output,
    size_t output_size,
    size_t *items_read
);
```

**Parameters:**
- `output` - Buffer for parsed data
- `output_size` - Size of output buffer
- `items_read` - Number of items successfully parsed

**Example:**
```c
uint16_t registers[10];
size_t count;
if (mbc_pdu_parse_read_response(&resp_pdu, registers, sizeof(registers), &count) == MBC_STATUS_OK) {
    printf("Read %zu registers\n", count);
}
```

---

## MBAP Functions

### mbc_mbap_encode

Encode MBAP frame (TCP).

```c
mbc_status_t mbc_mbap_encode(
    const mbc_mbap_header_t *header,
    const mbc_pdu_t *pdu,
    uint8_t *output,
    size_t output_size,
    size_t *frame_length
);
```

**Parameters:**
- `header` - MBAP header to encode
- `pdu` - PDU to encode
- `output` - Output buffer (minimum 260 bytes)
- `output_size` - Size of output buffer
- `frame_length` - Actual encoded frame length

**Returns:**
- `MBC_STATUS_OK` - Success
- `MBC_STATUS_BUFFER_TOO_SMALL` - Buffer too small

**Frame Format:**
```
[TxID(2)][Proto(2)][Len(2)][Unit(1)][PDU(N)]
```

**Example:**
```c
mbc_pdu_t pdu = {0};
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

mbc_mbap_header_t hdr = {
    .transaction_id = 1,
    .protocol_id = 0,
    .unit_id = 1,
    .length = (uint16_t)(pdu.data_length + 1)
};

uint8_t buffer[260];
size_t length;
mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &length);
// length = 12 (7 header + 5 PDU)
```

---

### mbc_mbap_decode

Decode MBAP frame (TCP).

```c
mbc_status_t mbc_mbap_decode(
    const uint8_t *frame,
    size_t frame_length,
    mbc_mbap_header_t *header,
    mbc_pdu_t *pdu
);
```

**Parameters:**
- `frame` - Input frame buffer
- `frame_length` - Frame length in bytes
- `header` - Output MBAP header
- `pdu` - Output PDU

**Returns:**
- `MBC_STATUS_OK` - Success
- `MBC_STATUS_DECODE_ERROR` - Invalid frame

**Example:**
```c
uint8_t rx_buffer[260];
ssize_t received = recv(sock, rx_buffer, sizeof(rx_buffer), 0);

mbc_mbap_header_t hdr;
mbc_pdu_t pdu;

if (mbc_mbap_decode(rx_buffer, (size_t)received, &hdr, &pdu) == MBC_STATUS_OK) {
    printf("TxID: %u, Function: 0x%02X\n", hdr.transaction_id, pdu.function);
}
```

---

### mbc_mbap_validate_header

Validate MBAP header fields.

```c
mbc_status_t mbc_mbap_validate_header(const mbc_mbap_header_t *header);
```

**Checks:**
- Protocol ID must be 0
- Length must be >= 2
- Unit ID in valid range

---

## RTU Functions

### mbc_rtu_encode

Encode RTU frame (Serial).

```c
mbc_status_t mbc_rtu_encode(
    uint8_t slave_addr,
    const mbc_pdu_t *pdu,
    uint8_t *output,
    size_t output_size,
    size_t *frame_length
);
```

**Parameters:**
- `slave_addr` - Modbus slave address (1-247, 0=broadcast)
- `pdu` - PDU to encode
- `output` - Output buffer (minimum 256 bytes)
- `output_size` - Size of output buffer
- `frame_length` - Actual encoded frame length

**Frame Format:**
```
[Slave(1)][PDU(N)][CRC_L(1)][CRC_H(1)]
```

**Example:**
```c
mbc_pdu_t pdu = {0};
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

uint8_t buffer[256];
size_t length;
mbc_rtu_encode(1, &pdu, buffer, sizeof(buffer), &length);
// length = 8 (1 slave + 5 PDU + 2 CRC)
// buffer: 01 03 00 00 00 0A C5 CD
```

---

### mbc_rtu_decode

Decode RTU frame (Serial).

```c
mbc_status_t mbc_rtu_decode(
    const uint8_t *frame,
    size_t frame_length,
    uint8_t *slave_addr,
    mbc_pdu_t *pdu
);
```

**Parameters:**
- `frame` - Input frame buffer
- `frame_length` - Frame length in bytes
- `slave_addr` - Output slave address
- `pdu` - Output PDU

**Returns:**
- `MBC_STATUS_OK` - Success, CRC valid
- `MBC_STATUS_CRC_ERROR` - CRC mismatch
- `MBC_STATUS_DECODE_ERROR` - Invalid frame

**Example:**
```c
uint8_t rx_buffer[256];
ssize_t received = read(serial_fd, rx_buffer, sizeof(rx_buffer));

uint8_t slave;
mbc_pdu_t pdu;

mbc_status_t status = mbc_rtu_decode(rx_buffer, (size_t)received, &slave, &pdu);
if (status == MBC_STATUS_OK) {
    printf("Slave: %u, Function: 0x%02X\n", slave, pdu.function);
} else if (status == MBC_STATUS_CRC_ERROR) {
    printf("CRC error - corrupted frame\n");
}
```

---

## CRC Functions

### mbc_rtu_calculate_crc

Calculate CRC-16 for RTU frame.

```c
uint16_t mbc_rtu_calculate_crc(const uint8_t *data, size_t length);
```

**Parameters:**
- `data` - Data to calculate CRC for
- `length` - Data length in bytes

**Returns:** CRC-16 value (Modbus polynomial 0xA001)

**Algorithm:**
- Initial value: 0xFFFF
- Polynomial: 0xA001 (reversed)
- Final XOR: None

**Example:**
```c
uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
uint16_t crc = mbc_rtu_calculate_crc(frame, 6);
// crc = 0xC5CD

// Append to frame (little-endian)
frame[6] = (uint8_t)(crc & 0xFF);        // 0xCD
frame[7] = (uint8_t)((crc >> 8) & 0xFF); // 0xC5
```

---

### mbc_rtu_validate_crc

Validate CRC of RTU frame.

```c
bool mbc_rtu_validate_crc(const uint8_t *frame, size_t frame_length);
```

**Parameters:**
- `frame` - Complete RTU frame including CRC
- `frame_length` - Total frame length

**Returns:**
- `true` - CRC valid
- `false` - CRC invalid

**Example:**
```c
uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xCD, 0xC5};
if (mbc_rtu_validate_crc(frame, 8)) {
    printf("✓ CRC valid\n");
} else {
    printf("✗ CRC error\n");
}
```

---

## Error Handling

All functions return `mbc_status_t`. Always check return values:

```c
mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

switch (status) {
    case MBC_STATUS_OK:
        // Success
        break;
    
    case MBC_STATUS_INVALID_PARAMETER:
        fprintf(stderr, "Invalid parameter\n");
        break;
    
    case MBC_STATUS_BUFFER_TOO_SMALL:
        fprintf(stderr, "Buffer too small\n");
        break;
    
    default:
        fprintf(stderr, "Error: %d\n", status);
        break;
}
```

## Thread Safety

Protocol functions are **stateless and thread-safe** when used with separate buffers:

```c
// Thread 1
mbc_pdu_t pdu1 = {0};
mbc_pdu_build_read_holding_request(&pdu1, 1, 0, 10);

// Thread 2
mbc_pdu_t pdu2 = {0};
mbc_pdu_build_read_holding_request(&pdu2, 1, 10, 10);
```

## Performance

Typical execution times (ARM Cortex-M4 @ 80MHz):

| Function | Cycles | Time |
|----------|--------|------|
| `mbc_pdu_build_*` | ~100 | 1.25 µs |
| `mbc_mbap_encode` | ~200 | 2.5 µs |
| `mbc_mbap_decode` | ~200 | 2.5 µs |
| `mbc_rtu_encode` | ~500 | 6.25 µs |
| `mbc_rtu_decode` | ~500 | 6.25 µs |
| `mbc_rtu_calculate_crc` | ~400 | 5 µs |

## Next Steps

- [Transport API](transport.md) - I/O operations
- [Diagnostics API](diagnostics.md) - Logging and debugging
- [Common Types](common.md) - Data structures

---

**Prev**: [Common Types ←](common.md) | **Next**: [Diagnostics API →](diagnostics.md)
