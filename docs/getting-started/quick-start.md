# Quick Start

Get ModbusCore up and running in 5 minutes!

## Prerequisites

- C17-compatible compiler (GCC 9+, Clang 12+, or MSVC 2019+)
- CMake 3.20 or higher
- Linux, macOS, or Windows

## Installation

### Option 1: System Install (Recommended)

```bash
# Clone the repository
git clone https://github.com/lgili/modbuscore.git
cd modbuscore

# Build and install
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build  # or without sudo on Windows
```

### Option 2: Package Managers

**Conan:**
```bash
conan install --requires=modbuscore/1.0.0
```

**vcpkg:**
```bash
vcpkg install modbuscore
```

**pkg-config:**
```bash
# After system install
pkg-config --modversion modbuscore
```

## Your First Application

### 1. Create a Simple TCP Client

Create `my_first_app.c`:

```c
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>
#include <stdio.h>

int main(void) {
    printf("ModbusCore TCP Client\n");
    
    // Build a Modbus PDU (Function Code 03: Read Holding Registers)
    mbc_pdu_t pdu = {0};
    mbc_status_t status = mbc_pdu_build_read_holding_request(
        &pdu,
        1,    // Unit ID
        0,    // Start address
        10    // Number of registers
    );
    
    if (status != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to build PDU\n");
        return 1;
    }
    
    // Encode as MBAP frame (Modbus TCP)
    uint8_t frame[260];
    size_t frame_len = 0;
    
    mbc_mbap_header_t header = {
        .transaction_id = 1,
        .protocol_id = 0,
        .length = (uint16_t)(pdu.payload_length + 2),  // +2 for unit_id and function
        .unit_id = 1,
    };
    
    uint8_t pdu_bytes[256];
    pdu_bytes[0] = pdu.function;
    memcpy(&pdu_bytes[1], pdu.payload, pdu.payload_length);
    
    status = mbc_mbap_encode(&header, pdu_bytes, pdu.payload_length + 1,
                            frame, sizeof(frame), &frame_len);
    
    if (status == MBC_STATUS_OK) {
        printf("✓ Built Modbus TCP frame (%zu bytes)\n", frame_len);
        printf("✓ Transaction ID: %u\n", header.transaction_id);
        printf("✓ Function Code: 0x%02X\n", pdu.function);
        printf("✓ Ready to send!\n");
    }
    
    return 0;
}
```

### 2. Compile and Run

**Using pkg-config:**
```bash
gcc my_first_app.c $(pkg-config --cflags --libs modbuscore) -o my_first_app
./my_first_app
```

**Using CMake:**

Create `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20)
project(MyModbusApp C)

find_package(ModbusCore REQUIRED)

add_executable(my_first_app my_first_app.c)
target_link_libraries(my_first_app ModbusCore::modbuscore)
```

Build:
```bash
cmake -B build
cmake --build build
./build/my_first_app
```

**Expected Output:**
```
ModbusCore TCP Client
✓ Built Modbus TCP frame (12 bytes)
✓ Transaction ID: 1
✓ Function Code: 0x03
✓ Ready to send!
```

## Next Steps

### Learn by Example

Check out ready-to-run examples:

```bash
cd modbuscore
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build

# Run TCP client example
./build/examples/modbus_tcp_client

# Run RTU server example
./build/examples/modbus_rtu_server
```

### Build a Complete TCP Client

See [First Application Guide](first-application.md) for a complete TCP client with:
- Connection management
- Request/response handling
- Error handling
- Proper cleanup

### Explore Features

- **[Transports](../user-guide/transports.md)** - TCP, RTU, custom
- **[Protocol Engine](../user-guide/protocol-engine.md)** - Client/server modes
- **[Auto-Heal](../user-guide/auto-heal.md)** - Automatic retry
- **[Testing](../user-guide/testing.md)** - Mock transports

## Common Issues

### "ModbusCore not found"

Make sure you installed ModbusCore:
```bash
sudo cmake --install build
```

Or set `CMAKE_PREFIX_PATH`:
```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/install
```

### "undefined reference to mbc_*"

Link against ModbusCore:
```cmake
target_link_libraries(myapp ModbusCore::modbuscore)
```

Or with pkg-config:
```bash
gcc myapp.c $(pkg-config --libs modbuscore) -o myapp
```

### More Help

- [Troubleshooting Guide](../reference/troubleshooting.md)
- [FAQ](../reference/faq.md)
- [GitHub Issues](https://github.com/lgili/modbuscore/issues)
- [Discussions](https://github.com/lgili/modbuscore/discussions)

---

**Next**: [Installation Guide →](installation.md) | [First Application →](first-application.md)
