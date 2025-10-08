# Modbus C Library

[![CI](https://github.com/lgili/modbus/actions/workflows/ci.yml/badge.svg)](https://github.com/lgili/modbus/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/website?label=docs&url=https%3A%2F%2Flgili.github.io%2Fmodbus%2Fdocs%2F)](https://lgili.github.io/modbus/docs/)
[![Latest release](https://img.shields.io/github/v/release/lgili/modbus?display_name=tag)](https://github.com/lgili/modbus/releases/latest)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Embedded-blue)](embedded/quickstarts)

> **Production-ready Modbus** – Modern C library for RTU, TCP, and ASCII. From bare-metal MCUs to desktop applications.

---

## Quick Start

### Desktop (Linux/macOS/Windows)

```c
#include <modbus/mb_host.h>
#include <stdio.h>

int main(void) {
    // Connect to TCP slave
    mb_host_client_t *client = mb_host_tcp_connect("192.168.1.10:502");
    if (!client) return 1;

    // Read holding register
    uint16_t value;
    mb_err_t err = mb_host_read_holding(client, 1, 0, 1, &value);
    printf("Register 0: %u (%s)\n", value, mb_host_error_string(err));

    mb_host_disconnect(client);
    return (err == MB_OK) ? 0 : 1;
}
```

**Build:**
```bash
gcc your_code.c -lmodbus -o modbus_app
./modbus_app
```

### Embedded (STM32/ESP32/nRF/etc)

See platform-specific quickstarts: [embedded/quickstarts](embedded/quickstarts)

---

## Why This Library?

✅ **Production-Ready** – Extensively tested, used in industrial applications  
✅ **Zero Dependencies** – Pure C11, no external libs, runs on any platform  
✅ **Heap-Free** – Perfect for embedded systems with strict memory constraints  
✅ **High Performance** – Zero-copy I/O, lock-free queues, <100µs turnaround  
✅ **Battery-Optimized** – Power management for IoT and portable devices  
✅ **Well-Documented** – Comprehensive API docs, examples, and guides  

---

## Key Features

### 🎯 Protocol Support
- **RTU** – RS-485/RS-232 with auto baud-rate
- **TCP** – Single and multi-connection servers
- **ASCII** – Full implementation (rarely used but available)
- **12 Function Codes** – 01, 02, 03, 04, 05, 06, 07, 0F, 10, 11, 16, 17

### ⚡ Performance
- **Zero-Copy I/O** – 47% memory savings, 33% CPU reduction ([docs](docs/zero_copy_io.md))
- **Lock-Free Queues** – True SPSC, 16ns latency, ISR-safe ([docs](docs/queue_and_pool.md))
- **ISR-Safe Mode** – <100µs RX→TX turnaround for half-duplex ([docs](docs/isr_safe_mode.md))
- **QoS Backpressure** – Priority queues prevent head-of-line blocking ([docs](docs/qos_backpressure.md))

### 🔋 Power Management
- **Idle Callbacks** – Automatic sleep when protocol is idle
- **Multi-Level Sleep** – WFI → Sleep → STOP mode based on duration
- **Platform Agnostic** – Works with FreeRTOS, Zephyr, bare-metal
- **Battery-Optimized** – 70-95% power reduction in typical scenarios

**Example:**
```c
#include <modbus/mb_power.h>

uint32_t my_idle_callback(void *ctx, uint32_t sleep_ms) {
    if (sleep_ms > 20) {
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        SystemClock_Config();
        return sleep_ms;
    }
    __WFI();
    return 1;
}

mb_client_set_idle_callback(&client, my_idle_callback, NULL, 5);
```

👉 **Full guide:** [docs/power_management.md](docs/power_management.md)

### 📊 Diagnostics & Monitoring
- **Function Code Counters** – Track every FC execution
- **Error Histograms** – Timeouts, CRC errors, exceptions
- **Circular Trace Buffer** – Last N events with timestamps
- **Zero Overhead** – <0.5% CPU, compile-time configurable

👉 **Learn more:** [docs/diagnostics.md](docs/diagnostics.md)

### 🛡️ Reliability
- **Retry & Backoff** – Configurable retry logic with exponential backoff
- **Watchdog Timers** – Per-transaction and global timeouts
- **CRC Validation** – Hardware-accelerated on supported platforms
- **Transaction Pool** – O(1) acquire/release, zero memory leaks

---

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| **Linux** | ✅ Full | POSIX sockets, pthreads |
| **macOS** | ✅ Full | POSIX sockets, dispatch |
| **Windows** | ✅ Full | Winsock2, MSVC support |
| **STM32** | ✅ Full | HAL, LL, bare-metal |
| **ESP32** | ✅ Full | ESP-IDF, Arduino |
| **nRF** | ✅ Full | Zephyr, nRF SDK |
| **FreeRTOS** | ✅ Full | All MCU families |
| **Zephyr** | ✅ Full | All supported boards |
| **Bare Metal** | ✅ Full | Any Cortex-M, RISC-V, AVR |

See [embedded/WHICH_QUICKSTART.md](embedded/WHICH_QUICKSTART.md) for platform selection guide.

---

## Installation

### CMake (Recommended)

```cmake
# CMakeLists.txt
include(FetchContent)

FetchContent_Declare(
    modbus
    GIT_REPOSITORY https://github.com/lgili/modbus.git
    GIT_TAG        v1.0.0  # Or latest release
)
FetchContent_MakeAvailable(modbus)

target_link_libraries(your_app PRIVATE modbus)
```

### Manual Integration

1. Copy `modbus/include/` and `modbus/src/` to your project
2. Add `modbus/include` to your include path
3. Compile all `.c` files in `modbus/src/`
4. Link the resulting library

See [embedded/quickstarts/drop_in](embedded/quickstarts/drop_in) for minimal integration example.

---

## Examples

| Example | Description | Platform |
|---------|-------------|----------|
| [level1_hello](examples/level1_hello) | Basic read/write operations | All |
| [tcp_client_cli](examples/tcp_client_cli.c) | Interactive TCP client | Linux/macOS/Windows |
| [tcp_server_demo](examples/tcp_server_demo.c) | Multi-connection server | All |
| [rtu_serial_client](examples/rtu_serial_client.c) | RS-485 client | All |
| [rtu_serial_server](examples/rtu_serial_server.c) | RS-485 server | All |
| [power_idle_demo](examples/power_idle_demo.c) | Power management | Embedded |
| [freertos_sim](examples/freertos_sim.c) | FreeRTOS integration | FreeRTOS |
| [bare_sim](examples/bare_sim.c) | Bare-metal example | Bare Metal |

More examples in [examples/](examples/) directory.

---

## Documentation

### User Guides
- [Power Management](docs/power_management.md) – Low-power modes and idle callbacks
- [ISR-Safe Mode](docs/isr_safe_mode.md) – Ultra-low latency for embedded
- [Zero-Copy I/O](docs/zero_copy_io.md) – Efficient memory usage
- [QoS & Backpressure](docs/qos_backpressure.md) – Priority-based flow control
- [Diagnostics](docs/diagnostics.md) – On-device debugging
- [Troubleshooting](docs/TROUBLESHOOTING.md) – Common issues and solutions

### API Reference
- [Client API](modbus/include/modbus/client.h) – Non-blocking client
- [Server API](modbus/include/modbus/server.h) – Server implementation
- [Host API](modbus/include/modbus/mb_host.h) – Blocking convenience functions
- [Power API](modbus/include/modbus/mb_power.h) – Power management

### Integration Guides
- [Embedded Quickstarts](embedded/quickstarts/) – Platform-specific setup
- [Porting Guide](embedded/porting-wizard.md) – New platform integration
- [Platform Checklist](embedded/checklists/) – Validation checklists

---

## Configuration

All configuration is done via `modbus/include/modbus/conf.h`. Key options:

```c
// Transport Selection
#define MB_CONF_TRANSPORT_RTU 1    // Enable RTU
#define MB_CONF_TRANSPORT_TCP 1    // Enable TCP
#define MB_CONF_TRANSPORT_ASCII 0  // Disable ASCII (rarely used)

// Client/Server Selection
#define MB_CONF_BUILD_CLIENT 1     // Enable client
#define MB_CONF_BUILD_SERVER 1     // Enable server

// Memory Pools
#define MB_CONF_CLIENT_MAX_TXN 8   // Max concurrent transactions
#define MB_CONF_PDU_MAX_SIZE 253   // Max PDU size (spec = 253)

// Features (optional, can be disabled to save memory)
#define MB_CONF_ENABLE_POWER_MANAGEMENT 1  // Power management (~64 bytes)
#define MB_CONF_DIAG_ENABLE_COUNTERS 1     // Diagnostics (~2.2 KB)
#define MB_CONF_DIAG_ENABLE_TRACE 1        // Trace buffer (~1 KB)

// Performance Tuning
#define MB_CONF_CLIENT_POLL_BUDGET_STEPS 8   // FSM steps per poll()
#define MB_CONF_SERVER_POLL_BUDGET_STEPS 16  // Server FSM budget
```

Override these in your project by defining them **before** including modbus headers.

---

## Building from Source

### Requirements
- CMake 3.15+
- C11 compiler (GCC, Clang, MSVC)
- Optional: Doxygen (for docs)

### Build Steps

```bash
# Clone repository
git clone https://github.com/lgili/modbus.git
cd modbus

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests (optional)
cd build && ctest
```

### Build Options

```bash
# Minimal build (client only, no TCP)
cmake -B build -DMODBUS_ENABLE_SERVER=OFF -DMODBUS_ENABLE_TRANSPORT_TCP=OFF

# Debug build with sanitizers
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DMODBUS_ENABLE_ASAN=ON

# Cross-compile for ARM
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake

# Generate documentation
cmake -B build -DMODBUS_BUILD_DOCS=ON
cmake --build build --target docs
```

---

## Testing

### Unit Tests (25 test suites)
```bash
cd build && ctest --output-on-failure
```

### Interoperability Tests
Test against pymodbus, libmodbus, and other implementations:
```bash
docker build -t modbus-interop -f interop/Dockerfile .
docker run --rm modbus-interop
```

See [interop/README.md](interop/README.md) for details.

### Fuzzing
```bash
cmake -B build-fuzz -DMODBUS_BUILD_FUZZERS=ON
cmake --build build-fuzz
./build-fuzz/fuzz/modbus_fuzzer corpus/
```

---

## Performance Benchmarks

**Tested on STM32F407 @ 168 MHz:**

| Operation | Latency | Throughput |
|-----------|---------|------------|
| FC03 Read (10 regs) | 1.2 ms | 830 tx/s |
| FC16 Write (10 regs) | 1.5 ms | 666 tx/s |
| Queue enqueue | 16 ns | - |
| Queue dequeue | 18 ns | - |
| CRC16 (256 bytes) | 45 µs | 5.7 MB/s |
| RX→TX turnaround | 89 µs | - |

**Memory footprint (typical client, RTU only):**
- Code: ~18 KB
- RAM: ~2.5 KB (with 8 transaction pool)
- Stack: ~1 KB peak

---

## Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Follow existing code style
4. Add tests for new features
5. Run `cmake --build build --target format` (if clang-format available)
6. Submit a pull request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

---

## License

MIT License. See [LICENSE](LICENSE) file for details.

---

## Support

- **Documentation**: [lgili.github.io/modbus/docs](https://lgili.github.io/modbus/docs/)
- **Issues**: [GitHub Issues](https://github.com/lgili/modbus/issues)
- **Discussions**: [GitHub Discussions](https://github.com/lgili/modbus/discussions)
- **Examples**: [examples/](examples/) directory

---

## Acknowledgments

This library builds upon modern embedded systems best practices and incorporates feedback from industrial deployments. Special thanks to all contributors and users who have helped improve the library.

---

## Roadmap

- [x] RTU, TCP, ASCII transport support
- [x] Zero-copy I/O primitives
- [x] Lock-free queues for ISR safety
- [x] QoS and backpressure handling
- [x] Power management for battery devices
- [x] Comprehensive diagnostics
- [x] Multi-vendor interop testing
- [ ] Modbus Security (planned)
- [ ] Modbus/TCP encryption (TLS)
- [ ] Additional function codes (FC18, FC20, FC21)
- [ ] Python bindings

---

**Made with ❤️ for embedded systems developers**
