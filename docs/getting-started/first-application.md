# First Application

Build a complete Modbus TCP client from scratch.

## Table of Contents

- [Overview](#overview)
- [Project Setup](#project-setup)
- [TCP Client](#modbus-tcp-client)
- [TCP Server](#modbus-tcp-server)
- [RTU Client](#modbus-rtu-client)
- [Error Handling](#error-handling)
- [Testing](#testing)

## Overview

This tutorial will guide you through creating production-ready Modbus applications.

**What You'll Learn:**
- ✅ Build TCP client and server
- ✅ Implement RTU communication
- ✅ Handle errors properly
- ✅ Test your application

**Time Required:** ~30 minutes

## Project Setup

### Create Project Structure

```bash
mkdir my_modbus_app
cd my_modbus_app
mkdir src build
```

### CMakeLists.txt

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_modbus_app C)

# C17 standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find ModbusCore
find_package(ModbusCore REQUIRED CONFIG)

# TCP Client
add_executable(tcp_client src/tcp_client.c)
target_link_libraries(tcp_client ModbusCore::modbuscore)

# TCP Server
add_executable(tcp_server src/tcp_server.c)
target_link_libraries(tcp_server ModbusCore::modbuscore)

# RTU Client
add_executable(rtu_client src/rtu_client.c)
target_link_libraries(rtu_client ModbusCore::modbuscore)
```

## Modbus TCP Client

### Complete Implementation

Create `src/tcp_client.c`:

```c
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>
#include <modbuscore/transport/tcp.h>
#include <modbuscore/diagnostics.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define closesocket close
#endif

int main(void) {
    mbc_status_t status;
    int sock = -1;

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "❌ WSAStartup failed\n");
        return 1;
    }
#endif

    // 1. Initialize diagnostics
    mbc_diag_config_t diag_cfg = {
        .enable_frame_tracing = true,
        .enable_error_logging = true,
        .enable_performance_stats = true
    };
    mbc_diagnostics_init(&diag_cfg);

    // 2. Create TCP connection
    printf("📡 Connecting to 127.0.0.1:502...\n");
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "❌ Failed to create socket\n");
        goto cleanup;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(502)
    };
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "❌ Connection failed\n");
        goto cleanup;
    }

    printf("✓ Connected successfully\n\n");

    // 3. Build Read Holding Registers request
    mbc_pdu_t req_pdu = {0};
    status = mbc_pdu_build_read_holding_request(&req_pdu, 1, 0, 10);
    if (status != MBC_STATUS_OK) {
        fprintf(stderr, "❌ Failed to build PDU: %d\n", status);
        goto cleanup;
    }

    printf("📦 Request PDU:\n");
    printf("   Function: 0x%02X (Read Holding Registers)\n", req_pdu.function);
    printf("   Address: %u\n", 0);
    printf("   Count: %u\n", 10);

    // 4. Encode MBAP frame
    uint8_t tx_buffer[260];
    size_t tx_length = 0;
    
    mbc_mbap_header_t mbap_hdr = {
        .transaction_id = 1,
        .protocol_id = 0,
        .unit_id = 1,
        .length = (uint16_t)(req_pdu.data_length + 1)
    };

    status = mbc_mbap_encode(&mbap_hdr, &req_pdu, tx_buffer, sizeof(tx_buffer), &tx_length);
    if (status != MBC_STATUS_OK) {
        fprintf(stderr, "❌ Failed to encode MBAP: %d\n", status);
        goto cleanup;
    }

    printf("\n📤 Sending %zu bytes...\n", tx_length);
    
    // Trace outgoing frame
    mbc_diagnostics_trace_frame(true, tx_buffer, tx_length);

    // 5. Send request
    ssize_t sent = send(sock, (const char*)tx_buffer, (int)tx_length, 0);
    if (sent != (ssize_t)tx_length) {
        fprintf(stderr, "❌ Send failed\n");
        goto cleanup;
    }

    // 6. Receive response
    uint8_t rx_buffer[260];
    ssize_t received = recv(sock, (char*)rx_buffer, sizeof(rx_buffer), 0);
    if (received <= 0) {
        fprintf(stderr, "❌ Receive failed\n");
        goto cleanup;
    }

    printf("\n📥 Received %zd bytes\n", received);
    
    // Trace incoming frame
    mbc_diagnostics_trace_frame(false, rx_buffer, (size_t)received);

    // 7. Decode MBAP response
    mbc_mbap_header_t resp_mbap = {0};
    mbc_pdu_t resp_pdu = {0};
    
    status = mbc_mbap_decode(rx_buffer, (size_t)received, &resp_mbap, &resp_pdu);
    if (status != MBC_STATUS_OK) {
        fprintf(stderr, "❌ Failed to decode MBAP: %d\n", status);
        goto cleanup;
    }

    printf("\n📦 Response MBAP:\n");
    printf("   Transaction ID: %u\n", resp_mbap.transaction_id);
    printf("   Unit ID: %u\n", resp_mbap.unit_id);

    // 8. Parse response data
    if (resp_pdu.function == 0x03) {  // Read Holding Registers
        uint8_t byte_count = resp_pdu.data[0];
        uint16_t num_registers = byte_count / 2;

        printf("\n📊 Register Values:\n");
        for (uint16_t i = 0; i < num_registers; i++) {
            uint16_t value = ((uint16_t)resp_pdu.data[1 + i*2] << 8) | 
                             resp_pdu.data[2 + i*2];
            printf("   Register %u: %u (0x%04X)\n", i, value, value);
        }
    } else if (resp_pdu.function & 0x80) {  // Exception response
        printf("❌ Modbus Exception: 0x%02X\n", resp_pdu.data[0]);
        goto cleanup;
    }

    // 9. Print statistics
    printf("\n📈 Statistics:\n");
    printf("   Total requests: %u\n", mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_TX_FRAMES));
    printf("   Total responses: %u\n", mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_RX_FRAMES));
    printf("   Errors: %u\n", mbc_diagnostics_get_counter(MBC_DIAG_COUNTER_ERRORS));

    printf("\n✅ Transaction completed successfully!\n");

