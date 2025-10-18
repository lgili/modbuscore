---
layout: default
title: Quick Start Guide
nav_order: 3
---

# Quick Start Guide
{: .no_toc }

Get up and running with ModbusCore v3.0 in minutes
{: .fs-6 .fw-300 }

## Table of Contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Installation

### Prerequisites

- **C11 Compiler** – GCC 7+, Clang 10+, or MSVC 2019+
- **CMake** 3.15 or higher
- **Python 3** (optional, for running test servers)

### Method 1: Clone and Build

```bash
# Clone the repository
git clone https://github.com/lgili/modbuscore.git
cd modbuscore

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build library and examples
make

# Run tests (optional)
ctest --output-on-failure
```

### Method 2: Add as CMake Submodule

```bash
# In your project directory
git submodule add https://github.com/lgili/modbuscore.git external/modbuscore
git submodule update --init --recursive
```

Then in your `CMakeLists.txt`:

```cmake
add_subdirectory(external/modbuscore)
target_link_libraries(your_app PRIVATE modbuscore)
```

### Method 3: CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    modbuscore
    GIT_REPOSITORY https://github.com/lgili/modbuscore.git
    GIT_TAG        main  # or specific version tag
)

FetchContent_MakeAvailable(modbuscore)

target_link_libraries(your_app PRIVATE modbuscore)
```

---

## Your First Modbus Client

Let's create a simple Modbus TCP client that reads holding registers!

### Step 1: Create `my_modbus_client.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <modbuscore/transport/posix_tcp.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 502
#define UNIT_ID 1

int main(void)
{
    printf("=== My First Modbus Client ===\n\n");

    // 1. Create TCP transport
    mbc_posix_tcp_config_t tcp_config = {
        .host = SERVER_HOST,
        .port = SERVER_PORT,
        .connect_timeout_ms = 5000,
        .recv_timeout_ms = 1000,
    };

    mbc_transport_iface_t transport;
    mbc_posix_tcp_ctx_t *tcp_ctx = NULL;

    if (!mbc_status_is_ok(mbc_posix_tcp_create(&tcp_config, &transport, &tcp_ctx))) {
        fprintf(stderr, "Failed to connect to %s:%u\n", SERVER_HOST, SERVER_PORT);
        return 1;
    }
    printf("✓ Connected to server\n");

    // 2. Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    if (!mbc_status_is_ok(mbc_runtime_builder_build(&builder, &runtime))) {
        fprintf(stderr, "Failed to build runtime\n");
        mbc_posix_tcp_destroy(tcp_ctx);
        return 1;
    }
    printf("✓ Runtime initialized\n");

    // 3. Initialize protocol engine
    mbc_engine_t engine;
    mbc_engine_config_t engine_config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .response_timeout_ms = 3000,
    };

    if (!mbc_status_is_ok(mbc_engine_init(&engine, &engine_config))) {
        fprintf(stderr, "Failed to initialize engine\n");
        mbc_runtime_shutdown(&runtime);
        mbc_posix_tcp_destroy(tcp_ctx);
        return 1;
    }
    printf("✓ Engine ready\n\n");

    // 4. Build FC03 request (Read 10 holding registers starting at address 0)
    mbc_pdu_t request_pdu;
    mbc_pdu_build_read_holding_request(&request_pdu, UNIT_ID, 0, 10);

    // 5. Serialize PDU to bytes
    uint8_t pdu_buffer[256];
    pdu_buffer[0] = request_pdu.function;
    memcpy(&pdu_buffer[1], request_pdu.payload, request_pdu.payload_length);
    size_t pdu_length = 1 + request_pdu.payload_length;

    // 6. Wrap with MBAP header
    mbc_mbap_header_t mbap_header = {
        .transaction_id = 1,
        .protocol_id = 0,
        .unit_id = UNIT_ID
    };

    uint8_t request_buffer[256];
    size_t request_length = 0;
    mbc_mbap_encode(&mbap_header, pdu_buffer, pdu_length,
                    request_buffer, sizeof(request_buffer), &request_length);

    printf("Sending request...\n");
    mbc_engine_submit_request(&engine, request_buffer, request_length);

    // Small delay to ensure transmission
    usleep(10000);

    // 7. Poll for response
    printf("Waiting for response...\n");
    bool success = false;

    for (int i = 0; i < 100; ++i) {
        mbc_status_t status = mbc_engine_step(&engine, 10);

        if (status == MBC_STATUS_TIMEOUT) {
            fprintf(stderr, "✗ Timeout\n");
            break;
        }

        mbc_pdu_t response_pdu;
        if (mbc_engine_take_pdu(&engine, &response_pdu)) {
            printf("✓ Response received!\n\n");

            // Check for exception
            if (response_pdu.function & 0x80U) {
                uint8_t exception_code;
                mbc_pdu_parse_exception(&response_pdu, NULL, &exception_code);
                fprintf(stderr, "✗ Server exception: 0x%02X\n", exception_code);
                break;
            }

            // Parse response
            const uint8_t *data;
            size_t count;
            mbc_pdu_parse_read_holding_response(&response_pdu, &data, &count);

            printf("Registers:\n");
            for (size_t i = 0; i < count; ++i) {
                uint16_t value = (data[i * 2] << 8) | data[i * 2 + 1];
                printf("  [%zu] = %u (0x%04X)\n", i, value, value);
            }

            success = true;
            break;
        }
    }

    // 8. Cleanup
    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(tcp_ctx);

    printf("\n%s\n", success ? "=== SUCCESS ===" : "=== FAILED ===");
    return success ? 0 : 1;
}
```

### Step 2: Compile

```bash
gcc my_modbus_client.c \
    -I/path/to/modbuscore/include \
    -L/path/to/modbuscore/build \
    -lmodbuscore \
    -o my_client
