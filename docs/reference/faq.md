# Frequently Asked Questions (FAQ)

Common questions about ModbusCore.

## General Questions

### What is ModbusCore?

ModbusCore is a lightweight, portable C17 library for Modbus TCP and RTU protocols. It's designed for embedded systems and industrial applications with zero dependencies and static memory allocation.

### Why ModbusCore instead of libmodbus?

| Feature | ModbusCore | libmodbus |
|---------|-----------|-----------|
| **Dependencies** | None | glib, pthread |
| **Memory** | Static only | Dynamic (malloc) |
| **Platforms** | POSIX, Windows, RTOS | POSIX, Windows only |
| **Size** | ~50KB | ~200KB |
| **C Standard** | C17 | C99 |
| **Thread Safety** | User-controlled | Built-in mutexes |

**Choose ModbusCore if you need:**
- ✅ Embedded/RTOS support (FreeRTOS, Zephyr)
- ✅ Zero dependencies
- ✅ Deterministic behavior
- ✅ Safety-critical applications

**Choose libmodbus if you prefer:**
- ✅ Higher-level API
- ✅ Built-in threading
- ✅ Mature ecosystem

### Is ModbusCore production-ready?

**Yes!** ModbusCore v1.0+ is stable and tested:
- ✅ Full Modbus spec compliance
- ✅ Extensive test coverage (>95%)
- ✅ Fuzzing-tested (LibFuzzer)
- ✅ Production use in industrial systems

### What licenses does ModbusCore use?

**MIT License** - permissive, commercial-friendly. You can use ModbusCore in:
- ✅ Open source projects
- ✅ Commercial products
- ✅ Proprietary software
- ✅ Closed-source applications

No royalties, no attribution required (but appreciated!).

---

## Installation & Setup

### How do I install ModbusCore?

**Option 1: System install (recommended)**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

**Option 2: Package managers**
```bash
# Conan
conan install modbuscore/1.0.0

# vcpkg
vcpkg install modbuscore
```

See [Installation Guide](../getting-started/installation.md) for details.

### CMake can't find ModbusCore

Set `CMAKE_PREFIX_PATH`:
```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/modbuscore/install
```

Or use environment variable:
```bash
export CMAKE_PREFIX_PATH=/usr/local
cmake -B build
```

### pkg-config doesn't work

Ensure `PKG_CONFIG_PATH` includes the install location:
```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
pkg-config --modversion modbuscore
```

**Note:** pkg-config has issues with paths containing spaces. Use CMake instead.

---

## Usage

### How do I build a simple Modbus TCP client?

```c
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

int main(void) {
    // 1. Build PDU
    mbc_pdu_t pdu = {0};
    mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    // 2. Encode MBAP frame
    mbc_mbap_header_t hdr = {.transaction_id = 1, .unit_id = 1, 
                              .length = pdu.data_length + 1};
    uint8_t buffer[260];
    size_t length;
    mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &length);
    
    // 3. Send via TCP (your code)
    send(sock, buffer, length, 0);
    
    return 0;
}
```

See [First Application](../getting-started/first-application.md) for complete examples.

### How do I parse a Modbus response?

```c
// Receive frame
uint8_t rx_buffer[260];
ssize_t received = recv(sock, rx_buffer, sizeof(rx_buffer), 0);

// Decode MBAP
mbc_mbap_header_t resp_hdr;
mbc_pdu_t resp_pdu;
if (mbc_mbap_decode(rx_buffer, received, &resp_hdr, &resp_pdu) == MBC_STATUS_OK) {
    // Check for exception
    if (resp_pdu.function & 0x80) {
        printf("Exception: 0x%02X\n", resp_pdu.data[0]);
    } else {
        // Parse data
        uint8_t byte_count = resp_pdu.data[0];
        uint16_t *registers = (uint16_t*)&resp_pdu.data[1];
        // Use registers...
    }
}
```

### How do I implement RTU on serial port?

