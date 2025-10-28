# Protocol Engine

Deep dive into ModbusCore protocol implementation: PDU, MBAP, and RTU.

## Table of Contents

- [Overview](#overview)
- [PDU Layer](#pdu-layer)
- [MBAP Layer](#mbap-layer)
- [RTU Layer](#rtu-layer)
- [Function Codes](#function-codes)
- [Exception Handling](#exception-handling)

## Overview

ModbusCore implements the Modbus protocol in three layers:

```
┌─────────────────────────────────────┐
│         Application Data            │  Your business logic
└─────────────────────────────────────┘
                 ▲ │
                 │ ▼
┌─────────────────────────────────────┐
│      PDU (Protocol Data Unit)       │  Function + Data
│         Function Code: 1 byte       │
│         Data: 0-252 bytes           │
└─────────────────────────────────────┘
                 ▲ │
                 │ ▼
         ┌───────┴──────┐
         │              │
         ▼              ▼
┌──────────────┐ ┌──────────────┐
│  MBAP (TCP)  │ │   RTU (Serial)│
│  7-byte hdr  │ │  Addr + CRC   │
└──────────────┘ └──────────────┘
```

## PDU Layer

### PDU Structure

```c
typedef struct {
    uint8_t function;        // Modbus function code (0x01-0x7F)
    uint8_t data[252];       // Function-specific data
    size_t data_length;      // Actual data bytes used
} mbc_pdu_t;
```

**Maximum size:** 253 bytes total (1 function + 252 data)

### Building PDUs

#### Read Holding Registers (0x03)

```c
mbc_pdu_t pdu = {0};
mbc_status_t status = mbc_pdu_build_read_holding_request(
    &pdu,
    1,      // slave_addr (for context, not encoded in PDU)
    0,      // start_address
    10      // count
);

// Result:
// pdu.function = 0x03
// pdu.data[0-1] = 0x0000  (start address, big-endian)
// pdu.data[2-3] = 0x000A  (count, big-endian)
// pdu.data_length = 4
```

**PDU bytes:**
```
03 00 00 00 0A
│  │     │
│  │     └─ Count: 10 registers
│  └─────── Start address: 0
└────────── Function: Read Holding (0x03)
```

#### Write Single Register (0x06)

```c
mbc_pdu_t pdu = {0};
mbc_pdu_build_write_single_request(
    &pdu,
    1,      // slave_addr
    5,      // register_address
    1234    // value
);

// Result:
// pdu.function = 0x06
// pdu.data[0-1] = 0x0005  (address)
// pdu.data[2-3] = 0x04D2  (value = 1234)
// pdu.data_length = 4
```

**PDU bytes:**
```
06 00 05 04 D2
│  │     │
│  │     └─ Value: 1234 (0x04D2)
│  └─────── Address: 5
└────────── Function: Write Single (0x06)
```

#### Write Multiple Registers (0x10)

```c
uint16_t values[] = {100, 200, 300};
mbc_pdu_t pdu = {0};
mbc_pdu_build_write_multiple_request(
    &pdu,
    1,       // slave_addr
    0,       // start_address
    values,  // data
    3        // count
);

// Result:
// pdu.function = 0x10
// pdu.data[0-1] = 0x0000  (start address)
// pdu.data[2-3] = 0x0003  (count)
// pdu.data[4] = 0x06      (byte count = 3*2)
// pdu.data[5-10] = values (big-endian)
// pdu.data_length = 11
```

**PDU bytes:**
```
10 00 00 00 03 06 00 64 00 C8 01 2C
│  │     │     │  │           │
│  │     │     │  │           └─ Value[2]: 300 (0x012C)
│  │     │     │  └─────────── Value[0]: 100 (0x0064)
│  │     │     └────────────── Byte count: 6
│  │     └──────────────────── Count: 3
│  └────────────────────────── Start address: 0
└───────────────────────────── Function: Write Multiple (0x10)
```

### Parsing PDU Responses

#### Read Response

```c
// Received PDU: 03 06 00 64 00 C8 01 2C
// (Read 3 holding registers response)

mbc_pdu_t resp_pdu = {0};
// After decoding from MBAP/RTU...

if (resp_pdu.function == 0x03) {
    uint8_t byte_count = resp_pdu.data[0];  // 6 (3 registers * 2 bytes)
    uint16_t num_registers = byte_count / 2;
    
    for (uint16_t i = 0; i < num_registers; i++) {
        uint16_t value = ((uint16_t)resp_pdu.data[1 + i*2] << 8) |
                         resp_pdu.data[2 + i*2];
        printf("Register %u: %u\n", i, value);
    }
}
```

**Output:**
```
Register 0: 100
Register 1: 200
Register 2: 300
```

#### Exception Response

```c
// Received PDU: 83 02
// (Exception: Illegal data address)

if (resp_pdu.function & 0x80) {
    // Exception response
    uint8_t exception_code = resp_pdu.data[0];
    
    switch (exception_code) {
        case 0x01: printf("Illegal function\n"); break;
        case 0x02: printf("Illegal data address\n"); break;
        case 0x03: printf("Illegal data value\n"); break;
        case 0x04: printf("Server device failure\n"); break;
        default: printf("Unknown exception: 0x%02X\n", exception_code);
    }
}
```

## MBAP Layer

### MBAP Header Structure

```c
typedef struct {
    uint16_t transaction_id;  // Matches request to response
    uint16_t protocol_id;     // Always 0 for Modbus
    uint16_t length;          // PDU length + 1 (for unit ID)
    uint8_t unit_id;          // Target device (0-255)
} mbc_mbap_header_t;
```

**Total:** 7 bytes header + PDU

### MBAP Frame Format

```
Byte:  0  1  2  3  4  5  6  7  8  9 10 11
      ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
      │ Transaction │Protocol│Length │U │PDU ...
      │     ID      │   ID   │       │I │
      │             │        │       │D │
      └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘
       ←─ 2 bytes ─→  2 bytes  2 b  1  ← varies →
```

### Encoding MBAP

```c
mbc_pdu_t pdu = {0};
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

mbc_mbap_header_t hdr = {
    .transaction_id = 1,
    .protocol_id = 0,
    .unit_id = 1,
    .length = (uint16_t)(pdu.data_length + 1)  // PDU + 1 for function
};

uint8_t tx_buffer[260];
size_t tx_length;

mbc_status_t status = mbc_mbap_encode(
    &hdr,
    &pdu,
    tx_buffer,
    sizeof(tx_buffer),
    &tx_length
);

// tx_buffer: 00 01 00 00 00 06 01 03 00 00 00 0A
// tx_length: 12
```

**Frame breakdown:**
```
00 01 - Transaction ID: 1
00 00 - Protocol ID: 0 (Modbus)
00 06 - Length: 6 (1 function + 4 data + 1 unit)
01    - Unit ID: 1
03    - Function: Read Holding
00 00 - Start address: 0
00 0A - Count: 10
```

### Decoding MBAP

```c
uint8_t rx_buffer[260];
ssize_t received = recv(sock, rx_buffer, sizeof(rx_buffer), 0);

mbc_mbap_header_t resp_hdr;
mbc_pdu_t resp_pdu;

if (mbc_mbap_decode(rx_buffer, (size_t)received, &resp_hdr, &resp_pdu) == MBC_STATUS_OK) {
    printf("Transaction ID: %u\n", resp_hdr.transaction_id);
    printf("Unit ID: %u\n", resp_hdr.unit_id);
    printf("Function: 0x%02X\n", resp_pdu.function);
    // Process resp_pdu...
}
```

### Transaction ID Management

```c
static uint16_t next_txid = 1;

uint16_t get_next_transaction_id(void) {
    uint16_t txid = next_txid++;
    if (next_txid == 0) {
        next_txid = 1;  // Skip 0
    }
    return txid;
}

// Usage
mbc_mbap_header_t hdr = {
    .transaction_id = get_next_transaction_id(),
    // ...
};
```

## RTU Layer

### RTU Frame Structure

```
┌────────┬──────────┬─────────┬──────────┐
│ Slave  │ Function │  Data   │ CRC-16   │
│  ID    │   Code   │         │          │
├────────┼──────────┼─────────┼──────────┤
│ 1 byte │  1 byte  │ N bytes │ 2 bytes  │
└────────┴──────────┴─────────┴──────────┘
```

### Encoding RTU

```c
mbc_pdu_t pdu = {0};
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

uint8_t tx_buffer[256];
size_t tx_length;

mbc_status_t status = mbc_rtu_encode(
    1,              // slave_addr
    &pdu,
    tx_buffer,
    sizeof(tx_buffer),
    &tx_length
);

// tx_buffer: 01 03 00 00 00 0A C5 CD
// tx_length: 8
```

**Frame breakdown:**
```
01    - Slave ID: 1
03    - Function: Read Holding
00 00 - Start address: 0
00 0A - Count: 10
C5 CD - CRC-16 (little-endian)
```

### CRC-16 Calculation

ModbusCore uses CRC-16 with polynomial 0xA001:

```c
uint16_t mbc_rtu_calculate_crc(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}
```

**Example:**
```c
uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
uint16_t crc = mbc_rtu_calculate_crc(frame, 6);
// crc = 0xC5CD

// Append to frame (little-endian!)
frame[6] = (uint8_t)(crc & 0xFF);        // 0xCD
frame[7] = (uint8_t)((crc >> 8) & 0xFF); // 0xC5
```

### Decoding RTU

```c
uint8_t rx_buffer[256];
ssize_t received = read(serial_fd, rx_buffer, sizeof(rx_buffer));

uint8_t slave_addr;
mbc_pdu_t resp_pdu;

mbc_status_t status = mbc_rtu_decode(
    rx_buffer,
    (size_t)received,
    &slave_addr,
    &resp_pdu
);

if (status == MBC_STATUS_OK) {
    printf("Slave: %u\n", slave_addr);
    printf("Function: 0x%02X\n", resp_pdu.function);
    // Process resp_pdu...
} else if (status == MBC_STATUS_CRC_ERROR) {
    printf("CRC mismatch! Data corrupted.\n");
}
```

### CRC Validation

```c
// Manual CRC check
uint16_t received_crc = ((uint16_t)rx_buffer[len-1] << 8) | rx_buffer[len-2];
uint16_t calculated_crc = mbc_rtu_calculate_crc(rx_buffer, len - 2);

if (received_crc == calculated_crc) {
    printf("✓ CRC valid\n");
} else {
    printf("✗ CRC error: expected 0x%04X, got 0x%04X\n", 
           calculated_crc, received_crc);
}
```

## Function Codes

### Supported Functions

| Code | Name | Request Data | Response Data |
|------|------|--------------|---------------|
| 0x01 | Read Coils | Start addr (2) + Count (2) | Byte count (1) + Coil status (N) |
| 0x02 | Read Discrete | Start addr (2) + Count (2) | Byte count (1) + Input status (N) |
| 0x03 | Read Holding | Start addr (2) + Count (2) | Byte count (1) + Register values (N*2) |
| 0x04 | Read Input | Start addr (2) + Count (2) | Byte count (1) + Register values (N*2) |
| 0x05 | Write Coil | Addr (2) + Value (2) | Addr (2) + Value (2) |
| 0x06 | Write Register | Addr (2) + Value (2) | Addr (2) + Value (2) |
| 0x0F | Write Coils | Addr (2) + Count (2) + Bytes (1) + Values (N) | Addr (2) + Count (2) |
| 0x10 | Write Registers | Addr (2) + Count (2) + Bytes (1) + Values (N*2) | Addr (2) + Count (2) |

### Data Limits

| Function | Max Count | Max Data Size |
|----------|-----------|---------------|
| Read Coils (0x01) | 2000 | 250 bytes |
| Read Discrete (0x02) | 2000 | 250 bytes |
| Read Holding (0x03) | 125 | 250 bytes |
| Read Input (0x04) | 125 | 250 bytes |
| Write Coils (0x0F) | 1968 | 246 bytes |
| Write Registers (0x10) | 123 | 246 bytes |

## Exception Handling

### Exception Response Format

```
┌──────────────┬──────────────────┐
│ Function     │ Exception Code   │
│ (original + 0x80) │              │
├──────────────┼──────────────────┤
│   1 byte     │     1 byte       │
└──────────────┴──────────────────┘
```

### Exception Codes

```c
#define MBC_EXCEPTION_ILLEGAL_FUNCTION        0x01
#define MBC_EXCEPTION_ILLEGAL_DATA_ADDRESS    0x02
#define MBC_EXCEPTION_ILLEGAL_DATA_VALUE      0x03
#define MBC_EXCEPTION_SERVER_DEVICE_FAILURE   0x04
#define MBC_EXCEPTION_ACKNOWLEDGE             0x05
#define MBC_EXCEPTION_SERVER_DEVICE_BUSY      0x06
```

### Detecting Exceptions

```c
bool is_exception_response(const mbc_pdu_t *pdu) {
    return (pdu->function & 0x80) != 0;
}

uint8_t get_exception_code(const mbc_pdu_t *pdu) {
    return pdu->data[0];
}

// Usage
if (is_exception_response(&resp_pdu)) {
    uint8_t code = get_exception_code(&resp_pdu);
    printf("Exception 0x%02X: ", code);
    
    switch (code) {
        case 0x01: printf("Illegal function\n"); break;
        case 0x02: printf("Illegal data address\n"); break;
        case 0x03: printf("Illegal data value\n"); break;
        default: printf("Unknown\n");
    }
}
```

### Building Exception Responses (Server)

```c
void build_exception_response(mbc_pdu_t *resp, uint8_t function, uint8_t code) {
    resp->function = function | 0x80;
    resp->data[0] = code;
    resp->data_length = 1;
}

// Usage
mbc_pdu_t resp = {0};
if (address_out_of_range) {
    build_exception_response(&resp, 0x03, 0x02);  // Illegal address
}
```

## Next Steps

- [Runtime](runtime.md) - Transaction management
- [Auto-Heal](auto-heal.md) - Error recovery
- [Diagnostics](diagnostics.md) - Logging and debugging

---

**Prev**: [Transports ←](transports.md) | **Next**: [Runtime →](runtime.md)