```

Or with CMake:

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(my_modbus_app C)

find_package(modbuscore REQUIRED)

add_executable(my_client my_modbus_client.c)
target_link_libraries(my_client PRIVATE modbuscore)
```

### Step 3: Run a Test Server

```bash
# In the modbuscore/tests directory
python3 simple_tcp_server.py
```

Or use any Modbus TCP server/simulator.

### Step 4: Run Your Client

```bash
./my_client
```

**Expected Output:**
```
=== My First Modbus Client ===

✓ Connected to server
✓ Runtime initialized
✓ Engine ready

Sending request...
Waiting for response...
✓ Response received!

Registers:
  [0] = 0 (0x0000)
  [1] = 1 (0x0001)
  [2] = 2 (0x0002)
  [3] = 3 (0x0003)
  [4] = 4 (0x0004)
  [5] = 5 (0x0005)
  [6] = 6 (0x0006)
  [7] = 7 (0x0007)
  [8] = 8 (0x0008)
  [9] = 9 (0x0009)

=== SUCCESS ===
```

---

## Common Patterns

### Reading Multiple Register Ranges

```c
// Read registers 0-9
read_registers(&engine, 0, 10);

// Read registers 100-109
read_registers(&engine, 100, 10);

// Helper function
void read_registers(mbc_engine_t *engine, uint16_t start, uint16_t count)
{
    mbc_pdu_t request;
    mbc_pdu_build_read_holding_request(&request, UNIT_ID, start, count);

    // ... encode, send, receive (as above)
}
```

### Writing Single Register (FC06)

```c
// Build FC06 request
mbc_pdu_t request;
mbc_pdu_build_write_single_register(&request, UNIT_ID, 100, 1234);

// Serialize and send (same as FC03)
// ...

// Parse FC06 response
uint16_t address, value;
mbc_pdu_parse_write_single_response(&response, &address, &value);
printf("Wrote %u to register %u\n", value, address);
```