```c
#include <modbuscore/protocol/rtu.h>
#include <termios.h>
#include <fcntl.h>

// Open serial port
int fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);

// Configure (9600 8N1)
struct termios tty = {0};
tcgetattr(fd, &tty);
cfsetospeed(&tty, B9600);
cfsetispeed(&tty, B9600);
tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
tty.c_cflag |= (CLOCAL | CREAD);
tty.c_cflag &= ~PARENB;
tcsetattr(fd, TCSANOW, &tty);

// Build and encode RTU frame
mbc_pdu_t pdu = {0};
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

uint8_t buffer[256];
size_t length;
mbc_rtu_encode(1, &pdu, buffer, sizeof(buffer), &length);

// Send
write(fd, buffer, length);
```

See [RTU Example](../getting-started/first-application.md#modbus-rtu-client) for details.

### What Modbus functions are supported?

| Code | Function | Read/Write | Supported |
|------|----------|-----------|-----------|
| 0x01 | Read Coils | Read | ✅ |
| 0x02 | Read Discrete Inputs | Read | ✅ |
| 0x03 | Read Holding Registers | Read | ✅ |
| 0x04 | Read Input Registers | Read | ✅ |
| 0x05 | Write Single Coil | Write | ✅ |
| 0x06 | Write Single Register | Write | ✅ |
| 0x0F | Write Multiple Coils | Write | ✅ |
| 0x10 | Write Multiple Registers | Write | ✅ |
| 0x17 | Read/Write Multiple | Both | ⚠️ Future |

### Can I use ModbusCore on ESP32/Arduino?

**ESP32: Yes!** See [ESP32 Example](../getting-started/examples.md#esp32-rtu-client)

**Arduino: Partial** - Arduino has limited C17 support. Use ESP-IDF for ESP32 boards instead.

**FreeRTOS: Yes!** See [FreeRTOS Example](../getting-started/examples.md#freertos-client)

**Zephyr: Yes!** See [Zephyr Example](../getting-started/examples.md#zephyr-rtu-client)

---

## Architecture

### Is ModbusCore thread-safe?

**No, by design.** You must synchronize access:

**Option 1: Separate instances per thread (recommended)**
```c
// Thread 1
mbc_runtime_config_t runtime1 = {0};

// Thread 2
mbc_runtime_config_t runtime2 = {0};
```

**Option 2: Mutex protection**
```c
pthread_mutex_t lock;

void thread_safe_call(void) {
    pthread_mutex_lock(&lock);
    mbc_pdu_build_read_holding_request(...);
    pthread_mutex_unlock(&lock);
}
```

See [Thread Safety](../user-guide/architecture.md#thread-safety) for details.

### Does ModbusCore use malloc/free?

**No!** All memory is statically allocated:
- ✅ No heap fragmentation
- ✅ Deterministic behavior
- ✅ Suitable for safety-critical systems

### What's the memory footprint?

**Code size:** ~50KB (typical, with -Os)
**RAM (per instance):**
- PDU: 256 bytes
- MBAP header: 8 bytes
- Runtime (optional): ~1KB

See [Footprint Guide](../docs/FOOTPRINT.md) for detailed analysis.

### Can I use custom transports (UDP, CAN, etc.)?

**Yes!** Implement the transport interface:

```c
typedef struct {
    mbc_status_t (*send)(void *ctx, const uint8_t *data, size_t len);
    mbc_status_t (*receive)(void *ctx, uint8_t *buf, size_t max, size_t *rcvd);
    void *user_context;
} mbc_custom_transport_t;
```

See [Custom Transports](../advanced/custom-transports.md) for examples.

---

## Troubleshooting

### CRC errors on RTU

**Check baud rate:**
```bash
stty -F /dev/ttyUSB0
```

**Verify parity/stop bits:**
```c
// Must match device settings
tty.c_cflag &= ~PARENB;   // No parity
tty.c_cflag &= ~CSTOPB;   // 1 stop bit
```

**Check inter-frame delay:**
```c
// Must wait 3.5 character times between frames
usleep(4000);  // ~3.6ms at 9600 baud
```

See [Troubleshooting Guide](troubleshooting.md) for more.

### TCP connection refused

**Check server is running:**
```bash
netstat -an | grep 502
```

**Check firewall:**
```bash
sudo ufw allow 502/tcp
```

**Try localhost first:**
```c
mbc_tcp_connect(&tcp, "127.0.0.1", 502);
```

### Timeout errors

**Increase timeout:**
```c
struct timeval tv = {.tv_sec = 10};  // 10 seconds
setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
```

**Check network latency:**
```bash
ping <device_ip>
```

### Exception responses

Common exception codes:

| Code | Meaning | Fix |
|------|---------|-----|
| 0x01 | Illegal function | Use supported function code |
| 0x02 | Illegal data address | Check register address range |
| 0x03 | Illegal data value | Validate write value |
| 0x04 | Server device failure | Check device status |

---

## Performance

### How fast is ModbusCore?

**TCP:** ~1000 transactions/sec (local network)
**RTU (9600 baud):** ~10 transactions/sec
**RTU (115200 baud):** ~100 transactions/sec

Limited by network/serial speed, not library.

### Can I improve performance?

**Use higher baud rates:**
```c
// RTU: Use 115200 instead of 9600
cfsetospeed(&tty, B115200);
```

**Batch writes:**
```c
// ❌ Slow: 10 individual writes
for (int i = 0; i < 10; i++) {
    mbc_pdu_build_write_single_request(&pdu, 1, i, values[i]);
    // send...
}

// ✅ Fast: 1 multi-write
mbc_pdu_build_write_multiple_request(&pdu, 1, 0, values, 10);
// send once
```

**Use TCP instead of RTU** for high-speed applications.

---

## Advanced Topics

### How do I enable auto-heal?

Build with auto-heal support:
```bash
cmake -B build -DENABLE_AUTO_HEAL=ON
```

Then configure:
```c
#include <modbuscore/auto_heal.h>

mbc_auto_heal_config_t cfg = {
    .enable_frame_repair = true,
    .max_retries = 3,
    .retry_delay_ms = 100
};
mbc_auto_heal_init(&cfg);
```

See [Auto-Heal Guide](../user-guide/auto-heal.md) for details.

### How do I enable diagnostics?

```c
#include <modbuscore/diagnostics.h>

mbc_diag_config_t cfg = {
    .enable_frame_tracing = true,
    .enable_error_logging = true,
    .trace_callback = my_trace_handler
};
mbc_diagnostics_init(&cfg);
```

See [Diagnostics Guide](../user-guide/diagnostics.md) for details.

### Can I use ModbusCore in safety-critical systems?

**Partially.** ModbusCore is designed for determinism and reliability:
- ✅ Static memory allocation
- ✅ No undefined behavior
- ✅ Extensive testing (unit, integration, fuzzing)

**But:** Not certified for safety standards (IEC 61508, etc.). Contact us for certification support.

---

## Getting Help

### Where do I report bugs?

[GitHub Issues](https://github.com/lgili/modbuscore/issues)

**Include:**
- ModbusCore version
- Platform (OS, architecture)
- Minimal reproducible example
- Expected vs actual behavior

### Where can I ask questions?

- **GitHub Discussions**: [Discussions](https://github.com/lgili/modbuscore/discussions)
- **Stack Overflow**: Tag `modbuscore`
- **Email**: modbuscore@example.com

### How can I contribute?

See [Contributing Guide](../contributing/README.md)

---

## License & Support

### Is commercial use allowed?

**Yes!** MIT License permits:
- ✅ Commercial use
- ✅ Modification
- ✅ Distribution
- ✅ Private use

No attribution required (but appreciated).

### Is professional support available?

**Community support** is free via GitHub Issues/Discussions.

**Commercial support** (consulting, custom features, priority bug fixes) available on request. Contact: modbuscore@example.com

---

**Still have questions?** [Open a discussion](https://github.com/lgili/modbuscore/discussions) or [file an issue](https://github.com/lgili/modbuscore/issues).
