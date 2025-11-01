# Custom Transport Implementation

Complete guide to implementing custom transport layers for ModbusCore, enabling support for serial ports, Bluetooth, CAN bus, and other communication media.

## Table of Contents

- [Overview](#overview)
- [Transport Interface](#transport-interface)
- [Implementation Guide](#implementation-guide)
- [Example: UART Transport](#example-uart-transport)
- [Example: Bluetooth Transport](#example-bluetooth-transport)
- [Example: CAN Bus Transport](#example-can-bus-transport)
- [Testing Custom Transports](#testing-custom-transports)
- [Best Practices](#best-practices)
- [Debugging](#debugging)
- [Performance Optimization](#performance-optimization)

---

## Overview

ModbusCore uses a **transport abstraction layer** to decouple protocol logic from physical communication. This allows the same protocol engine to work with:

- **TCP sockets** (POSIX/Winsock)
- **Serial ports** (UART/RS-485/RS-232)
- **Wireless** (Bluetooth, WiFi, LoRa)
- **Fieldbus** (CAN bus, Profibus)
- **Mock transports** (testing/simulation)

### Architecture

```
┌──────────────────────────────────────────────┐
│         ModbusCore Protocol Engine           │
│  (client.c, server.c, pdu.c, mbap.c)         │
└───────────────────┬──────────────────────────┘
                    │
                    │ mbc_transport_iface_t
                    │
        ┌───────────┴───────────┐
        │                       │
┌───────▼───────┐       ┌──────▼─────────┐
│  TCP/IP       │       │  Custom        │
│  (posix_tcp.c)│       │  (your_impl.c) │
└───────┬───────┘       └──────┬─────────┘
        │                      │
┌───────▼────────┐     ┌───────▼────────┐
│  Socket API    │     │  UART/BLE/CAN  │
│  (send/recv)   │     │  (HAL calls)   │
└────────────────┘     └────────────────┘
```

---

## Transport Interface

### Core Structure

Defined in `include/modbuscore/runtime/dependencies.h`:

```c
/**
 * @brief Transport layer interface.
 *
 * All callbacks are non-blocking and return status codes.
 */
typedef struct mbc_transport_iface {
    void* ctx; /**< User context (your transport state) */

    /**
     * @brief Send data through the transport.
     *
     * @param ctx User context
     * @param buffer Data to send
     * @param length Number of bytes to send
     * @param out I/O result (bytes sent, timing)
     * @return MBC_STATUS_OK on success, error code otherwise
     */
    mbc_status_t (*send)(void* ctx, const uint8_t* buffer, size_t length,
                         mbc_transport_io_t* out);

    /**
     * @brief Receive data from the transport.
     *
     * @param ctx User context
     * @param buffer Buffer to receive data into
     * @param capacity Buffer capacity
     * @param out I/O result (bytes received, timing)
     * @return MBC_STATUS_OK on success, error code otherwise
     */
    mbc_status_t (*receive)(void* ctx, uint8_t* buffer, size_t capacity,
                            mbc_transport_io_t* out);

    /**
     * @brief Get current timestamp in milliseconds (optional).
     *
     * Used for timeout management and diagnostics.
     *
     * @param ctx User context
     * @return Current time in milliseconds
     */
    uint64_t (*now)(void* ctx);

    /**
     * @brief Cooperative yield (optional).
     *
     * Called by protocol engine to allow cooperative multitasking.
     * Can advance simulation time in testing scenarios.
     *
     * @param ctx User context
     */
    void (*yield)(void* ctx);
} mbc_transport_iface_t;
```

### I/O Result Structure

```c
/**
 * @brief Transport I/O operation result.
 */
typedef struct mbc_transport_io {
    size_t processed;   /**< Number of bytes sent/received */
    uint32_t elapsed_ms; /**< Time spent in I/O operation (optional) */
} mbc_transport_io_t;
```

### Helper Functions

ModbusCore provides convenience wrappers in `transport/iface.h`:

```c
/* Validates parameters and calls transport->send() */
mbc_status_t mbc_transport_send(const mbc_transport_iface_t* iface,
                                const uint8_t* buffer, size_t length,
                                mbc_transport_io_t* out);

/* Validates parameters and calls transport->receive() */
mbc_status_t mbc_transport_receive(const mbc_transport_iface_t* iface,
                                   uint8_t* buffer, size_t capacity,
                                   mbc_transport_io_t* out);

/* Safe wrapper for now() callback */
uint64_t mbc_transport_now(const mbc_transport_iface_t* iface);

/* Safe wrapper for yield() callback */
void mbc_transport_yield(const mbc_transport_iface_t* iface);
```

---

## Implementation Guide

### Step 1: Define Transport Context

Your transport needs internal state (file descriptors, buffers, configuration):

```c
/* my_transport.h */
#ifndef MY_TRANSPORT_H
#define MY_TRANSPORT_H

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>

typedef struct {
    const char* port_name;  /**< Serial port name (e.g., "/dev/ttyUSB0") */
    uint32_t baud_rate;     /**< Baud rate (9600, 115200, etc.) */
    uint32_t timeout_ms;    /**< Read timeout in milliseconds */
} my_transport_config_t;

typedef struct my_transport my_transport_t;

/* Lifecycle functions */
mbc_status_t my_transport_create(const my_transport_config_t* config,
                                 mbc_transport_iface_t* out_iface,
                                 my_transport_t** out_ctx);

void my_transport_destroy(my_transport_t* ctx);

/* Optional: Connection management */
bool my_transport_is_connected(const my_transport_t* ctx);

#endif /* MY_TRANSPORT_H */
```

### Step 2: Implement Internal State

```c
/* my_transport.c */
#include "my_transport.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

struct my_transport {
    int fd;                 /**< File descriptor for serial port */
    uint32_t timeout_ms;    /**< Read timeout */
    bool connected;         /**< Connection state */
};
```

### Step 3: Implement Callback Functions

#### Send Callback

```c
static mbc_status_t my_transport_send(void* ctx, const uint8_t* buffer,
                                      size_t length, mbc_transport_io_t* out)
{
    my_transport_t* transport = (my_transport_t*)ctx;

    /* Validate parameters */
    if (!transport || !buffer || length == 0) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!transport->connected) {
        return MBC_STATUS_IO_ERROR;
    }

    /* Write to serial port */
    ssize_t written = write(transport->fd, buffer, length);
    if (written < 0) {
        /* I/O error (disconnected, hardware failure) */
        transport->connected = false;
        return MBC_STATUS_IO_ERROR;
    }

    /* Report bytes written */
    if (out) {
        out->processed = (size_t)written;
        out->elapsed_ms = 0; /* Optional: measure actual time */
    }

    return MBC_STATUS_OK;
}
```

#### Receive Callback

```c
static mbc_status_t my_transport_receive(void* ctx, uint8_t* buffer,
                                         size_t capacity, mbc_transport_io_t* out)
{
    my_transport_t* transport = (my_transport_t*)ctx;

    /* Validate parameters */
    if (!transport || !buffer || capacity == 0) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!transport->connected) {
        return MBC_STATUS_IO_ERROR;
    }

    /* Non-blocking read */
    ssize_t received = read(transport->fd, buffer, capacity);

    if (received < 0) {
        /* Check if it's a timeout (EWOULDBLOCK/EAGAIN) or real error */
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            /* No data available - not an error */
            if (out) {
                out->processed = 0;
            }
            return MBC_STATUS_OK;
        }

        /* Real I/O error */
        transport->connected = false;
        return MBC_STATUS_IO_ERROR;
    }

    /* Report bytes received */
    if (out) {
        out->processed = (size_t)received;
        out->elapsed_ms = 0;
    }

    return MBC_STATUS_OK;
}
```

#### Timestamp Callback (Optional but Recommended)

```c
static uint64_t my_transport_now(void* ctx)
{
    (void)ctx; /* Unused */

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}
```

#### Yield Callback (Optional)

```c
static void my_transport_yield(void* ctx)
{
    (void)ctx; /* Unused */

    /* Cooperative yield - useful in FreeRTOS/Zephyr */
    /* Example: taskYIELD(); or sched_yield(); */
    usleep(1000); /* Simple: sleep 1ms */
}
```

### Step 4: Implement Create/Destroy Functions

#### Create

```c
mbc_status_t my_transport_create(const my_transport_config_t* config,
                                 mbc_transport_iface_t* out_iface,
                                 my_transport_t** out_ctx)
{
    if (!config || !out_iface || !out_ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    /* Allocate context */
    my_transport_t* transport = calloc(1, sizeof(*transport));
    if (!transport) {
        return MBC_STATUS_NO_RESOURCES;
    }

    /* Open serial port */
    transport->fd = open(config->port_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (transport->fd < 0) {
        free(transport);
        return MBC_STATUS_IO_ERROR;
    }

    /* Configure serial port (baud rate, 8N1, etc.) */
    struct termios tty;
    if (tcgetattr(transport->fd, &tty) != 0) {
        close(transport->fd);
        free(transport);
        return MBC_STATUS_IO_ERROR;
    }

    /* Set baud rate */
    speed_t speed = B9600;
    switch (config->baud_rate) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        default:     speed = B9600;   break;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* 8N1 mode */
    tty.c_cflag &= ~PARENB;  /* No parity */
    tty.c_cflag &= ~CSTOPB;  /* 1 stop bit */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      /* 8 data bits */
    tty.c_cflag |= CREAD | CLOCAL; /* Enable receiver, ignore modem control */

    /* Raw mode (no canonical, no echo) */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); /* No software flow control */
    tty.c_oflag &= ~OPOST; /* Raw output */

    /* Timeout configuration (VMIN=0, VTIME=timeout in deciseconds) */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = config->timeout_ms / 100; /* Convert ms to deciseconds */

    if (tcsetattr(transport->fd, TCSANOW, &tty) != 0) {
        close(transport->fd);
        free(transport);
        return MBC_STATUS_IO_ERROR;
    }

    /* Initialize state */
    transport->timeout_ms = config->timeout_ms;
    transport->connected = true;

    /* Fill transport interface */
    out_iface->ctx = transport;
    out_iface->send = my_transport_send;
    out_iface->receive = my_transport_receive;
    out_iface->now = my_transport_now;
    out_iface->yield = my_transport_yield;

    *out_ctx = transport;
    return MBC_STATUS_OK;
}
```

#### Destroy

```c
void my_transport_destroy(my_transport_t* ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    ctx->connected = false;
    free(ctx);
}
```

#### Connection Status

```c
bool my_transport_is_connected(const my_transport_t* ctx)
{
    return ctx && ctx->connected;
}
```

---

## Example: UART Transport

Complete implementation for STM32 UART (bare-metal):

```c
/* stm32_uart_transport.h */
#ifndef STM32_UART_TRANSPORT_H
#define STM32_UART_TRANSPORT_H

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>
#include <stm32f4xx_hal.h>

typedef struct {
    UART_HandleTypeDef* huart; /**< STM32 HAL UART handle */
    uint32_t timeout_ms;       /**< Timeout for receive operations */
} stm32_uart_config_t;

typedef struct stm32_uart_transport stm32_uart_transport_t;

mbc_status_t stm32_uart_create(const stm32_uart_config_t* config,
                               mbc_transport_iface_t* out_iface,
                               stm32_uart_transport_t** out_ctx);

void stm32_uart_destroy(stm32_uart_transport_t* ctx);

#endif /* STM32_UART_TRANSPORT_H */
```

```c
/* stm32_uart_transport.c */
#include "stm32_uart_transport.h"
#include <stdlib.h>

struct stm32_uart_transport {
    UART_HandleTypeDef* huart;
    uint32_t timeout_ms;
};

static mbc_status_t stm32_uart_send(void* ctx, const uint8_t* buffer,
                                    size_t length, mbc_transport_io_t* out)
{
    stm32_uart_transport_t* uart = (stm32_uart_transport_t*)ctx;

    if (!uart || !buffer || length == 0) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    /* HAL_UART_Transmit blocks until complete (use HAL_UART_Transmit_DMA for async) */
    HAL_StatusTypeDef status = HAL_UART_Transmit(uart->huart,
                                                  (uint8_t*)buffer,
                                                  (uint16_t)length,
                                                  uart->timeout_ms);

    if (status != HAL_OK) {
        return MBC_STATUS_IO_ERROR;
    }

    if (out) {
        out->processed = length;
    }

    return MBC_STATUS_OK;
}

static mbc_status_t stm32_uart_receive(void* ctx, uint8_t* buffer,
                                       size_t capacity, mbc_transport_io_t* out)
{
    stm32_uart_transport_t* uart = (stm32_uart_transport_t*)ctx;

    if (!uart || !buffer || capacity == 0) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    /* Non-blocking receive: try to read up to capacity bytes */
    uint16_t received = 0;

    /* Check if data is available */
    if (__HAL_UART_GET_FLAG(uart->huart, UART_FLAG_RXNE)) {
        /* Receive with short timeout (1ms) to simulate non-blocking */
        HAL_StatusTypeDef status = HAL_UART_Receive(uart->huart, buffer,
                                                     (uint16_t)capacity, 1);

        if (status == HAL_OK) {
            received = (uint16_t)capacity;
        } else if (status == HAL_TIMEOUT) {
            /* Timeout is not an error - just no data */
            received = 0;
        } else {
            return MBC_STATUS_IO_ERROR;
        }
    }

    if (out) {
        out->processed = received;
    }

    return MBC_STATUS_OK;
}

static uint64_t stm32_uart_now(void* ctx)
{
    (void)ctx;
    return (uint64_t)HAL_GetTick(); /* STM32 HAL tick (1ms resolution) */
}

static void stm32_uart_yield(void* ctx)
{
    (void)ctx;
    /* Cooperative yield - in bare-metal, just return */
    /* In FreeRTOS: taskYIELD(); */
}

mbc_status_t stm32_uart_create(const stm32_uart_config_t* config,
                               mbc_transport_iface_t* out_iface,
                               stm32_uart_transport_t** out_ctx)
{
    if (!config || !config->huart || !out_iface || !out_ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    stm32_uart_transport_t* uart = calloc(1, sizeof(*uart));
    if (!uart) {
        return MBC_STATUS_NO_RESOURCES;
    }

    uart->huart = config->huart;
    uart->timeout_ms = config->timeout_ms;

    out_iface->ctx = uart;
    out_iface->send = stm32_uart_send;
    out_iface->receive = stm32_uart_receive;
    out_iface->now = stm32_uart_now;
    out_iface->yield = stm32_uart_yield;

    *out_ctx = uart;
    return MBC_STATUS_OK;
}

void stm32_uart_destroy(stm32_uart_transport_t* ctx)
{
    free(ctx);
}
```

### Usage

```c
/* main.c */
#include "stm32_uart_transport.h"
#include <modbuscore/client/client.h>

UART_HandleTypeDef huart2; /* Initialized by STM32CubeMX */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init(); /* Configure UART2 */

    /* Create UART transport */
    stm32_uart_config_t uart_config = {
        .huart = &huart2,
        .timeout_ms = 1000
    };

    mbc_transport_iface_t transport;
    stm32_uart_transport_t* uart_ctx = NULL;

    mbc_status_t status = stm32_uart_create(&uart_config, &transport, &uart_ctx);
    if (status != MBC_STATUS_OK) {
        Error_Handler();
    }

    /* Create Modbus client */
    mbc_client_config_t client_config = {
        .mode = MBC_MODE_RTU,
        .timeout_ms = 1000,
        .max_retries = 3
    };

    mbc_client_t client;
    status = mbc_client_create(&client_config, &transport, &client);
    if (status != MBC_STATUS_OK) {
        Error_Handler();
    }

    /* Read holding registers */
    uint16_t registers[10];
    status = mbc_client_read_holding_registers(&client, 1, 0x0000, 10, registers);
    if (status == MBC_STATUS_OK) {
        /* Process registers */
    }

    /* Cleanup */
    mbc_client_destroy(&client);
    stm32_uart_destroy(uart_ctx);

    while (1) {
        HAL_Delay(1000);
    }
}
```

---

## Example: Bluetooth Transport

Bluetooth Low Energy (BLE) transport using BlueZ on Linux:

```c
/* ble_transport.h */
#ifndef BLE_TRANSPORT_H
#define BLE_TRANSPORT_H

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>

typedef struct {
    const char* device_address; /**< BLE device MAC address (e.g., "AA:BB:CC:DD:EE:FF") */
    const char* service_uuid;   /**< GATT service UUID */
    const char* char_uuid;      /**< GATT characteristic UUID for TX/RX */
} ble_transport_config_t;

typedef struct ble_transport ble_transport_t;

mbc_status_t ble_transport_create(const ble_transport_config_t* config,
                                  mbc_transport_iface_t* out_iface,
                                  ble_transport_t** out_ctx);

void ble_transport_destroy(ble_transport_t* ctx);

#endif /* BLE_TRANSPORT_H */
```

```c
/* ble_transport.c (simplified using BlueZ D-Bus API) */
#include "ble_transport.h"
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

struct ble_transport {
    GDBusConnection* connection;
    GDBusProxy* char_proxy;
    uint8_t rx_buffer[256];
    size_t rx_len;
    bool connected;
};

static mbc_status_t ble_transport_send(void* ctx, const uint8_t* buffer,
                                       size_t length, mbc_transport_io_t* out)
{
    ble_transport_t* ble = (ble_transport_t*)ctx;

    if (!ble || !buffer || length == 0 || !ble->connected) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    /* Write to BLE characteristic via D-Bus */
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));

    for (size_t i = 0; i < length; i++) {
        g_variant_builder_add(&builder, "y", buffer[i]);
    }

    GVariant* value = g_variant_builder_end(&builder);
    GError* error = NULL;

    g_dbus_proxy_call_sync(ble->char_proxy,
                           "WriteValue",
                           g_variant_new("(@ay)", value),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, &error);

    if (error) {
        g_error_free(error);
        return MBC_STATUS_IO_ERROR;
    }

    if (out) {
        out->processed = length;
    }

    return MBC_STATUS_OK;
}

static mbc_status_t ble_transport_receive(void* ctx, uint8_t* buffer,
                                          size_t capacity, mbc_transport_io_t* out)
{
    ble_transport_t* ble = (ble_transport_t*)ctx;

    if (!ble || !buffer || capacity == 0) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    /* Check if data is available in internal buffer */
    size_t to_copy = (ble->rx_len < capacity) ? ble->rx_len : capacity;
    memcpy(buffer, ble->rx_buffer, to_copy);

    /* Consume data from buffer */
    memmove(ble->rx_buffer, ble->rx_buffer + to_copy, ble->rx_len - to_copy);
    ble->rx_len -= to_copy;

    if (out) {
        out->processed = to_copy;
    }

    return MBC_STATUS_OK;
}

/* ... (implement create/destroy similarly to UART example) ... */
```

---

## Example: CAN Bus Transport

CAN bus transport using SocketCAN on Linux:

```c
/* can_transport.h */
#ifndef CAN_TRANSPORT_H
#define CAN_TRANSPORT_H

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>

typedef struct {
    const char* interface_name; /**< CAN interface (e.g., "can0") */
    uint32_t can_id_tx;         /**< CAN ID for transmit */
    uint32_t can_id_rx;         /**< CAN ID for receive */
} can_transport_config_t;

typedef struct can_transport can_transport_t;

mbc_status_t can_transport_create(const can_transport_config_t* config,
                                  mbc_transport_iface_t* out_iface,
                                  can_transport_t** out_ctx);

void can_transport_destroy(can_transport_t* ctx);

#endif /* CAN_TRANSPORT_H */
```

```c
/* can_transport.c */
#include "can_transport.h"
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

struct can_transport {
    int sockfd;
    uint32_t can_id_tx;
    uint32_t can_id_rx;
    bool connected;
};

static mbc_status_t can_transport_send(void* ctx, const uint8_t* buffer,
                                       size_t length, mbc_transport_io_t* out)
{
    can_transport_t* can = (can_transport_t*)ctx;

    if (!can || !buffer || length == 0) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    /* Fragment data into CAN frames (max 8 bytes per frame) */
    size_t sent = 0;

    while (sent < length) {
        struct can_frame frame = {0};
        frame.can_id = can->can_id_tx;
        frame.can_dlc = (length - sent < 8) ? (length - sent) : 8;

        memcpy(frame.data, buffer + sent, frame.can_dlc);

        ssize_t written = write(can->sockfd, &frame, sizeof(frame));
        if (written != sizeof(frame)) {
            return MBC_STATUS_IO_ERROR;
        }

        sent += frame.can_dlc;
    }

    if (out) {
        out->processed = sent;
    }

    return MBC_STATUS_OK;
}

static mbc_status_t can_transport_receive(void* ctx, uint8_t* buffer,
                                          size_t capacity, mbc_transport_io_t* out)
{
    can_transport_t* can = (can_transport_t*)ctx;

    if (!can || !buffer || capacity == 0) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    struct can_frame frame;
    ssize_t received = read(can->sockfd, &frame, sizeof(frame));

    if (received < 0) {
        /* No data available (non-blocking) */
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (out) {
                out->processed = 0;
            }
            return MBC_STATUS_OK;
        }
        return MBC_STATUS_IO_ERROR;
    }

    /* Filter by CAN ID */
    if (frame.can_id != can->can_id_rx) {
        if (out) {
            out->processed = 0;
        }
        return MBC_STATUS_OK; /* Ignore frame */
    }

    /* Copy data to buffer */
    size_t to_copy = (frame.can_dlc < capacity) ? frame.can_dlc : capacity;
    memcpy(buffer, frame.data, to_copy);

    if (out) {
        out->processed = to_copy;
    }

    return MBC_STATUS_OK;
}

mbc_status_t can_transport_create(const can_transport_config_t* config,
                                  mbc_transport_iface_t* out_iface,
                                  can_transport_t** out_ctx)
{
    if (!config || !out_iface || !out_ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    can_transport_t* can = calloc(1, sizeof(*can));
    if (!can) {
        return MBC_STATUS_NO_RESOURCES;
    }

    /* Open CAN socket */
    can->sockfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can->sockfd < 0) {
        free(can);
        return MBC_STATUS_IO_ERROR;
    }

    /* Get interface index */
    struct ifreq ifr;
    strncpy(ifr.ifr_name, config->interface_name, IFNAMSIZ - 1);
    if (ioctl(can->sockfd, SIOCGIFINDEX, &ifr) < 0) {
        close(can->sockfd);
        free(can);
        return MBC_STATUS_IO_ERROR;
    }

    /* Bind to CAN interface */
    struct sockaddr_can addr = {0};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(can->sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(can->sockfd);
        free(can);
        return MBC_STATUS_IO_ERROR;
    }

    /* Set non-blocking mode */
    int flags = fcntl(can->sockfd, F_GETFL, 0);
    fcntl(can->sockfd, F_SETFL, flags | O_NONBLOCK);

    can->can_id_tx = config->can_id_tx;
    can->can_id_rx = config->can_id_rx;
    can->connected = true;

    out_iface->ctx = can;
    out_iface->send = can_transport_send;
    out_iface->receive = can_transport_receive;
    out_iface->now = NULL; /* Use system clock */
    out_iface->yield = NULL;

    *out_ctx = can;
    return MBC_STATUS_OK;
}

void can_transport_destroy(can_transport_t* ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->sockfd >= 0) {
        close(ctx->sockfd);
    }

    free(ctx);
}
```

---

## Testing Custom Transports

### Unit Tests with Mock Transport

Use ModbusCore's mock transport to test protocol logic independently:

```c
#include <modbuscore/transport/mock.h>
#include <assert.h>

void test_custom_transport_logic(void)
{
    /* Create mock transport */
    mbc_transport_iface_t mock_iface;
    mbc_mock_transport_t* mock = NULL;

    assert(mbc_mock_transport_create(NULL, &mock_iface, &mock) == MBC_STATUS_OK);

    /* Inject response frame */
    uint8_t response[] = {0x01, 0x03, 0x04, 0x00, 0x01, 0x00, 0x02};
    mbc_mock_transport_schedule_rx(mock, response, sizeof(response), 0);

    /* Test your transport wrapper using mock as backend */
    /* ... */

    mbc_mock_transport_destroy(mock);
}
```

### Loopback Test

Test your transport by connecting TX to RX:

```c
void test_uart_loopback(void)
{
    /* Configure UART with TX connected to RX (hardware jumper) */
    my_transport_config_t config = {
        .port_name = "/dev/ttyUSB0",
        .baud_rate = 9600,
        .timeout_ms = 1000
    };

    mbc_transport_iface_t transport;
    my_transport_t* ctx = NULL;

    assert(my_transport_create(&config, &transport, &ctx) == MBC_STATUS_OK);

    /* Send data */
    uint8_t tx_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    mbc_transport_io_t io = {0};

    assert(mbc_transport_send(&transport, tx_data, sizeof(tx_data), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(tx_data));

    /* Receive data (should match) */
    uint8_t rx_data[sizeof(tx_data)] = {0};
    usleep(10000); /* Wait for loopback propagation */

    assert(mbc_transport_receive(&transport, rx_data, sizeof(rx_data), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(tx_data));
    assert(memcmp(rx_data, tx_data, sizeof(tx_data)) == 0);

    my_transport_destroy(ctx);
}
```

### Integration Test with Real Modbus Device

```c
void test_uart_with_real_device(void)
{
    my_transport_config_t config = {
        .port_name = "/dev/ttyUSB0",
        .baud_rate = 9600,
        .timeout_ms = 1000
    };

    mbc_transport_iface_t transport;
    my_transport_t* ctx = NULL;

    assert(my_transport_create(&config, &transport, &ctx) == MBC_STATUS_OK);

    /* Create Modbus client */
    mbc_client_config_t client_config = {
        .mode = MBC_MODE_RTU,
        .timeout_ms = 1000,
        .max_retries = 3
    };

    mbc_client_t client;
    assert(mbc_client_create(&client_config, &transport, &client) == MBC_STATUS_OK);

    /* Read registers from device at address 1 */
    uint16_t registers[10];
    mbc_status_t status = mbc_client_read_holding_registers(&client, 1, 0x0000, 10, registers);

    printf("Read status: %d\n", status);
    if (status == MBC_STATUS_OK) {
        for (int i = 0; i < 10; i++) {
            printf("Register[%d] = 0x%04X\n", i, registers[i]);
        }
    }

    mbc_client_destroy(&client);
    my_transport_destroy(ctx);
}
```

---

## Best Practices

### 1. Non-Blocking I/O

Always implement non-blocking send/receive:

```c
/* ✅ GOOD: Non-blocking receive */
static mbc_status_t good_receive(void* ctx, uint8_t* buffer, size_t capacity,
                                 mbc_transport_io_t* out)
{
    ssize_t received = read(fd, buffer, capacity); /* Non-blocking */

    if (received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            /* No data available - return OK with 0 bytes */
            if (out) {
                out->processed = 0;
            }
            return MBC_STATUS_OK;
        }
        return MBC_STATUS_IO_ERROR;
    }

    if (out) {
        out->processed = (size_t)received;
    }
    return MBC_STATUS_OK;
}

/* ❌ BAD: Blocking receive */
static mbc_status_t bad_receive(void* ctx, uint8_t* buffer, size_t capacity,
                                mbc_transport_io_t* out)
{
    /* This blocks until 'capacity' bytes are received! */
    ssize_t received = read(fd, buffer, capacity);
    /* ... */
}
```

### 2. Error Handling

Distinguish between transient and permanent errors:

```c
if (errno == EWOULDBLOCK || errno == EAGAIN) {
    /* Transient: no data available */
    return MBC_STATUS_OK; /* processed = 0 */
}

if (errno == ECONNRESET || errno == EPIPE) {
    /* Permanent: connection lost */
    return MBC_STATUS_IO_ERROR;
}
```

### 3. Resource Management

Always clean up resources:

```c
void my_transport_destroy(my_transport_t* ctx)
{
    if (!ctx) {
        return;
    }

    /* Close all handles */
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    /* Free buffers */
    free(ctx->rx_buffer);
    ctx->rx_buffer = NULL;

    /* Zero out sensitive data */
    memset(ctx, 0, sizeof(*ctx));

    free(ctx);
}
```

### 4. Thread Safety

If using threads, protect shared state with mutexes:

```c
struct my_transport {
    int fd;
    pthread_mutex_t lock;
    /* ... */
};

static mbc_status_t my_transport_send(void* ctx, const uint8_t* buffer,
                                      size_t length, mbc_transport_io_t* out)
{
    my_transport_t* transport = (my_transport_t*)ctx;

    pthread_mutex_lock(&transport->lock);

    ssize_t written = write(transport->fd, buffer, length);

    pthread_mutex_unlock(&transport->lock);

    /* ... */
}
```

### 5. Timeout Handling

Implement timeouts using `now()` callback:

```c
static mbc_status_t receive_with_timeout(my_transport_t* transport,
                                         uint8_t* buffer, size_t capacity,
                                         uint32_t timeout_ms,
                                         mbc_transport_io_t* out)
{
    uint64_t start_time = transport->iface.now(transport);
    size_t total_received = 0;

    while (total_received < capacity) {
        mbc_transport_io_t io = {0};
        mbc_status_t status = mbc_transport_receive(&transport->iface,
                                                     buffer + total_received,
                                                     capacity - total_received,
                                                     &io);

        if (status != MBC_STATUS_OK) {
            return status;
        }

        total_received += io.processed;

        /* Check timeout */
        uint64_t elapsed = transport->iface.now(transport) - start_time;
        if (elapsed >= timeout_ms) {
            break; /* Timeout */
        }

        if (io.processed == 0) {
            /* No data yet - yield */
            transport->iface.yield(transport);
        }
    }

    if (out) {
        out->processed = total_received;
    }

    return MBC_STATUS_OK;
}
```

---

## Debugging

### Enable Verbose Logging

```c
static mbc_status_t my_transport_send(void* ctx, const uint8_t* buffer,
                                      size_t length, mbc_transport_io_t* out)
{
    #ifdef DEBUG_TRANSPORT
    printf("[TRANSPORT] send: %zu bytes\n", length);
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");
    #endif

    /* ... actual send logic ... */
}
```

### Use Wireshark/tcpdump

For TCP/IP transports:

```bash
# Capture Modbus TCP traffic
tcpdump -i any -w modbus.pcap port 502

# View in Wireshark
wireshark modbus.pcap
```

### Logic Analyzer for Serial

For UART/RS-485, use a logic analyzer (Saleae, Rigol, etc.) to inspect:
- Baud rate accuracy
- Start/stop bits
- Data bits
- CRC errors

---

## Performance Optimization

### 1. Zero-Copy I/O

Avoid intermediate buffers:

```c
/* ✅ GOOD: Direct I/O */
mbc_status_t send(void* ctx, const uint8_t* buffer, size_t length, ...)
{
    return write(fd, buffer, length); /* Direct write */
}

/* ❌ BAD: Unnecessary copy */
mbc_status_t send(void* ctx, const uint8_t* buffer, size_t length, ...)
{
    uint8_t temp[256];
    memcpy(temp, buffer, length); /* Wasteful copy */
    return write(fd, temp, length);
}
```

### 2. DMA for Embedded Systems

Use DMA for high-throughput scenarios:

```c
static mbc_status_t stm32_uart_send_dma(void* ctx, const uint8_t* buffer,
                                        size_t length, mbc_transport_io_t* out)
{
    stm32_uart_transport_t* uart = (stm32_uart_transport_t*)ctx;

    /* Start DMA transfer */
    HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(uart->huart,
                                                      (uint8_t*)buffer,
                                                      (uint16_t)length);

    if (status != HAL_OK) {
        return MBC_STATUS_IO_ERROR;
    }

    /* Poll for completion (or use interrupt callback) */
    while (uart->huart->gState != HAL_UART_STATE_READY) {
        /* Yield to other tasks */
    }

    if (out) {
        out->processed = length;
    }

    return MBC_STATUS_OK;
}
```

### 3. Batch Operations

Reduce system calls by batching:

```c
/* Instead of multiple small writes... */
write(fd, frame1, 8);
write(fd, frame2, 8);
write(fd, frame3, 8);

/* ...batch into one write: */
uint8_t batch[24];
memcpy(batch + 0, frame1, 8);
memcpy(batch + 8, frame2, 8);
memcpy(batch + 16, frame3, 8);
write(fd, batch, 24); /* Single syscall */
```

---

## Summary

| Aspect | Recommendation |
|--------|----------------|
| **Blocking** | Always non-blocking (return immediately) |
| **Error Handling** | Distinguish transient (OK + 0 bytes) from permanent (IO_ERROR) |
| **Timeouts** | Implement using `now()` callback |
| **Thread Safety** | Protect shared state with mutexes |
| **Testing** | Unit tests (mock), loopback, integration with real devices |
| **Performance** | Zero-copy, DMA, batching |

**Next Steps:**
1. Review existing implementations: `src/transport/posix_tcp.c`, `src/transport/mock.c`
2. Study transport interface: `include/modbuscore/runtime/dependencies.h`
3. Check example: `docs/transport_custom.md` (Portuguese quick guide)
4. Test with mock transport before hardware integration

Custom transports enable ModbusCore to run on any platform—from bare-metal microcontrollers to cloud-native Linux systems. 🚀