### Writing Multiple Registers (FC16)

```c
// Prepare data
uint16_t values[] = {100, 200, 300, 400, 500};

// Build FC16 request
mbc_pdu_t request;
mbc_pdu_build_write_multiple_registers(&request, UNIT_ID, 0, values, 5);

// Serialize and send
// ...

// Parse FC16 response
uint16_t start_address, quantity;
mbc_pdu_parse_write_multiple_response(&response, &start_address, &quantity);
printf("Wrote %u registers starting at %u\n", quantity, start_address);
```

---

## Testing with Different Servers

### Option 1: Python Simple Server (Included)

```bash
cd tests
python3 simple_tcp_server.py
```

Listens on `127.0.0.1:5502`

### Option 2: pymodbus Server

```bash
pip install pymodbus

# Run modbus server
pymodbus.server --host 0.0.0.0 --port 502
```

### Option 3: Docker Modbus Simulator

```bash
docker run -d -p 502:502 oitc/modbus-server
```

### Option 4: Hardware PLC/RTU

Change connection settings:

```c
mbc_posix_tcp_config_t tcp_config = {
    .host = "192.168.1.100",  // Your PLC IP
    .port = 502,
    .connect_timeout_ms = 5000,
    .recv_timeout_ms = 1000,
};
```

---

## Error Handling

### Checking Status Codes

```c
mbc_status_t status = mbc_engine_init(&engine, &config);

if (!mbc_status_is_ok(status)) {
    switch (status) {
        case MBC_STATUS_INVALID_ARGUMENT:
            fprintf(stderr, "Invalid argument\n");
            break;
        case MBC_STATUS_NOT_INITIALISED:
            fprintf(stderr, "Not initialized\n");
            break;
        case MBC_STATUS_TIMEOUT:
            fprintf(stderr, "Timeout\n");
            break;
        default:
            fprintf(stderr, "Error code: %d\n", status);
    }
    return 1;
}
```

### Handling Modbus Exceptions

```c
mbc_pdu_t response;
if (mbc_engine_take_pdu(&engine, &response)) {
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
                printf("Unknown (0x%02X)\n", exception_code);
        }
        return;
    }

    // Normal response processing
    // ...
}
```

---

## Next Steps

Now that you have a working client:

1. **Learn the Architecture** – [Architecture Overview](architecture.md)
2. **Explore API** – [API Reference](api/)
3. **Add Error Handling** – [Error Handling Guide](guides/error-handling.md)
4. **Build a Server** – [Server Mode Guide](guides/server-mode.md) *(Phase 6)*
5. **Add RTU Support** – [RTU Guide](guides/rtu-support.md) *(Phase 5)*

---

## Troubleshooting

### Connection Failed

```
✗ Failed to connect to 127.0.0.1:502 (status=-5)
```

**Solutions:**
- Check if server is running: `netstat -an | grep 502`
- Try different port (e.g., 5502 for test server)
- Check firewall settings
- Verify server IP address

### Timeout Waiting for Response

```
✗ Timeout waiting for response (iteration 99)
```

**Solutions:**
- Increase `response_timeout_ms` in engine config
- Check network connectivity
- Verify server is responding (use Wireshark/tcpdump)
- Ensure correct unit ID

### Exception Response

```
✗ Server returned exception: FC=0x03, Code=0x02
```

**Solutions:**
- **0x01 (ILLEGAL FUNCTION)** – Function code not supported by server
- **0x02 (ILLEGAL ADDRESS)** – Register address out of range
- **0x03 (ILLEGAL VALUE)** – Invalid quantity or data value
- **0x04 (SLAVE FAILURE)** – Server internal error

Check your register addresses and quantities!

---

## Full Example Code

See [`tests/example_tcp_client_fc03.c`](https://github.com/lgili/modbuscore/blob/main/tests/example_tcp_client_fc03.c) for a complete, annotated example.
