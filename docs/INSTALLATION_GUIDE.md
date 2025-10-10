# Installation Guide

This guide explains how to install and use ModbusCore in your projects.

## Table of Contents

- [Installation Methods](#installation-methods)
- [CMake Package](#cmake-package)
- [pkg-config](#pkg-config)
- [Amalgamated Build](#amalgamated-build)
- [Examples](#examples)

---

## Installation Methods

### Method 1: System Installation (Recommended)

Build and install the library system-wide:

```bash
# Configure
cmake --preset host-release

# Build
cmake --build --preset host-release

# Install (may require sudo)
sudo cmake --install build/host-release --prefix /usr/local
```

**Installed files:**
- Headers: `/usr/local/include/modbus/`
- Library: `/usr/local/lib/libmodbus.a`
- CMake config: `/usr/local/lib/cmake/modbus/`
- pkg-config: `/usr/local/lib/pkgconfig/modbus.pc`
- Documentation: `/usr/local/share/doc/modbus/`
- Examples: `/usr/local/share/doc/modbus/examples/`

**Uninstall:**
```bash
cd build/host-release
sudo make uninstall  # or sudo ninja uninstall
```

### Method 2: Custom Prefix

Install to a custom location:

```bash
cmake --install build/host-release --prefix /opt/modbus
```

Set environment variables:
```bash
export CMAKE_PREFIX_PATH=/opt/modbus:$CMAKE_PREFIX_PATH
export PKG_CONFIG_PATH=/opt/modbus/lib/pkgconfig:$PKG_CONFIG_PATH
```

### Method 3: Embedded/Cross-Compilation

For embedded targets, build with the appropriate preset:

```bash
# STM32G0 TINY profile
cmake --preset stm32g0-tiny
cmake --build --preset stm32g0-tiny

# Link the library in your embedded project
# Library: build/stm32g0-tiny/modbus/libmodbus.a
```

### Method 4: Amalgamated Build (Drop-in)

Use the amalgamation script to generate single-file source:

```bash
# TINY profile (minimal footprint)
python scripts/amalgamate.py --profile TINY --output modbus_tiny

# LEAN profile (balanced)
python scripts/amalgamate.py --profile LEAN --output modbus_lean

# FULL profile (all features)
python scripts/amalgamate.py --profile FULL --output modbus_full

# Custom configuration
python scripts/amalgamate.py \
    --profile CUSTOM \
    --client --server \
    --rtu --tcp \
    --fc 01 03 06 16 \
    --output modbus_custom
```

Generated files:
- `embedded/quickstarts/drop_in/modbus_<profile>.c`
- `embedded/quickstarts/drop_in/modbus_<profile>.h`

**Usage:**
1. Copy the `.c` and `.h` files to your project
2. Add `modbus_<profile>.c` to your build
3. Include `modbus_<profile>.h` in your code

---

## CMake Package

### Using find_package()

After system installation, use `find_package()` in your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyModbusApp)

# Find the modbus library
find_package(modbus REQUIRED)

# Create your executable
add_executable(my_app main.c)

# Link with modbus
target_link_libraries(my_app PRIVATE modbus::modbus)
```

**Build your project:**
```bash
mkdir build && cd build
cmake ..
make
```

### Version Requirements

Specify minimum version:

```cmake
find_package(modbus 1.0.0 REQUIRED)
```

### Checking Variables

The package provides these variables:

```cmake
find_package(modbus REQUIRED)

message(STATUS "Modbus version: ${MODBUS_VERSION}")
message(STATUS "Modbus include dirs: ${MODBUS_INCLUDE_DIRS}")
message(STATUS "Modbus libraries: ${MODBUS_LIBRARIES}")
```

### Complete Example

```cmake
cmake_minimum_required(VERSION 3.15)
project(ModbusClientDemo VERSION 1.0.0)

# Find modbus library
find_package(modbus 1.0.0 REQUIRED)

# Your application
add_executable(client_demo
    src/main.c
    src/serial_port.c
)

target_link_libraries(client_demo PRIVATE
    modbus::modbus
    pthread  # If needed for your platform
)

target_compile_features(client_demo PRIVATE c_std_11)
```

---

## pkg-config

### Using pkg-config

For Makefile or other build systems:

```bash
# Check if modbus is installed
pkg-config --exists modbus && echo "Modbus found"

# Get compiler flags
pkg-config --cflags modbus

# Get linker flags
pkg-config --libs modbus

# Get version
pkg-config --modversion modbus
```

### Makefile Example

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra $(shell pkg-config --cflags modbus)
LDFLAGS = $(shell pkg-config --libs modbus)

SRCS = main.c serial_port.c
OBJS = $(SRCS:.c=.o)
TARGET = client_demo

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
```

**Build:**
```bash
make
```

### Meson Example

```meson
project('modbus_client', 'c', version: '1.0.0')

modbus_dep = dependency('modbus', version: '>= 1.0.0')

executable('client_demo',
    'src/main.c',
    'src/serial_port.c',
    dependencies: [modbus_dep],
    install: true
)
```

---

## Amalgamated Build

### Profile-Based Generation

**TINY Profile** (~26KB ROM, 8KB RAM):
```bash
python scripts/amalgamate.py --profile TINY --output modbus_tiny
```

**Generated configuration:**
- Client OR Server (not both)
- RTU transport only
- Essential function codes (FC03, FC06, FC16)
- Queue depth: 8
- No diagnostics

**LEAN Profile** (~45KB ROM, 16KB RAM):
```bash
python scripts/amalgamate.py --profile LEAN --output modbus_lean
```

**Generated configuration:**
- Client + Server
- RTU + TCP transports
- Common function codes (FC01-FC06, FC15-FC16)
- Queue depth: 16
- Basic diagnostics

**FULL Profile** (~85KB ROM, 32KB RAM):
```bash
python scripts/amalgamate.py --profile FULL --output modbus_full
```

**Generated configuration:**
- Client + Server
- All transports (RTU, TCP, ASCII)
- All function codes (FC01-FC17)
- Queue depth: 32
- Full diagnostics with trace buffer

### Custom Configuration

Fine-tune your build:

```bash
python scripts/amalgamate.py \
    --profile CUSTOM \
    --client \
    --server \
    --rtu \
    --tcp \
    --fc 01 03 05 06 15 16 \
    --queue-depth 16 \
    --diag-counters \
    --output modbus_custom
```

**Available options:**
- `--profile {TINY,LEAN,FULL,CUSTOM}` - Select base profile
- `--client` - Enable client mode
- `--server` - Enable server mode
- `--rtu` - Enable RTU transport
- `--tcp` - Enable TCP transport
- `--ascii` - Enable ASCII transport
- `--fc FC [FC ...]` - Enable function codes (01-17)
- `--queue-depth N` - Set queue depth (4-128)
- `--diag-counters` - Enable diagnostic counters
- `--diag-trace N` - Enable trace buffer with N entries
- `--output NAME` - Output file basename

### Using Amalgamated Files

1. **Copy files to your project:**
   ```bash
   cp embedded/quickstarts/drop_in/modbus_tiny.c src/
   cp embedded/quickstarts/drop_in/modbus_tiny.h include/
   ```

2. **Add to your build system:**
   
   **CMakeLists.txt:**
   ```cmake
   add_executable(my_app
       src/main.c
       src/modbus_tiny.c
   )
   
   target_include_directories(my_app PRIVATE include)
   ```
   
   **Makefile:**
   ```makefile
   SRCS = main.c modbus_tiny.c
   OBJS = $(SRCS:.c=.o)
   
   my_app: $(OBJS)
       $(CC) $(OBJS) -o my_app
   ```

3. **Include in your code:**
   ```c
   #include "modbus_tiny.h"
   
   int main(void) {
       mb_client_t client;
       // Use the library...
       return 0;
   }
   ```

---

## Examples

### Example 1: RTU Client (System Install + CMake)

**File: main.c**
```c
#include <modbus/client.h>
#include <modbus/rtu.h>
#include <stdio.h>

int main(void) {
    // Transport interface
    mb_rtu_transport_t rtu;
    
    // Client instance
    mb_client_t client;
    
    // Initialize RTU transport
    mb_rtu_init(&rtu, "/dev/ttyUSB0", 9600, MB_PARITY_NONE);
    
    // Initialize client
    mb_client_init(&client, mb_rtu_get_interface(&rtu), NULL, 0);
    
    // Read holding registers
    uint16_t regs[10];
    mb_errno_t err = mb_client_read_holding_registers_sync(
        &client, 1, 0, 10, regs, 1000
    );
    
    if (err == MB_EOK) {
        printf("Read successful: %u values\n", 10);
        for (int i = 0; i < 10; i++) {
            printf("  Register %d: %u\n", i, regs[i]);
        }
    } else {
        printf("Read failed: %d\n", err);
    }
    
    // Cleanup
    mb_client_destroy(&client);
    mb_rtu_destroy(&rtu);
    
    return 0;
}
```

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.15)
project(RTU_Client)

find_package(modbus REQUIRED)

add_executable(rtu_client main.c)
target_link_libraries(rtu_client PRIVATE modbus::modbus)
```

**Build:**
```bash
mkdir build && cd build
cmake ..
make
./rtu_client
```

### Example 2: TCP Server (pkg-config + Makefile)

**File: server.c**
```c
#include <modbus/server.h>
#include <modbus/tcp.h>
#include <stdio.h>

// Server callbacks
static mb_errno_t read_holding_regs(
    void *priv, uint16_t addr, uint16_t count, uint16_t *out
) {
    // Simulate register data
    for (uint16_t i = 0; i < count; i++) {
        out[i] = addr + i + 1000;
    }
    return MB_EOK;
}

int main(void) {
    mb_tcp_transport_t tcp;
    mb_server_t server;
    mb_server_callbacks_t callbacks = {0};
    
    // Set callback
    callbacks.read_holding_registers = read_holding_regs;
    
    // Initialize TCP transport
    mb_tcp_server_init(&tcp, "0.0.0.0", 502);
    
    // Initialize server
    mb_server_init(&server, mb_tcp_get_interface(&tcp), &callbacks, NULL);
    
    printf("Modbus TCP server listening on port 502...\n");
    
    // Run server
    while (1) {
        mb_server_poll(&server, 100);
    }
    
    return 0;
}
```

**Makefile:**
```makefile
CC = gcc
CFLAGS = -std=c11 -Wall $(shell pkg-config --cflags modbus)
LDFLAGS = $(shell pkg-config --libs modbus)

tcp_server: server.c
	$(CC) $(CFLAGS) server.c $(LDFLAGS) -o tcp_server

clean:
	rm -f tcp_server

.PHONY: clean
```

**Build and run:**
```bash
make
sudo ./tcp_server  # sudo needed for port 502
```

### Example 3: Embedded STM32 (Amalgamated)

**Generate amalgamation:**
```bash
python scripts/amalgamate.py \
    --profile LEAN \
    --client \
    --rtu \
    --fc 03 06 16 \
    --output modbus_stm32
```

**File: main.c (STM32)**
```c
#include "modbus_stm32.h"
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart2;

// RTU transport with HAL
static ssize_t uart_write(void *priv, const void *buf, size_t len) {
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, len, 1000);
    return len;
}

static ssize_t uart_read(void *priv, void *buf, size_t len) {
    if (HAL_UART_Receive(&huart2, buf, len, 100) == HAL_OK)
        return len;
    return 0;
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_UART2_Init();
    
    // Setup Modbus RTU client
    mb_rtu_transport_t rtu;
    mb_client_t client;
    
    mb_rtu_init(&rtu, uart_write, uart_read, NULL);
    mb_client_init(&client, mb_rtu_get_interface(&rtu), NULL, 0);
    
    while (1) {
        // Read sensor data via Modbus
        uint16_t sensor_value;
        if (mb_client_read_holding_registers_sync(
            &client, 1, 0, 1, &sensor_value, 1000
        ) == MB_EOK) {
            // Process sensor value
        }
        
        HAL_Delay(1000);
    }
}
```

---

## Verification

### Verify Installation

Check that the library is properly installed:

```bash
# Check library file
ls /usr/local/lib/libmodbus.a

# Check headers
ls /usr/local/include/modbus/

# Check CMake config
ls /usr/local/lib/cmake/modbus/

# Check pkg-config
pkg-config --exists modbus && echo "OK" || echo "Not found"

# Check version
pkg-config --modversion modbus
```

### Test Installation

Quick test with CMake:

```bash
cat > test.c << 'EOF'
#include <modbus/client.h>
#include <stdio.h>

int main(void) {
    printf("Modbus library version: %d.%d.%d\n",
        MB_VERSION_MAJOR, MB_VERSION_MINOR, MB_VERSION_PATCH);
    return 0;
}
EOF

cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.15)
project(test)
find_package(modbus REQUIRED)
add_executable(test test.c)
target_link_libraries(test PRIVATE modbus::modbus)
EOF

mkdir build && cd build
cmake .. && make
./test
```

Expected output:
```
Modbus library version: 1.0.0
```

---

## Troubleshooting

### Library Not Found

**Problem:** `find_package(modbus)` fails or pkg-config doesn't find modbus.

**Solutions:**
1. Check installation path:
   ```bash
   find /usr -name "modbus*.pc" 2>/dev/null
   find /usr -name "modbusConfig.cmake" 2>/dev/null
   ```

2. Set `CMAKE_PREFIX_PATH`:
   ```bash
   export CMAKE_PREFIX_PATH=/usr/local:$CMAKE_PREFIX_PATH
   ```

3. Set `PKG_CONFIG_PATH`:
   ```bash
   export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
   ```

### Header Not Found

**Problem:** Compiler can't find `modbus/client.h`.

**Solutions:**
1. Check include path:
   ```bash
   ls /usr/local/include/modbus/
   ```

2. Add include directory manually:
   ```cmake
   target_include_directories(my_app PRIVATE /usr/local/include)
   ```

### Linker Errors

**Problem:** Undefined references during linking.

**Solutions:**
1. Check library exists:
   ```bash
   ls /usr/local/lib/libmodbus.a
   ```

2. Link explicitly:
   ```cmake
   target_link_libraries(my_app PRIVATE /usr/local/lib/libmodbus.a)
   ```

3. Check for missing dependencies (pthread, etc.):
   ```cmake
   find_package(Threads REQUIRED)
   target_link_libraries(my_app PRIVATE modbus::modbus Threads::Threads)
   ```

### Amalgamation Script Fails

**Problem:** `amalgamate.py` exits with error.

**Solutions:**
1. Check Python version (requires 3.6+):
   ```bash
   python --version
   ```

2. Check file permissions:
   ```bash
   chmod +x scripts/amalgamate.py
   ```

3. Run with verbose output:
   ```bash
   python scripts/amalgamate.py --profile TINY --output test -v
   ```

---

## See Also

- [CMAKE_PRESETS_GUIDE.md](CMAKE_PRESETS_GUIDE.md) - CMake preset usage
- [KCONFIG_GUIDE.md](KCONFIG_GUIDE.md) - Kconfig configuration
- [FOOTPRINT.md](FOOTPRINT.md) - Memory usage analysis
- [Examples directory](../examples/) - More usage examples
