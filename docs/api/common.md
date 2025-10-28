# Common Types

Core types and definitions used throughout ModbusCore.

## Table of Contents

- [Status Codes](#status-codes)
- [PDU Structure](#pdu-structure)
- [MBAP Header](#mbap-header)
- [Constants](#constants)
- [Type Definitions](#type-definitions)

## Status Codes

All ModbusCore functions return `mbc_status_t`:

```c
typedef enum {
    MBC_STATUS_OK = 0,                    ///< Operation successful
    MBC_STATUS_INVALID_PARAMETER,         ///< Invalid function parameter
    MBC_STATUS_BUFFER_TOO_SMALL,          ///< Output buffer insufficient
    MBC_STATUS_INVALID_FUNCTION,          ///< Unsupported function code
    MBC_STATUS_INVALID_DATA_LENGTH,       ///< Invalid data length
    MBC_STATUS_CRC_ERROR,                 ///< CRC mismatch (RTU only)
    MBC_STATUS_TIMEOUT,                   ///< Operation timeout
    MBC_STATUS_IO_ERROR,                  ///< I/O operation failed
    MBC_STATUS_NOT_CONNECTED,             ///< Not connected to device
    MBC_STATUS_ENCODE_ERROR,              ///< Frame encoding failed
    MBC_STATUS_DECODE_ERROR,              ///< Frame decoding failed
} mbc_status_t;
```

### Usage

```c
mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

if (status != MBC_STATUS_OK) {
    fprintf(stderr, "Error: %d\n", status);
    return 1;
}
```

### Status Code Descriptions

| Code | Value | Description | Common Causes |
|------|-------|-------------|---------------|
| `MBC_STATUS_OK` | 0 | Success | - |
| `MBC_STATUS_INVALID_PARAMETER` | 1 | Invalid parameter | NULL pointer, out-of-range value |
| `MBC_STATUS_BUFFER_TOO_SMALL` | 2 | Buffer too small | Insufficient buffer size |
| `MBC_STATUS_INVALID_FUNCTION` | 3 | Invalid function | Unsupported Modbus function code |
| `MBC_STATUS_INVALID_DATA_LENGTH` | 4 | Invalid length | Data exceeds Modbus limits |
| `MBC_STATUS_CRC_ERROR` | 5 | CRC mismatch | Corrupted RTU frame |
| `MBC_STATUS_TIMEOUT` | 6 | Timeout | No response, slow network |
| `MBC_STATUS_IO_ERROR` | 7 | I/O error | Socket/serial error |
| `MBC_STATUS_NOT_CONNECTED` | 8 | Not connected | Connection closed |
| `MBC_STATUS_ENCODE_ERROR` | 9 | Encode failed | Internal error |
| `MBC_STATUS_DECODE_ERROR` | 10 | Decode failed | Malformed frame |

## PDU Structure

Protocol Data Unit - the core Modbus message:

```c
typedef struct {
    uint8_t function;        ///< Modbus function code (0x01-0x7F)
    uint8_t data[252];       ///< Function-specific data
    size_t data_length;      ///< Actual data bytes used (0-252)
} mbc_pdu_t;
```

### Initialization

```c
// Always zero-initialize
mbc_pdu_t pdu = {0};

// Or explicit initialization
mbc_pdu_t pdu = {
    .function = 0x03,
    .data_length = 0
};
```

### Memory Layout

```
┌──────────┬─────────────────────────┬──────────────┐
│ function │        data[252]        │ data_length  │
├──────────┼─────────────────────────┼──────────────┤
│  1 byte  │       252 bytes         │  size_t      │
└──────────┴─────────────────────────┴──────────────┘
```

**Total size:** 256 bytes + sizeof(size_t)

### PDU Limits

```c
#define MBC_PDU_MAX_DATA_LENGTH 252
#define MBC_PDU_MAX_SIZE        253  // 1 function + 252 data
```

## MBAP Header

Modbus Application Protocol header (TCP only):

```c
typedef struct {
    uint16_t transaction_id;  ///< Request/response matching
    uint16_t protocol_id;     ///< Always 0 for Modbus
    uint16_t length;          ///< PDU length + 1 (unit ID)
    uint8_t unit_id;          ///< Target device identifier
} mbc_mbap_header_t;
```

### Initialization

```c
mbc_mbap_header_t hdr = {
    .transaction_id = 1,
    .protocol_id = 0,
    .unit_id = 1,
    .length = 6  // Example: 1 function + 4 data + 1 unit
};
```

### Field Ranges

| Field | Range | Notes |
|-------|-------|-------|
| `transaction_id` | 0-65535 | 0 is valid but avoid for clarity |
| `protocol_id` | 0 only | Always 0 for Modbus TCP |
| `length` | 2-255 | Minimum: unit(1) + function(1) |
| `unit_id` | 0-255 | 0 = broadcast, 255 = reserved |

## Constants

### Function Codes

```c
#define MBC_FUNCTION_READ_COILS                 0x01
#define MBC_FUNCTION_READ_DISCRETE_INPUTS       0x02
#define MBC_FUNCTION_READ_HOLDING_REGISTERS     0x03
#define MBC_FUNCTION_READ_INPUT_REGISTERS       0x04
#define MBC_FUNCTION_WRITE_SINGLE_COIL          0x05
#define MBC_FUNCTION_WRITE_SINGLE_REGISTER      0x06
#define MBC_FUNCTION_WRITE_MULTIPLE_COILS       0x0F
#define MBC_FUNCTION_WRITE_MULTIPLE_REGISTERS   0x10

#define MBC_EXCEPTION_RESPONSE_MASK             0x80
```

### Exception Codes

```c
#define MBC_EXCEPTION_ILLEGAL_FUNCTION          0x01
#define MBC_EXCEPTION_ILLEGAL_DATA_ADDRESS      0x02
#define MBC_EXCEPTION_ILLEGAL_DATA_VALUE        0x03
#define MBC_EXCEPTION_SERVER_DEVICE_FAILURE     0x04
#define MBC_EXCEPTION_ACKNOWLEDGE               0x05
#define MBC_EXCEPTION_SERVER_DEVICE_BUSY        0x06
```

### Limits

```c
// PDU limits
#define MBC_PDU_MAX_DATA_LENGTH                 252
#define MBC_PDU_MAX_SIZE                        253

// Read limits (Modbus spec)
#define MBC_MAX_READ_COILS                      2000
#define MBC_MAX_READ_DISCRETE_INPUTS            2000
#define MBC_MAX_READ_HOLDING_REGISTERS          125
#define MBC_MAX_READ_INPUT_REGISTERS            125

// Write limits (Modbus spec)
#define MBC_MAX_WRITE_COILS                     1968
#define MBC_MAX_WRITE_REGISTERS                 123

// Address limits
#define MBC_MIN_SLAVE_ADDRESS                   1
#define MBC_MAX_SLAVE_ADDRESS                   247
#define MBC_BROADCAST_ADDRESS                   0

// Frame sizes
#define MBC_MBAP_HEADER_SIZE                    7
#define MBC_MBAP_MAX_FRAME_SIZE                 260  // 7 header + 253 PDU
#define MBC_RTU_MAX_FRAME_SIZE                  256  // 1 addr + 253 PDU + 2 CRC
```

### Ports

```c
#define MBC_TCP_DEFAULT_PORT                    502
```

## Type Definitions

### Basic Types

```c
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ModbusCore uses standard C types
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
```

### Endianness

ModbusCore follows Modbus specification (big-endian for multi-byte values):

```c
// Convert host to Modbus byte order (big-endian)
static inline uint16_t mbc_htons(uint16_t hostshort) {
    return ((hostshort & 0xFF00) >> 8) | ((hostshort & 0x00FF) << 8);
}

// Convert Modbus to host byte order
static inline uint16_t mbc_ntohs(uint16_t netshort) {
    return mbc_htons(netshort);  // Same operation
}

// Usage
uint16_t host_value = 1234;
uint16_t modbus_value = mbc_htons(host_value);

// Store in buffer (big-endian)
buffer[0] = (uint8_t)(modbus_value >> 8);
buffer[1] = (uint8_t)(modbus_value & 0xFF);
```

### Boolean Values

For coil values:

```c
#define MBC_COIL_ON    0xFF00
#define MBC_COIL_OFF   0x0000

// Usage
if (coil_value == MBC_COIL_ON) {
    // Coil is ON
}
```

### Function Pointers

```c
// Transport send callback
typedef mbc_status_t (*mbc_send_fn_t)(
    void *ctx, 
    const uint8_t *data, 
    size_t length
);

// Transport receive callback
typedef mbc_status_t (*mbc_receive_fn_t)(
    void *ctx, 
    uint8_t *buffer, 
    size_t max_length, 
    size_t *received
);

// Diagnostic trace callback
typedef void (*mbc_trace_fn_t)(
    bool is_tx, 
    const uint8_t *data, 
    size_t length
);
```

## Helper Macros

### Array Size

```c
#define MBC_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Usage
uint16_t registers[] = {100, 200, 300};
size_t count = MBC_ARRAY_SIZE(registers);  // 3
```

### Min/Max

```c
#define MBC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define MBC_MAX(a, b) ((a) > (b) ? (a) : (b))
```

### Bit Operations

```c
#define MBC_BIT(n)              (1U << (n))
#define MBC_SET_BIT(val, n)     ((val) |= MBC_BIT(n))
#define MBC_CLEAR_BIT(val, n)   ((val) &= ~MBC_BIT(n))
#define MBC_TOGGLE_BIT(val, n)  ((val) ^= MBC_BIT(n))
#define MBC_CHECK_BIT(val, n)   (((val) & MBC_BIT(n)) != 0)
```

## Compiler Attributes

### Packed Structures

```c
#if defined(__GNUC__) || defined(__clang__)
    #define MBC_PACKED __attribute__((packed))
#elif defined(_MSC_VER)
    #define MBC_PACKED
    #pragma pack(push, 1)
#else
    #define MBC_PACKED
#endif

// Usage (if needed)
typedef struct MBC_PACKED {
    uint8_t field1;
    uint16_t field2;
} my_struct_t;
```

### Unused Parameters

```c
#define MBC_UNUSED(x) (void)(x)

// Usage
void my_function(int used_param, int unused_param) {
    MBC_UNUSED(unused_param);
    // Use used_param...
}
```

## Platform Differences

### Windows vs POSIX

```c
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #define MBC_SOCKET SOCKET
    #define MBC_INVALID_SOCKET INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <unistd.h>
    #define MBC_SOCKET int
    #define MBC_INVALID_SOCKET -1
#endif
```

### Sleep Functions

```c
#ifdef _WIN32
    #define mbc_sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define mbc_sleep_ms(ms) usleep((ms) * 1000)
#endif
```

## Example Usage

### Complete Type Usage

```c
#include <modbuscore/common.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

int main(void) {
    mbc_status_t status;
    
    // PDU
    mbc_pdu_t pdu = {0};
    status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    if (status != MBC_STATUS_OK) {
        return 1;
    }
    
    // MBAP header
    mbc_mbap_header_t hdr = {
        .transaction_id = 1,
        .protocol_id = 0,
        .unit_id = 1,
        .length = (uint16_t)(pdu.data_length + 1)
    };
    
    // Encode
    uint8_t buffer[MBC_MBAP_MAX_FRAME_SIZE];
    size_t length;
    
    status = mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &length);
    if (status != MBC_STATUS_OK) {
        return 1;
    }
    
    // Send buffer...
    
    return 0;
}
```

## Next Steps

- [Protocol API](protocol.md) - PDU/MBAP/RTU functions
- [Transport API](transport.md) - I/O operations
- [Diagnostics API](diagnostics.md) - Logging and tracing

---

**Prev**: [API Reference ←](README.md) | **Next**: [Protocol API →](protocol.md)
