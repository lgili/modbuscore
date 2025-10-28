# Architecture

Understand ModbusCore's design principles and internal structure.

## Table of Contents

- [Overview](#overview)
- [Design Principles](#design-principles)
- [Layer Architecture](#layer-architecture)
- [Core Components](#core-components)
- [Data Flow](#data-flow)
- [Memory Management](#memory-management)
- [Thread Safety](#thread-safety)

## Overview

ModbusCore is designed with a **layered architecture** that separates concerns and enables flexibility:

```
┌─────────────────────────────────────────┐
│         Application Layer               │
│   (Your Code: Client/Server Logic)      │
└─────────────────────────────────────────┘
                  ▲ │
                  │ ▼
┌─────────────────────────────────────────┐
│          Runtime Layer                  │
│  • State Management                     │
│  • Transaction Tracking                 │
│  • Timeout Handling                     │
└─────────────────────────────────────────┘
                  ▲ │
                  │ ▼
┌─────────────────────────────────────────┐
│         Protocol Layer                  │
│  • PDU Building/Parsing                 │
│  • MBAP Encoding/Decoding               │
│  • RTU Framing & CRC                    │
└─────────────────────────────────────────┘
                  ▲ │
                  │ ▼
┌─────────────────────────────────────────┐
│         Transport Layer                 │
│  • TCP: Socket Management               │
│  • RTU: Serial I/O                      │
│  • Custom: User-defined                 │
└─────────────────────────────────────────┘
                  ▲ │
                  │ ▼
┌─────────────────────────────────────────┐
│          Platform Layer                 │
│  • OS: POSIX, Windows, FreeRTOS         │
│  • Hardware: UART, Ethernet             │
└─────────────────────────────────────────┘
```

## Design Principles

### 1. **Zero Dependencies**

ModbusCore has **no external dependencies**:
- ✅ Pure C17 standard library only
- ✅ No third-party libraries required
- ✅ Minimal platform requirements

**Why?** Maximum portability and minimal integration effort.

### 2. **Static Memory**

All memory is statically allocated:
```c
// No malloc/free in the library
typedef struct {
    uint8_t function;
    uint8_t data[252];  // Fixed-size buffer
    size_t data_length;
} mbc_pdu_t;
```

**Why?** 
- Deterministic behavior for embedded systems
- No heap fragmentation
- Suitable for safety-critical applications

### 3. **Separation of Concerns**

Each layer has a single responsibility:
- **Protocol Layer**: Frame format only (no I/O)
- **Transport Layer**: I/O only (no protocol logic)
- **Runtime Layer**: State only (no protocol/transport)

**Why?** Easy to test, maintain, and extend.

### 4. **Explicit State**

No hidden global state:
```c
// User owns all state
mbc_runtime_config_t runtime = {0};
mbc_runtime_init(&runtime);

// State is passed explicitly
mbc_runtime_poll(&runtime);
```

**Why?** 
- Multiple independent instances
- Thread-safe when isolated
- Clear ownership

### 5. **Error Handling**

All functions return status codes:
```c
typedef enum {
    MBC_STATUS_OK = 0,
    MBC_STATUS_INVALID_PARAMETER,
    MBC_STATUS_BUFFER_TOO_SMALL,
    MBC_STATUS_CRC_ERROR,
    // ...
} mbc_status_t;

// Always check return values
if (status != MBC_STATUS_OK) {
    // Handle error
}
```

**Why?** Explicit error handling, no exceptions.

## Layer Architecture

### Application Layer

**Your code** that implements business logic:
- Client polling logic
- Server request handlers
- Data mapping (registers ↔ application data)

**Responsibilities:**
- Decision making
- Application state
- User interaction

---

### Runtime Layer

**Files:** `modbus/src/runtime/`

Manages Modbus transactions:
```c
typedef struct {
    mbc_transaction_t transactions[MAX_TRANSACTIONS];
    mbc_timeout_config_t timeout_config;
    uint16_t next_transaction_id;
} mbc_runtime_config_t;
```

**Responsibilities:**
- Transaction tracking (request → response)
- Timeout detection
- Request/response matching
- Queue management

**APIs:**
- `mbc_runtime_init()` - Initialize runtime
- `mbc_runtime_poll()` - Process transactions
- `mbc_runtime_add_transaction()` - Queue request
- `mbc_runtime_timeout_check()` - Check timeouts

---

### Protocol Layer

**Files:** `modbus/src/protocol/`

Handles Modbus frame formats:

#### PDU (Protocol Data Unit)

```c
typedef struct {
    uint8_t function;        // Modbus function code
    uint8_t data[252];       // Function-specific data
    size_t data_length;      // Actual data length
} mbc_pdu_t;
```

**Functions:**
- `mbc_pdu_build_read_holding_request()` - Build read PDU
- `mbc_pdu_build_write_single_request()` - Build write PDU
- `mbc_pdu_parse_response()` - Parse response PDU

#### MBAP (Modbus Application Protocol - TCP)

```c
typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;    // Always 0
    uint16_t length;
    uint8_t unit_id;
} mbc_mbap_header_t;
```

**Functions:**
- `mbc_mbap_encode()` - MBAP header + PDU → frame
- `mbc_mbap_decode()` - Frame → MBAP header + PDU

#### RTU (Remote Terminal Unit - Serial)

**Functions:**
- `mbc_rtu_encode()` - Slave addr + PDU + CRC → frame
- `mbc_rtu_decode()` - Frame → slave addr + PDU (validates CRC)
- `mbc_rtu_calculate_crc()` - CRC-16 calculation

---

### Transport Layer

**Files:** `modbus/src/transport/`

Handles actual I/O:

#### TCP Transport

```c
typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
    bool connected;
} mbc_tcp_transport_t;
```

**Functions:**
- `mbc_tcp_connect()` - Establish TCP connection
- `mbc_tcp_send()` - Send frame
- `mbc_tcp_receive()` - Receive frame
- `mbc_tcp_disconnect()` - Close connection

#### RTU Transport

```c
typedef struct {
    int serial_fd;
    int baudrate;
    char parity;
    int stop_bits;
} mbc_rtu_transport_t;
```

**Functions:**
- `mbc_rtu_open()` - Open serial port
- `mbc_rtu_send()` - Send frame
- `mbc_rtu_receive()` - Receive frame
- `mbc_rtu_close()` - Close serial port

#### Custom Transports

Users can implement custom transports (e.g., UDP, CAN):
```c
typedef struct {
    mbc_status_t (*send)(void *ctx, const uint8_t *data, size_t len);
    mbc_status_t (*receive)(void *ctx, uint8_t *data, size_t max_len, size_t *received);
    void *user_context;
} mbc_custom_transport_t;
```

## Core Components

### 1. PDU Builder

**Purpose:** Create request/response PDUs

**Example:**
```c
mbc_pdu_t pdu = {0};

// Read Holding Registers
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
// Result: function=0x03, data=[00 00 00 0A], data_length=4

// Write Single Register
mbc_pdu_build_write_single_request(&pdu, 1, 5, 1234);
// Result: function=0x06, data=[00 05 04 D2], data_length=4
```

**Supported Functions:**
- 0x01: Read Coils
- 0x02: Read Discrete Inputs
- 0x03: Read Holding Registers
- 0x04: Read Input Registers
- 0x05: Write Single Coil
- 0x06: Write Single Register
- 0x0F: Write Multiple Coils
- 0x10: Write Multiple Registers

### 2. Frame Encoder/Decoder

**Purpose:** Convert PDU ↔ wire format

#### TCP (MBAP)

```c
// Encode: MBAP header + PDU → frame
uint8_t tx_buffer[260];
size_t tx_length;

mbc_mbap_header_t hdr = {
    .transaction_id = 1,
    .protocol_id = 0,
    .unit_id = 1,
    .length = (uint16_t)(pdu.data_length + 1)
};

mbc_mbap_encode(&hdr, &pdu, tx_buffer, sizeof(tx_buffer), &tx_length);
// tx_buffer: [00 01][00 00][00 05][01][03][00 00][00 0A]
//             TxID   Proto  Len    UID Func Data
```

#### RTU

```c
// Encode: Slave addr + PDU + CRC → frame
uint8_t tx_buffer[256];
size_t tx_length;

mbc_rtu_encode(1, &pdu, tx_buffer, sizeof(tx_buffer), &tx_length);
// tx_buffer: [01][03][00 00][00 0A][CRC_L][CRC_H]
//             Slave Func Data      CRC-16
```

### 3. CRC Calculator

**Purpose:** RTU frame integrity

**Implementation:**
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

**Usage:**
```c
uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
uint16_t crc = mbc_rtu_calculate_crc(frame, 6);
// crc = 0xC5CD

// Append to frame (little-endian)
frame[6] = (uint8_t)(crc & 0xFF);        // 0xCD
frame[7] = (uint8_t)((crc >> 8) & 0xFF); // 0xC5
```

### 4. Runtime Engine

**Purpose:** Transaction lifecycle management

```c
typedef struct {
    uint16_t transaction_id;
    mbc_pdu_t request;
    mbc_pdu_t response;
    uint32_t timestamp_ms;
    uint32_t timeout_ms;
    bool pending;
    bool completed;
} mbc_transaction_t;
```

**State Machine:**
```
IDLE → PENDING → WAITING → COMPLETED
        ▲                      │
        └──────── TIMEOUT ─────┘
```

## Data Flow

### Client Request Flow

```
1. Application: Build request PDU
   ↓
2. Runtime: Add transaction, assign TxID
   ↓
3. Protocol: Encode PDU → frame (MBAP/RTU)
   ↓
4. Transport: Send frame
   ↓
5. [Network]
   ↓
6. Transport: Receive frame
   ↓
7. Protocol: Decode frame → MBAP/RTU + PDU
   ↓
8. Runtime: Match response to transaction
   ↓
9. Application: Process response PDU
```

### Server Request Flow

```
1. Transport: Receive frame
   ↓
2. Protocol: Decode frame → MBAP/RTU + PDU
   ↓
3. Application: Process request PDU
   ↓
4. Application: Build response PDU
   ↓
5. Protocol: Encode PDU → frame
   ↓
6. Transport: Send frame
```

## Memory Management

### Stack Allocation

All structures fit on the stack:
```c
void my_function(void) {
    mbc_pdu_t pdu = {0};                    // 256 bytes
    mbc_mbap_header_t hdr = {0};            // 8 bytes
    uint8_t buffer[260];                     // 260 bytes
    // Total: ~524 bytes stack
}
```

### No Dynamic Allocation

Library never calls `malloc()` / `free()`:
- ✅ Suitable for embedded systems
- ✅ No memory leaks
- ✅ Deterministic behavior

### Buffer Sizing

**PDU:** Max 253 bytes (Modbus spec)
```c
#define MBC_PDU_MAX_DATA_LENGTH 252
```

**MBAP Frame:** Max 260 bytes
```c
// MBAP header (7) + function (1) + data (252) = 260
uint8_t buffer[260];
```

**RTU Frame:** Max 256 bytes
```c
// Slave (1) + function (1) + data (252) + CRC (2) = 256
uint8_t buffer[256];
```

## Thread Safety

### Single-Threaded by Default

ModbusCore is **not thread-safe by default**:
- No internal mutexes
- User must synchronize access

### Multi-Threaded Usage

**Option 1:** One runtime per thread (recommended)
```c
// Thread 1
mbc_runtime_config_t runtime1 = {0};
mbc_runtime_init(&runtime1);

// Thread 2
mbc_runtime_config_t runtime2 = {0};
mbc_runtime_init(&runtime2);
```

**Option 2:** Shared runtime with mutex
```c
pthread_mutex_t modbus_mutex;

// Thread 1
pthread_mutex_lock(&modbus_mutex);
mbc_runtime_poll(&shared_runtime);
pthread_mutex_unlock(&modbus_mutex);

// Thread 2
pthread_mutex_lock(&modbus_mutex);
mbc_runtime_poll(&shared_runtime);
pthread_mutex_unlock(&modbus_mutex);
```

### ISR Safety

For interrupt-safe operation:
```c
// Configure ISR-safe mode (when available)
mbc_runtime_config_t runtime = {
    .isr_safe = true  // Disables non-atomic operations
};
```

See [ISR-Safe Mode](../docs/isr_safe_mode.md) for details.

## Next Steps

- [Transports](transports.md) - TCP vs RTU deep dive
- [Protocol Engine](protocol-engine.md) - PDU/MBAP/RTU internals
- [Runtime](runtime.md) - Transaction management

---

**Prev**: [Examples ←](../getting-started/examples.md) | **Next**: [Transports →](transports.md)