cleanup:
    if (sock >= 0) {
        closesocket(sock);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
```

### Build and Run

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run (ensure a Modbus TCP server is running on port 502)
./build/tcp_client
```

**Expected Output:**
```
📡 Connecting to 127.0.0.1:502...
✓ Connected successfully

📦 Request PDU:
   Function: 0x03 (Read Holding Registers)
   Address: 0
   Count: 10

📤 Sending 12 bytes...

📥 Received 27 bytes

📦 Response MBAP:
   Transaction ID: 1
   Unit ID: 1

📊 Register Values:
   Register 0: 100 (0x0064)
   Register 1: 200 (0x00C8)
   ...

📈 Statistics:
   Total requests: 1
   Total responses: 1
   Errors: 0

✅ Transaction completed successfully!
```

## Modbus TCP Server

Create `src/tcp_server.c`:

```c
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #define closesocket close
#endif

// Simulated register map
static uint16_t holding_registers[100] = {0};

static mbc_status_t process_read_holding(const mbc_pdu_t *req, mbc_pdu_t *resp) {
    // Parse request
    uint16_t start_addr = ((uint16_t)req->data[0] << 8) | req->data[1];
    uint16_t count = ((uint16_t)req->data[2] << 8) | req->data[3];

    // Validate
    if (start_addr + count > 100) {
        resp->function = req->function | 0x80;  // Exception
        resp->data[0] = 0x02;  // Illegal data address
        resp->data_length = 1;
        return MBC_STATUS_OK;
    }

    // Build response
    resp->function = 0x03;
    resp->data[0] = (uint8_t)(count * 2);  // Byte count
    
    for (uint16_t i = 0; i < count; i++) {
        uint16_t value = holding_registers[start_addr + i];
        resp->data[1 + i*2] = (uint8_t)(value >> 8);
        resp->data[2 + i*2] = (uint8_t)(value & 0xFF);
    }
    
    resp->data_length = 1 + count * 2;
    return MBC_STATUS_OK;
}

int main(void) {
    int server_sock = -1;

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    // Initialize register map with test data
    for (int i = 0; i < 100; i++) {
        holding_registers[i] = i * 10;
    }

    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        fprintf(stderr, "❌ Failed to create socket\n");
        return 1;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // Bind
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(502)
    };

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "❌ Bind failed (try: sudo ./tcp_server)\n");
        goto cleanup;
    }

    // Listen
    if (listen(server_sock, 5) < 0) {
        fprintf(stderr, "❌ Listen failed\n");
        goto cleanup;
    }

    printf("🚀 Modbus TCP Server listening on port 502\n");
    printf("   Press Ctrl+C to stop\n\n");

    // Accept loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;

        printf("📡 Client connected\n");

        // Handle client
        uint8_t rx_buffer[260];
        ssize_t received = recv(client_sock, (char*)rx_buffer, sizeof(rx_buffer), 0);
        
        if (received > 0) {
            // Decode request
            mbc_mbap_header_t req_mbap;
            mbc_pdu_t req_pdu;
            
            if (mbc_mbap_decode(rx_buffer, (size_t)received, &req_mbap, &req_pdu) == MBC_STATUS_OK) {
                printf("   RX: Function 0x%02X\n", req_pdu.function);

                // Process
                mbc_pdu_t resp_pdu = {0};
                if (req_pdu.function == 0x03) {
                    process_read_holding(&req_pdu, &resp_pdu);
                } else {
                    resp_pdu.function = req_pdu.function | 0x80;
                    resp_pdu.data[0] = 0x01;  // Illegal function
                    resp_pdu.data_length = 1;
                }

                // Encode response
                uint8_t tx_buffer[260];
                size_t tx_length;
                
                mbc_mbap_header_t resp_mbap = req_mbap;
                resp_mbap.length = (uint16_t)(resp_pdu.data_length + 1);

                if (mbc_mbap_encode(&resp_mbap, &resp_pdu, tx_buffer, sizeof(tx_buffer), &tx_length) == MBC_STATUS_OK) {
                    send(client_sock, (const char*)tx_buffer, (int)tx_length, 0);
                    printf("   TX: %zu bytes\n", tx_length);
                }
            }
        }

        closesocket(client_sock);
        printf("   Client disconnected\n\n");
    }

cleanup:
    if (server_sock >= 0) closesocket(server_sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
```

### Run Server

```bash
# Linux/macOS (requires sudo for port 502)
sudo ./build/tcp_server

# Windows (run as Administrator)
.\build\tcp_server.exe
```

## Modbus RTU Client

Create `src/rtu_client.c`:

```c
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/transport/rtu.h>
#include <stdio.h>

#ifndef _WIN32
    #include <fcntl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

int main(void) {
#ifndef _WIN32
    // Open serial port
    int fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
    if (fd < 0) {
        fprintf(stderr, "❌ Failed to open /dev/ttyUSB0\n");
        return 1;
    }

    // Configure serial port
    struct termios tty = {0};
    tcgetattr(fd, &tty);
    
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit
    tty.c_cflag |= (CLOCAL | CREAD);              // Enable receiver
    tty.c_cflag &= ~PARENB;                       // No parity
    tty.c_cflag &= ~CSTOPB;                       // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;                      // No hardware flow control

    tcsetattr(fd, TCSANOW, &tty);

    printf("📡 Serial port /dev/ttyUSB0 opened (9600 8N1)\n");

    // Build request
    mbc_pdu_t req_pdu = {0};
    mbc_pdu_build_read_holding_request(&req_pdu, 1, 0, 10);

    // Encode RTU frame
    uint8_t tx_buffer[256];
    size_t tx_length;
    
    if (mbc_rtu_encode(1, &req_pdu, tx_buffer, sizeof(tx_buffer), &tx_length) == MBC_STATUS_OK) {
        printf("📤 Sending %zu bytes (including CRC)...\n", tx_length);
        
        write(fd, tx_buffer, tx_length);
        
        // Wait for response
        usleep(100000);  // 100ms
        
        uint8_t rx_buffer[256];
        ssize_t received = read(fd, rx_buffer, sizeof(rx_buffer));
        
        if (received > 0) {
            printf("📥 Received %zd bytes\n", received);
            
            uint8_t slave_addr;
            mbc_pdu_t resp_pdu;
            
            if (mbc_rtu_decode(rx_buffer, (size_t)received, &slave_addr, &resp_pdu) == MBC_STATUS_OK) {
                printf("✅ Valid RTU frame received!\n");
            }
        }
    }

    close(fd);
#else
    printf("⚠️  RTU example requires POSIX (Linux/macOS)\n");
#endif
    
    return 0;
}
```

## Error Handling

### Proper Error Checking

```c
mbc_status_t status;

// Always check return values
status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
if (status != MBC_STATUS_OK) {
    switch (status) {
        case MBC_STATUS_INVALID_PARAMETER:
            fprintf(stderr, "Invalid parameter\n");
            break;
        case MBC_STATUS_BUFFER_TOO_SMALL:
            fprintf(stderr, "Buffer too small\n");
            break;
        default:
            fprintf(stderr, "Error: %d\n", status);
    }
    return 1;
}
```

### Timeout Handling

```c
#ifndef _WIN32
#include <sys/select.h>

// Set socket timeout
struct timeval tv = {
    .tv_sec = 5,   // 5 seconds
    .tv_usec = 0
};
setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

// Or use select()
fd_set readfds;
FD_ZERO(&readfds);
FD_SET(sock, &readfds);

if (select(sock + 1, &readfds, NULL, NULL, &tv) > 0) {
    // Data available
    recv(sock, buffer, sizeof(buffer), 0);
} else {
    fprintf(stderr, "Timeout\n");
}
#endif
```

## Testing

### Unit Test Your Application

Create `tests/test_client.c`:

```c
#include <modbuscore/protocol/pdu.h>
#include <assert.h>
#include <stdio.h>

void test_request_building(void) {
    mbc_pdu_t pdu = {0};
    mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    assert(status == MBC_STATUS_OK);
    assert(pdu.function == 0x03);
    printf("✓ test_request_building passed\n");
}

int main(void) {
    test_request_building();
    printf("\n✅ All tests passed!\n");
    return 0;
}
```

Add to `CMakeLists.txt`:
```cmake
enable_testing()

add_executable(test_client tests/test_client.c)
target_link_libraries(test_client ModbusCore::modbuscore)
add_test(NAME test_client COMMAND test_client)
```

Run tests:
```bash
ctest --test-dir build --output-on-failure
```

### Integration Test

Test client against server:

```bash
# Terminal 1: Start server
sudo ./build/tcp_server

# Terminal 2: Run client
./build/tcp_client
```

## Next Steps

- [Examples](examples.md) - More complete examples
- [Architecture](../user-guide/architecture.md) - How ModbusCore works
- [API Reference](../api/README.md) - Complete API docs

---

**Prev**: [Quick Start ←](quick-start.md) | **Next**: [Examples →](examples.md)
