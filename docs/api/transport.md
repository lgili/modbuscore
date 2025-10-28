# Transport API

Complete reference for ModbusCore transport layer interfaces and platform-specific drivers.

## Table of Contents

- [Overview](#overview)
- [Transport Interface](#transport-interface)
- [TCP Transport](#tcp-transport)
  - [POSIX TCP](#posix-tcp)
  - [Winsock TCP](#winsock-tcp)
- [RTU Transport](#rtu-transport)
  - [POSIX RTU](#posix-rtu)
  - [Windows RTU](#windows-rtu)
- [Mock Transport](#mock-transport)
- [Custom Transports](#custom-transports)

## Overview

ModbusCore uses a **transport abstraction layer** that allows the protocol engine to work with different physical media (TCP, serial RTU, mocks) without modification.

**Key Features:**
- Non-blocking I/O for cooperative multitasking
- Platform-specific drivers (POSIX, Windows)
- Plug-and-play transport switching
- Mock transports for testing

**Headers:**
- Base interface: `<modbuscore/transport/iface.h>`
- POSIX TCP: `<modbuscore/transport/posix_tcp.h>`
- Winsock TCP: `<modbuscore/transport/winsock_tcp.h>`
- POSIX RTU: `<modbuscore/transport/posix_rtu.h>`
- Windows RTU: `<modbuscore/transport/win32_rtu.h>`
- Mock: `<modbuscore/transport/mock.h>`

---

## Transport Interface

The base transport interface provides a uniform API for all transports.

### mbc_transport_iface_t

Transport abstraction structure:

```c
typedef struct mbc_transport_iface {
    void *ctx;  // Driver-specific context
    
    // Send data
    mbc_status_t (*send)(void *ctx, const uint8_t *buffer, size_t length,
                         mbc_transport_io_t *out);
    
    // Receive data
    mbc_status_t (*receive)(void *ctx, uint8_t *buffer, size_t capacity,
                            mbc_transport_io_t *out);
    
    // Get current timestamp (milliseconds)
    uint64_t (*now)(void *ctx);
    
    // Cooperative yield (optional)
    void (*yield)(void *ctx);
} mbc_transport_iface_t;
```

---

### mbc_transport_send

Send data through transport.

```c
mbc_status_t mbc_transport_send(
    const mbc_transport_iface_t *iface,
    const uint8_t *buffer,
    size_t length,
    mbc_transport_io_t *out
);
```

**Parameters:**
- `iface` - Transport interface
- `buffer` - Data to send
- `length` - Number of bytes to send
- `out` - I/O result (can be NULL)

**Returns:**
- `MBC_STATUS_OK` - All data sent
- `MBC_STATUS_TRANSPORT_ERROR` - Send failed
- `MBC_STATUS_WOULD_BLOCK` - Non-blocking socket would block

**Example:**
```c
uint8_t tx_buffer[260];
size_t tx_length;

mbc_mbap_encode(&header, &pdu, tx_buffer, sizeof(tx_buffer), &tx_length);

mbc_transport_io_t io;
mbc_status_t status = mbc_transport_send(&iface, tx_buffer, tx_length, &io);

if (status == MBC_STATUS_OK) {
    printf("Sent %zu bytes\n", io.processed);
}
```

---

### mbc_transport_receive

Receive data from transport.

```c
mbc_status_t mbc_transport_receive(
    const mbc_transport_iface_t *iface,
    uint8_t *buffer,
    size_t capacity,
    mbc_transport_io_t *out
);
```

**Parameters:**
- `iface` - Transport interface
- `buffer` - Buffer to receive into
- `capacity` - Buffer size
- `out` - I/O result (can be NULL)

**Returns:**
- `MBC_STATUS_OK` - Data received
- `MBC_STATUS_WOULD_BLOCK` - No data available (non-blocking)
- `MBC_STATUS_TRANSPORT_ERROR` - Connection error

**Example:**
```c
uint8_t rx_buffer[260];
mbc_transport_io_t io;

mbc_status_t status = mbc_transport_receive(&iface, rx_buffer, sizeof(rx_buffer), &io);

if (status == MBC_STATUS_OK) {
    printf("Received %zu bytes\n", io.processed);
    // Process response...
}
```

---

### mbc_transport_now

Get current timestamp from transport.

```c
uint64_t mbc_transport_now(const mbc_transport_iface_t *iface);
```

**Returns:** Current time in milliseconds

**Example:**
```c
uint64_t start = mbc_transport_now(&iface);
// ... perform operation ...
uint64_t elapsed = mbc_transport_now(&iface) - start;
printf("Operation took %llu ms\n", elapsed);
```

---

### mbc_transport_yield

Cooperative yield (optional).

```c
void mbc_transport_yield(const mbc_transport_iface_t *iface);
```

Allows transport to yield control in cooperative multitasking environments (FreeRTOS, etc.).

---

## TCP Transport

### POSIX TCP

**Platform:** Linux, macOS, BSD, Unix-like systems  
**Header:** `<modbuscore/transport/posix_tcp.h>`

#### mbc_posix_tcp_config_t

TCP configuration structure:

```c
typedef struct mbc_posix_tcp_config {
    const char *host;             // Hostname or IP address
    uint16_t port;                // TCP port (default 502)
    uint32_t connect_timeout_ms;  // Connection timeout (0 = unlimited)
    uint32_t recv_timeout_ms;     // I/O timeout (0 = no timeout)
} mbc_posix_tcp_config_t;
```

---

#### mbc_posix_tcp_create

Create and connect POSIX TCP client.

```c
mbc_status_t mbc_posix_tcp_create(
    const mbc_posix_tcp_config_t *config,
    mbc_transport_iface_t *out_iface,
    mbc_posix_tcp_ctx_t **out_ctx
);
```

**Parameters:**
- `config` - Connection configuration
- `out_iface` - Transport interface (output)
- `out_ctx` - Allocated context (output)

**Returns:**
- `MBC_STATUS_OK` - Connected successfully
- `MBC_STATUS_TRANSPORT_ERROR` - Connection failed

**Example:**
```c
mbc_posix_tcp_config_t config = {
    .host = "192.168.1.100",
    .port = 502,
    .connect_timeout_ms = 5000,
    .recv_timeout_ms = 1000
};

mbc_transport_iface_t iface;
mbc_posix_tcp_ctx_t *ctx = NULL;

mbc_status_t status = mbc_posix_tcp_create(&config, &iface, &ctx);

if (status == MBC_STATUS_OK) {
    printf("Connected to %s:%u\n", config.host, config.port);
    
    // Use with runtime builder
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    // ... perform operations ...
    
    mbc_posix_tcp_destroy(ctx);
}
```

---

#### mbc_posix_tcp_destroy

Destroy TCP context and close socket.

```c
void mbc_posix_tcp_destroy(mbc_posix_tcp_ctx_t *ctx);
```

**Example:**
```c
mbc_posix_tcp_destroy(ctx);  // Safe to call with NULL
```

---

#### mbc_posix_tcp_is_connected

Check if connection is active.

```c
bool mbc_posix_tcp_is_connected(const mbc_posix_tcp_ctx_t *ctx);
```

**Example:**
```c
if (!mbc_posix_tcp_is_connected(ctx)) {
    printf("Connection lost\n");
    // Reconnect logic...
}
```

---

### Winsock TCP

**Platform:** Windows  
**Header:** `<modbuscore/transport/winsock_tcp.h>`

#### mbc_winsock_tcp_config_t

Windows TCP configuration:

```c
typedef struct mbc_winsock_tcp_config {
    const char *host;            // Target hostname or IP
    uint16_t port;               // Target port
    uint32_t connect_timeout_ms; // Connection timeout
    uint32_t recv_timeout_ms;    // Receive timeout
} mbc_winsock_tcp_config_t;
```

---

#### mbc_winsock_tcp_create

Create Windows TCP client.

```c
mbc_status_t mbc_winsock_tcp_create(
    const mbc_winsock_tcp_config_t *config,
    mbc_transport_iface_t *out_iface,
    mbc_winsock_tcp_ctx_t **out_ctx
);
```

**Example:**
```c
#ifdef _WIN32

mbc_winsock_tcp_config_t config = {
    .host = "10.0.0.50",
    .port = 502,
    .connect_timeout_ms = 3000,
    .recv_timeout_ms = 1000
};

mbc_transport_iface_t iface;
mbc_winsock_tcp_ctx_t *ctx = NULL;

if (mbc_winsock_tcp_create(&config, &iface, &ctx) == MBC_STATUS_OK) {
    printf("Connected (Windows)\n");
    
    // Use transport...
    
    mbc_winsock_tcp_destroy(ctx);
}

#endif
```

---

#### mbc_winsock_tcp_destroy

Destroy Winsock context.

```c
void mbc_winsock_tcp_destroy(mbc_winsock_tcp_ctx_t *ctx);
```

---

#### mbc_winsock_tcp_is_connected

Check Windows connection status.

```c
bool mbc_winsock_tcp_is_connected(const mbc_winsock_tcp_ctx_t *ctx);
```

---

## RTU Transport

### POSIX RTU

**Platform:** Linux, macOS, BSD (serial devices)  
**Header:** `<modbuscore/transport/posix_rtu.h>`

#### mbc_posix_rtu_config_t

Serial RTU configuration:

```c
typedef struct mbc_posix_rtu_config {
    const char *device_path;     // Device path (e.g., "/dev/ttyUSB0")
    uint32_t baud_rate;          // Baud rate (e.g., 9600, 19200)
    uint8_t data_bits;           // Data bits (5-8, default 8)
    char parity;                 // 'N' (none), 'E' (even), 'O' (odd)
    uint8_t stop_bits;           // Stop bits (1 or 2)
    uint32_t guard_time_us;      // Inter-frame guard time (0 = auto)
    size_t rx_buffer_capacity;   // RX buffer size (default 256)
} mbc_posix_rtu_config_t;
```

---

#### mbc_posix_rtu_create

Create POSIX RTU serial transport.

```c
mbc_status_t mbc_posix_rtu_create(
    const mbc_posix_rtu_config_t *config,
    mbc_transport_iface_t *out_iface,
    mbc_posix_rtu_ctx_t **out_ctx
);
```

**Example:**
```c
mbc_posix_rtu_config_t config = {
    .device_path = "/dev/ttyUSB0",
    .baud_rate = 19200,
    .data_bits = 8,
    .parity = 'N',
    .stop_bits = 1,
    .guard_time_us = 0,  // Auto-calculate
    .rx_buffer_capacity = 256
};

mbc_transport_iface_t iface;
mbc_posix_rtu_ctx_t *ctx = NULL;

mbc_status_t status = mbc_posix_rtu_create(&config, &iface, &ctx);

if (status == MBC_STATUS_OK) {
    printf("Opened %s at %u baud\n", config.device_path, config.baud_rate);
    
    // Use with runtime
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    // ... RTU operations ...
    
    mbc_posix_rtu_destroy(ctx);
}
```

**Guard Time:** Automatically calculated as 3.5 character times if `guard_time_us = 0`.

---

#### mbc_posix_rtu_destroy

Destroy RTU context and close serial port.

```c
void mbc_posix_rtu_destroy(mbc_posix_rtu_ctx_t *ctx);
```

---

#### mbc_posix_rtu_reset

Reset RTU transport state.

```c
void mbc_posix_rtu_reset(mbc_posix_rtu_ctx_t *ctx);
```

Clears RX buffers and resets frame detection state.

---

### Windows RTU

**Platform:** Windows  
**Header:** `<modbuscore/transport/win32_rtu.h>`

Similar API to POSIX RTU but uses Windows serial APIs (`CreateFile`, `ReadFile`, `WriteFile`).

#### mbc_win32_rtu_create

```c
mbc_status_t mbc_win32_rtu_create(
    const mbc_win32_rtu_config_t *config,
    mbc_transport_iface_t *out_iface,
    mbc_win32_rtu_ctx_t **out_ctx
);
```

**Example:**
```c
#ifdef _WIN32

mbc_win32_rtu_config_t config = {
    .device_path = "COM3",
    .baud_rate = 9600,
    .data_bits = 8,
    .parity = 'N',
    .stop_bits = 1
};

mbc_transport_iface_t iface;
mbc_win32_rtu_ctx_t *ctx = NULL;

if (mbc_win32_rtu_create(&config, &iface, &ctx) == MBC_STATUS_OK) {
    printf("Opened COM3\n");
    
    // Use transport...
    
    mbc_win32_rtu_destroy(ctx);
}

#endif
```

---

## Mock Transport

**Header:** `<modbuscore/transport/mock.h>`

Mock transports are used for testing and simulation.

### mbc_mock_transport_create

Create a mock transport with scripted responses.

```c
mbc_status_t mbc_mock_transport_create(
    mbc_transport_iface_t *out_iface,
    mbc_mock_transport_ctx_t **out_ctx
);
```

**Example:**
```c
mbc_transport_iface_t iface;
mbc_mock_transport_ctx_t *ctx = NULL;

mbc_mock_transport_create(&iface, &ctx);

// Configure mock responses
uint8_t response[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x01, 0x03, 0x02, 0x00, 0x64};
mbc_mock_transport_add_response(ctx, response, sizeof(response));

// Use with runtime for testing
mbc_runtime_builder_with_transport(&builder, &iface);

// ...

mbc_mock_transport_destroy(ctx);
```

---

## Custom Transports

You can implement custom transports by filling in the `mbc_transport_iface_t` structure.

### Example: Custom Transport

```c
typedef struct {
    int my_fd;
    // ... custom state ...
} my_transport_ctx_t;

mbc_status_t my_send(void *ctx, const uint8_t *buffer, size_t length,
                     mbc_transport_io_t *out) {
    my_transport_ctx_t *t = (my_transport_ctx_t*)ctx;
    
    ssize_t sent = my_custom_write(t->my_fd, buffer, length);
    
    if (sent < 0) {
        return MBC_STATUS_TRANSPORT_ERROR;
    }
    
    if (out) {
        out->processed = (size_t)sent;
    }
    
    return MBC_STATUS_OK;
}

mbc_status_t my_receive(void *ctx, uint8_t *buffer, size_t capacity,
                        mbc_transport_io_t *out) {
    my_transport_ctx_t *t = (my_transport_ctx_t*)ctx;
    
    ssize_t received = my_custom_read(t->my_fd, buffer, capacity);
    
    if (received < 0) {
        return MBC_STATUS_WOULD_BLOCK;
    }
    
    if (out) {
        out->processed = (size_t)received;
    }
    
    return MBC_STATUS_OK;
}

uint64_t my_now(void *ctx) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void my_yield(void *ctx) {
    // Optional: yield to scheduler
    sched_yield();
}

// Create custom transport
my_transport_ctx_t my_ctx = { .my_fd = open_device() };

mbc_transport_iface_t iface = {
    .ctx = &my_ctx,
    .send = my_send,
    .receive = my_receive,
    .now = my_now,
    .yield = my_yield
};

// Use with runtime
mbc_runtime_builder_with_transport(&builder, &iface);
```

---

## Platform-Specific Examples

### Linux TCP Client

```c
#include <modbuscore/transport/posix_tcp.h>

mbc_posix_tcp_config_t config = {
    .host = "192.168.0.10",
    .port = 502,
    .connect_timeout_ms = 5000,
    .recv_timeout_ms = 1000
};

mbc_transport_iface_t iface;
mbc_posix_tcp_ctx_t *ctx = NULL;

if (mbc_posix_tcp_create(&config, &iface, &ctx) == MBC_STATUS_OK) {
    // Connected!
}
```

---

### Windows TCP Client

```c
#ifdef _WIN32
#include <modbuscore/transport/winsock_tcp.h>

mbc_winsock_tcp_config_t config = {
    .host = "10.0.0.5",
    .port = 502,
    .connect_timeout_ms = 3000,
    .recv_timeout_ms = 1000
};

mbc_transport_iface_t iface;
mbc_winsock_tcp_ctx_t *ctx = NULL;

mbc_winsock_tcp_create(&config, &iface, &ctx);
#endif
```

---

### Linux RTU Serial

```c
#include <modbuscore/transport/posix_rtu.h>

mbc_posix_rtu_config_t config = {
    .device_path = "/dev/ttyUSB0",
    .baud_rate = 9600,
    .data_bits = 8,
    .parity = 'N',
    .stop_bits = 1
};

mbc_transport_iface_t iface;
mbc_posix_rtu_ctx_t *ctx = NULL;

mbc_posix_rtu_create(&config, &iface, &ctx);
```

---

### Windows RTU Serial

```c
#ifdef _WIN32
#include <modbuscore/transport/win32_rtu.h>

mbc_win32_rtu_config_t config = {
    .device_path = "COM1",
    .baud_rate = 19200,
    .data_bits = 8,
    .parity = 'E',
    .stop_bits = 1
};

mbc_transport_iface_t iface;
mbc_win32_rtu_ctx_t *ctx = NULL;

mbc_win32_rtu_create(&config, &iface, &ctx);
#endif
```

---

## Thread Safety

Transport drivers are **not thread-safe** by default. If using transports from multiple threads, wrap calls with mutexes.

## Performance Notes

- **TCP:** ~500 µs latency per request on LAN
- **RTU:** ~10-50 ms per request (depends on baud rate)
- **Non-blocking:** All transports return `MBC_STATUS_WOULD_BLOCK` when no data is available

## Next Steps

- [Runtime API](runtime.md) - High-level client/server runtime
- [Protocol API](protocol.md) - Low-level protocol functions
- [User Guide: Transports](../user-guide/transports.md) - Transport selection guide

---

**Prev**: [Diagnostics API ←](diagnostics.md) | **Next**: [Runtime API →](runtime.md)
