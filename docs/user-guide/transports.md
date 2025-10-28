# Transports

Deep dive into ModbusCore transport layers: TCP, RTU, and custom implementations.

## Table of Contents

- [Overview](#overview)
- [TCP Transport](#tcp-transport)
- [RTU Transport](#rtu-transport)
- [Transport Comparison](#transport-comparison)
- [Custom Transports](#custom-transports)
- [Best Practices](#best-practices)

## Overview

ModbusCore supports multiple transport layers:

| Transport | Physical | Use Case | Frame Format |
|-----------|----------|----------|--------------|
| **TCP** | Ethernet | Industrial networks, SCADA | MBAP header + PDU |
| **RTU** | RS-232/485 | Field devices, PLCs | Slave + PDU + CRC |
| **Custom** | Any | UDP, CAN, LoRa, etc. | User-defined |

## TCP Transport

### MBAP Frame Format

Modbus Application Protocol (MBAP) frame structure:

```
┌─────────────┬──────────┬────────┬─────────┬──────────┬─────────┐
│ Transaction │ Protocol │ Length │ Unit ID │ Function │  Data   │
│     ID      │    ID    │        │         │   Code   │         │
├─────────────┼──────────┼────────┼─────────┼──────────┼─────────┤
│   2 bytes   │ 2 bytes  │2 bytes │ 1 byte  │  1 byte  │ N bytes │
└─────────────┴──────────┴────────┴─────────┴──────────┴─────────┘
     Big-endian  Always 0  PDU+1    Slave     Modbus    Request/
                                      addr     function  Response
```

**Example Frame:**
```
00 01 00 00 00 06 01 03 00 00 00 0A
│  │  │  │  │  │  │  │  │     │  │
│  │  │  │  │  │  │  │  │     └──┴─ Count: 10 registers
│  │  │  │  │  │  │  │  └────────── Start address: 0
│  │  │  │  │  │  │  └───────────── Function: Read Holding (0x03)
│  │  │  │  │  │  └──────────────── Unit ID: 1
│  │  │  │  │  └─────────────────── Length: 6 (1+5 PDU bytes)
│  │  │  └──┴────────────────────── Protocol ID: 0 (Modbus)
│  └──┴───────────────────────────── Transaction ID: 1
```

### TCP Implementation

#### Basic TCP Client

```c
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int tcp_connect(const char *ip, uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

mbc_status_t tcp_send_request(int sock, uint16_t txid, const mbc_pdu_t *pdu) {
    // Encode MBAP frame
    mbc_mbap_header_t hdr = {
        .transaction_id = txid,
        .protocol_id = 0,
        .unit_id = 1,
        .length = (uint16_t)(pdu->data_length + 1)
    };

    uint8_t buffer[260];
    size_t len;
    
    if (mbc_mbap_encode(&hdr, pdu, buffer, sizeof(buffer), &len) != MBC_STATUS_OK) {
        return MBC_STATUS_ENCODE_ERROR;
    }

    // Send
    if (send(sock, buffer, len, 0) != (ssize_t)len) {
        return MBC_STATUS_IO_ERROR;
    }

    return MBC_STATUS_OK;
}

mbc_status_t tcp_receive_response(int sock, mbc_pdu_t *pdu) {
    uint8_t buffer[260];
    ssize_t received = recv(sock, buffer, sizeof(buffer), 0);
    
    if (received <= 0) {
        return MBC_STATUS_IO_ERROR;
    }

    mbc_mbap_header_t hdr;
    return mbc_mbap_decode(buffer, (size_t)received, &hdr, pdu);
}
```

#### TCP Server

```c
int tcp_listen(uint16_t port) {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };

    bind(server, (struct sockaddr*)&addr, sizeof(addr));
    listen(server, 5);

    return server;
}

void tcp_server_loop(int server_sock, mbc_request_handler_t handler) {
    while (1) {
        int client = accept(server_sock, NULL, NULL);
        if (client < 0) continue;

        uint8_t rx_buf[260];
        ssize_t received = recv(client, rx_buf, sizeof(rx_buf), 0);
        
        if (received > 0) {
            mbc_mbap_header_t req_hdr;
            mbc_pdu_t req_pdu;
            
            if (mbc_mbap_decode(rx_buf, received, &req_hdr, &req_pdu) == MBC_STATUS_OK) {
                // Process request
                mbc_pdu_t resp_pdu = {0};
                handler(&req_pdu, &resp_pdu);

                // Send response
                uint8_t tx_buf[260];
                size_t tx_len;
                
                mbc_mbap_header_t resp_hdr = req_hdr;
                resp_hdr.length = (uint16_t)(resp_pdu.data_length + 1);

                if (mbc_mbap_encode(&resp_hdr, &resp_pdu, tx_buf, sizeof(tx_buf), &tx_len) == MBC_STATUS_OK) {
                    send(client, tx_buf, tx_len, 0);
                }
            }
        }

        close(client);
    }
}
```

### TCP Features

#### Keep-Alive

```c
// Enable TCP keep-alive
int opt = 1;
setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

#ifdef __linux__
// Linux-specific keep-alive tuning
int idle = 60;      // Start probing after 60s idle
int interval = 10;  // Probe every 10s
int count = 3;      // Drop after 3 failed probes

setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
```

#### Non-Blocking I/O

```c
#include <fcntl.h>

// Set non-blocking mode
int flags = fcntl(sock, F_GETFL, 0);
fcntl(sock, F_SETFL, flags | O_NONBLOCK);

// Use with select/poll/epoll
fd_set readfds;
FD_ZERO(&readfds);
FD_SET(sock, &readfds);

struct timeval tv = {.tv_sec = 5, .tv_usec = 0};

if (select(sock + 1, &readfds, NULL, NULL, &tv) > 0) {
    // Data available
    recv(sock, buffer, sizeof(buffer), 0);
}
```

#### Connection Timeout

```c
#include <sys/select.h>

int tcp_connect_timeout(const char *ip, uint16_t port, int timeout_sec) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    // Set non-blocking
    fcntl(sock, F_SETFL, O_NONBLOCK);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    inet_pton(AF_INET, ip, &addr.sin_addr);

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    // Wait for connection
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);

    struct timeval tv = {.tv_sec = timeout_sec, .tv_usec = 0};

    if (select(sock + 1, NULL, &writefds, NULL, &tv) <= 0) {
        close(sock);
        return -1;  // Timeout
    }

    // Check connection status
    int error;
    socklen_t len = sizeof(error);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);

    if (error != 0) {
        close(sock);
        return -1;
    }

    // Set back to blocking
    fcntl(sock, F_SETFL, 0);

    return sock;
}
```

## RTU Transport

### RTU Frame Format

Remote Terminal Unit (RTU) frame structure:

```
┌───────────┬──────────┬─────────┬─────────────┐
│ Slave ID  │ Function │  Data   │  CRC-16     │
│           │   Code   │         │  (Modbus)   │
├───────────┼──────────┼─────────┼─────────────┤
│  1 byte   │  1 byte  │ N bytes │  2 bytes    │
└───────────┴──────────┴─────────┴─────────────┘
   Device     Modbus    Request/   Error
   address    function  Response   detection
```

**Example Frame:**
```
01 03 00 00 00 0A C5 CD
│  │  │     │  │  │  │
│  │  │     │  │  └──┴─ CRC-16: 0xC5CD (little-endian)
│  │  │     └──┴─────── Count: 10 registers
│  │  └────────────────── Start address: 0
│  └───────────────────── Function: Read Holding (0x03)
└──────────────────────── Slave ID: 1
```

### RTU Implementation

#### Serial Port Configuration

```c
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

int rtu_open_serial(const char *port, int baudrate) {
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0) return -1;

    struct termios tty = {0};
    tcgetattr(fd, &tty);

    // Baud rate
    speed_t speed;
    switch (baudrate) {
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 115200: speed = B115200; break;
        default:     speed = B9600;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8N1 (8 data bits, no parity, 1 stop bit)
    tty.c_cflag &= ~PARENB;        // No parity
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            // 8 data bits

    // No hardware flow control
    tty.c_cflag &= ~CRTSCTS;

    // Enable receiver, ignore modem control lines
    tty.c_cflag |= (CLOCAL | CREAD);

    // Raw mode (no echo, no canonical processing)
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    // Read timeout: 100ms
    tty.c_cc[VTIME] = 1;
    tty.c_cc[VMIN] = 0;

    tcsetattr(fd, TCSANOW, &tty);

    return fd;
}
```

#### RTU Send/Receive

```c
mbc_status_t rtu_send_request(int fd, uint8_t slave, const mbc_pdu_t *pdu) {
    uint8_t buffer[256];
    size_t len;

    // Encode RTU frame
    if (mbc_rtu_encode(slave, pdu, buffer, sizeof(buffer), &len) != MBC_STATUS_OK) {
        return MBC_STATUS_ENCODE_ERROR;
    }

    // Calculate inter-frame delay (3.5 character times)
    // At 9600 baud: ~3.6ms
    usleep(4000);

    // Send
    if (write(fd, buffer, len) != (ssize_t)len) {
        return MBC_STATUS_IO_ERROR;
    }

    return MBC_STATUS_OK;
}

mbc_status_t rtu_receive_response(int fd, uint8_t *slave, mbc_pdu_t *pdu) {
    uint8_t buffer[256];
    size_t total = 0;
    
    // Read with timeout
    while (total < sizeof(buffer)) {
        ssize_t n = read(fd, buffer + total, sizeof(buffer) - total);
        if (n <= 0) break;
        total += n;
        
        // Check for frame gap (3.5 char times)
        usleep(4000);
        
        // No more data?
        if (read(fd, buffer + total, 1) <= 0) break;
    }

    if (total == 0) {
        return MBC_STATUS_TIMEOUT;
    }

    // Decode RTU frame
    return mbc_rtu_decode(buffer, total, slave, pdu);
}
```

#### Timing Calculations

```c
// Calculate character time in microseconds
uint32_t rtu_char_time_us(int baudrate) {
    // 1 character = 1 start + 8 data + 1 parity + 1 stop = 11 bits
    return (11 * 1000000) / baudrate;
}

// Calculate inter-frame delay (3.5 character times)
uint32_t rtu_inter_frame_delay_us(int baudrate) {
    uint32_t char_time = rtu_char_time_us(baudrate);
    
    // Minimum 1750us for high baud rates (>19200)
    if (baudrate > 19200) {
        return 1750;
    }
    
    return char_time * 35 / 10;  // 3.5 characters
}

// Example usage
int baudrate = 9600;
uint32_t delay = rtu_inter_frame_delay_us(baudrate);
usleep(delay);  // ~3600us at 9600 baud
```

### RTU Advanced Features

#### Half-Duplex RS-485

```c
// GPIO control for RS-485 DE/RE pins
void rs485_set_direction(bool transmit) {
    if (transmit) {
        // Enable driver (DE=1, RE=1)
        gpio_set_level(GPIO_RS485_DE, 1);
    } else {
        // Enable receiver (DE=0, RE=0)
        gpio_set_level(GPIO_RS485_DE, 0);
    }
}

mbc_status_t rtu_send_rs485(int fd, uint8_t slave, const mbc_pdu_t *pdu) {
    uint8_t buffer[256];
    size_t len;

    mbc_rtu_encode(slave, pdu, buffer, sizeof(buffer), &len);

    rs485_set_direction(true);   // TX mode
    write(fd, buffer, len);
    tcdrain(fd);                 // Wait for TX complete
    rs485_set_direction(false);  // RX mode

    return MBC_STATUS_OK;
}
```

#### Broadcast Messages

```c
// Slave address 0 = broadcast (no response expected)
mbc_pdu_t pdu = {0};
mbc_pdu_build_write_single_request(&pdu, 1, 5, 100);

// Send to all slaves
rtu_send_request(fd, 0, &pdu);

// No response for broadcasts!
```

## Transport Comparison

### Performance

| Metric | TCP | RTU (9600) | RTU (115200) |
|--------|-----|------------|--------------|
| **Latency** | 1-10ms | 100-500ms | 10-50ms |
| **Throughput** | 10+ MB/s | 960 B/s | 11.5 KB/s |
| **Overhead** | 7 bytes | 3 bytes | 3 bytes |
| **Max Distance** | Unlimited | 1200m | 1200m |
| **Topology** | Star/Mesh | Bus | Bus |

### Reliability

| Feature | TCP | RTU |
|---------|-----|-----|
| **Error Detection** | TCP checksum | CRC-16 |
| **Retransmission** | Automatic (TCP) | Manual |
| **Flow Control** | TCP windowing | None |
| **Connection State** | Stateful | Stateless |

### Use Cases

**Use TCP when:**
- ✅ Ethernet infrastructure available
- ✅ High throughput required
- ✅ Long distances (via routers)
- ✅ Multiple simultaneous connections
- ✅ Complex topologies

**Use RTU when:**
- ✅ RS-232/485 infrastructure
- ✅ Field devices (PLCs, sensors)
- ✅ Deterministic timing required
- ✅ Simple point-to-point or bus
- ✅ Low power consumption

## Custom Transports

### Custom Transport Interface

```c
typedef struct {
    mbc_status_t (*send)(void *ctx, const uint8_t *data, size_t len);
    mbc_status_t (*receive)(void *ctx, uint8_t *buffer, size_t max_len, size_t *received);
    void *user_context;
} mbc_custom_transport_t;
```

### Example: UDP Transport

```c
typedef struct {
    int socket;
    struct sockaddr_in remote_addr;
} udp_context_t;

mbc_status_t udp_send(void *ctx, const uint8_t *data, size_t len) {
    udp_context_t *udp = ctx;
    
    ssize_t sent = sendto(udp->socket, data, len, 0,
                          (struct sockaddr*)&udp->remote_addr,
                          sizeof(udp->remote_addr));
    
    return (sent == (ssize_t)len) ? MBC_STATUS_OK : MBC_STATUS_IO_ERROR;
}

mbc_status_t udp_receive(void *ctx, uint8_t *buffer, size_t max_len, size_t *received) {
    udp_context_t *udp = ctx;
    
    ssize_t n = recvfrom(udp->socket, buffer, max_len, 0, NULL, NULL);
    if (n < 0) return MBC_STATUS_IO_ERROR;
    
    *received = (size_t)n;
    return MBC_STATUS_OK;
}

// Usage
udp_context_t udp_ctx = {
    .socket = socket(AF_INET, SOCK_DGRAM, 0),
    .remote_addr = {.sin_family = AF_INET, .sin_port = htons(502)}
};

mbc_custom_transport_t transport = {
    .send = udp_send,
    .receive = udp_receive,
    .user_context = &udp_ctx
};
```

### Example: CAN Bus Transport

```c
#include <linux/can.h>

typedef struct {
    int socket;
    uint32_t tx_id;
    uint32_t rx_id;
} can_context_t;

mbc_status_t can_send(void *ctx, const uint8_t *data, size_t len) {
    can_context_t *can = ctx;
    struct can_frame frame = {0};
    
    frame.can_id = can->tx_id;
    frame.can_dlc = (len > 8) ? 8 : len;
    memcpy(frame.data, data, frame.can_dlc);
    
    if (write(can->socket, &frame, sizeof(frame)) != sizeof(frame)) {
        return MBC_STATUS_IO_ERROR;
    }
    
    // Send remaining bytes in next frame
    if (len > 8) {
        return can_send(ctx, data + 8, len - 8);
    }
    
    return MBC_STATUS_OK;
}
```

## Best Practices

### TCP Best Practices

1. **Always check connection state:**
   ```c
   if (send(sock, data, len, 0) < 0) {
       // Connection lost, reconnect
       close(sock);
       sock = tcp_connect(ip, port);
   }
   ```

2. **Use timeouts:**
   ```c
   struct timeval tv = {.tv_sec = 5};
   setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
   ```

3. **Handle partial reads:**
   ```c
   size_t total = 0;
   while (total < expected_len) {
       ssize_t n = recv(sock, buffer + total, expected_len - total, 0);
       if (n <= 0) break;
       total += n;
   }
   ```

### RTU Best Practices

1. **Respect inter-frame delays:**
   ```c
   usleep(rtu_inter_frame_delay_us(baudrate));
   ```

2. **Flush buffers before receiving:**
   ```c
   tcflush(fd, TCIFLUSH);  // Clear input buffer
   ```

3. **Validate CRC:**
   ```c
   uint8_t slave;
   mbc_pdu_t pdu;
   if (mbc_rtu_decode(frame, len, &slave, &pdu) != MBC_STATUS_OK) {
       // CRC error, retry
   }
   ```

4. **Use appropriate timeouts:**
   ```c
   // Timeout = RTU frame time + processing time
   uint32_t timeout_ms = (rtu_char_time_us(baudrate) * 256) / 1000 + 100;
   ```

## Next Steps

- [Protocol Engine](protocol-engine.md) - PDU/MBAP/RTU details
- [Runtime](runtime.md) - Transaction management
- [Custom Transports](../advanced/custom-transports.md) - Build your own

---

**Prev**: [Architecture ←](architecture.md) | **Next**: [Protocol Engine →](protocol-engine.md)
